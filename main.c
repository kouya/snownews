// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
//
// Snownews is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// Snownews is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Snownews. If not, see http://www.gnu.org/licenses/.

#include "main.h"
#include "interface.h"
#include "io-internal.h"
#include "setup.h"
#include "ui-support.h"
#include <ncurses.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

//----------------------------------------------------------------------
// Global variables

struct feed* _feed_list = NULL;
struct feed* _unfiltered_feed_list = NULL;	// Backup first pointer for filter mode.
				// Needs to be global so it can be used in the signal handler.
				// Must be set to NULL by default and whenever it's not used anymore!
bool _feed_list_changed = false;

struct settings _settings = {
	.keybindings = {
		// Define default values for keybindings. If some are defined differently
		// in the keybindings file they will be overwritten. If some are missing or broken/wrong
		// these are sane defaults.
		.about		= 'A',
		.addfeed	= 'a',
		.andxor		= 'X',
		.categorize	= 'C',
		.changefeedname	= 'c',
		.deletefeed	= 'D',
		.dfltbrowser	= 'B',
		.end		= '>',
		.enter		= 'l',
		.feedinfo	= 'i',
		.filter		= 'f',
		.filtercurrent	= 'g',
		.forcereload	= 'T',
		.help		= 'h',
		.home		= '<',
		.markallread	= 'm',
		.markread	= 'm',
		.markunread	= 'M',
		.movedown	= 'N',
		.moveup		= 'P',
		.newheadlines	= 'H',
		.next		= 'n',
		.nofilter	= 'F',
		.pdown		= ' ',
		.perfeedfilter	= 'e',
		.prev		= 'p',
		.prevmenu	= 'q',
		.pup		= 'b',
		.quit		= 'q',
		.reload		= 'r',
		.reloadall	= 'R',
		.sortfeeds	= 's',
		.typeahead	= '/',
		.urljump	= 'o',
		.urljump2	= 'O'
	},
	.color = {
		.feedtitle	= -1,
		.feedtitlebold	= 0,
		.newitems	= 5,
		.newitemsbold	= 0,
		.urljump	= 4,
		.urljumpbold	= 0
	},
};

//----------------------------------------------------------------------

static int last_signal = 0;

//----------------------------------------------------------------------

enum EPIDAction { pid_file_delete, pid_file_create };

static void make_pidfile_name (char* fnbuf, size_t fnbufsz)
{
	const char* rundir = getenv("XDG_RUNTIME_DIR");
	if (rundir)
		snprintf (fnbuf, fnbufsz, "%s/snownews.pid", rundir);
	else
		snprintf (fnbuf, fnbufsz, "%s/.snownews/pid", getenv("HOME"));
}

static void modifyPIDFile (enum EPIDAction action) {
	char pid_path [PATH_MAX];
	make_pidfile_name (pid_path, sizeof(pid_path));
	if (action == pid_file_create) {
		FILE* file = fopen(pid_path, "w");
		if (!file) {
			printf("Unable to write PID file %s!", pid_path);
			return;
		}
		fprintf(file, "%d", getpid());
		fclose(file);
	} else
		unlink(pid_path);
}

static void checkPIDFile (void) {
	char pid_path [PATH_MAX];
	make_pidfile_name (pid_path, sizeof(pid_path));

	FILE* pidfile = fopen(pid_path, "r");
	if (!pidfile)
		return;
	char pidbuf[12];
	fgets (pidbuf, sizeof(pidbuf), pidfile);
	fclose (pidfile);
	pid_t pid = atoi(pidbuf);

	if (kill(pid, 0) == 0) {
		printf("Snownews seems to be already running with process ID %d.\n", pid);
		printf("A pid file exists at \"%s\".\n", pid_path);
		exit(2);
	} else {
		printf("A pid file exists at \"%s\",\nbut Snownews doesn't seem to be running. Delete that file and start it again.\n", pid_path);
		printf("Continue anyway? (y/n) ");
		char ybuf[2] = {};
		fgets (ybuf, sizeof(ybuf), stdin);
		if (ybuf[0] != 'y')
			exit(2);
		modifyPIDFile (pid_file_delete);
	}
}

// Deinit ncurses and quit.
_Noreturn void MainQuit (const char* func, const char* error) {
	if (!error)	// Only save settings if we didn't exit on error.
		WriteCache();
	endwin();	// Make sure no ncurses function is called after this point!
	modifyPIDFile(pid_file_delete);

	if (last_signal)
		printf ("Exiting via signal %d.\n", last_signal);
	if (error) {
		printf (_("Aborting program execution!\nAn internal error occured. Snownews has quit, no changes has been saved!\n"));
		printf (_("Please submit a bugreport at https://github.com/kouya/snownews/issues\n"));
		printf ("----\n");
		// Please don't localize! I don't want to receive Japanese error messages.
		// Thanks. :)
		printf ("While executing: %s\n", func);
		printf ("Error as reported by the system: %s\n\n", error);
		exit (EXIT_FAILURE);
	}
	exit (EXIT_SUCCESS);
}

// Signal handler function.
static _Noreturn void MainSignalHandler (int sig) {
	last_signal = sig;

	// If there is a _unfiltered_feed_list!=NULL a filter is set. Reset _feed_list
	// so the correct list gets written on the disk when exisiting via SIGINT.
	if (_unfiltered_feed_list)
		_feed_list = _unfiltered_feed_list;
	MainQuit (NULL, "Signal");
}

// Automatic child reaper.
static void sigChildHandler (int sig __attribute__((unused))) {
	// Wait for any child without blocking
	waitpid (-1, NULL, WNOHANG);
}

static void printHelp (void) {
	printf (_("Snownews %s\n\n"), SNOWNEWS_VERSTRING);
	printf (_("usage: snownews [-huV] [--help|--update|--version]\n\n"));
	printf (_("\t--charset|-l\tForce using this charset.\n"));
	printf (_("\t--cursor-on|-c\tForce cursor always visible.\n"));
	printf (_("\t--help|-h\tPrint this help message.\n"));
	printf (_("\t--update|-u\tAutomatically update every feed.\n"));
	printf (_("\t--version|-V\tPrint version number and exit.\n"));
}

static void badOption (const char * arg) {
	printf (_("Option %s requires an argument.\n"), arg);
}

int main (int argc, char *argv[]) {
	#ifdef LOCALEPATH
		setlocale (LC_ALL, "");
		bindtextdomain ("snownews", LOCALEPATH);
		textdomain ("snownews");
	#endif
	srand (time(0));	// Init the pRNG. See about.c for usages of rand() ;)

	bool autoupdate = false;	// Automatically update feeds on app start... or not if set to 0.
	for (int i = 1; i < argc; ++i) {
		char* arg = argv[i];
		if (strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0) {
			printf (_("Snownews version %s\n\n"), SNOWNEWS_VERSION);
			return EXIT_SUCCESS;
		} else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--update") == 0) {
			autoupdate = true;
		} else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--cursor-on") == 0) {
			_settings.cursor_always_visible = true;
		} else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--charset") == 0) {
			const char* chset = argv[i++];
			if (chset)
				_settings.global_charset = chset;
			else {
				badOption(arg);
				return EXIT_FAILURE;
			}
		} else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			printHelp();
			return EXIT_SUCCESS;
		} else {
			printf ("Unkown option given: \"%s\".\n", arg);
			printHelp();
			return EXIT_SUCCESS;
		}
	}

	// Create PID file.
	checkPIDFile();
	modifyPIDFile (pid_file_create);

	signal (SIGHUP, MainSignalHandler);
	signal (SIGINT, MainSignalHandler);
	signal (SIGTERM, MainSignalHandler);
	signal (SIGABRT, MainSignalHandler);
	signal (SIGSEGV, MainSignalHandler);

	// Un-broken pipify
	signal (SIGPIPE, SIG_IGN);
	signal (SIGCHLD, sigChildHandler);
	#ifdef SIGWINCH
		signal (SIGWINCH, sig_winch);
	#endif

	InitCurses();

	// Check if configfiles exist and create/read them.
	LoadAllFeeds (Config());
	if (autoupdate)
		UpdateAllFeeds();

	// Give control to main program loop.
	UIMainInterface();

	// We really shouldn't be here at all... ah well.
	return EXIT_SUCCESS;
}
