// This file is part of Snownews - A lightweight console RSS newsreader
//
// Copyright (c) 2003-2004 Oliver Feiler <kiza@kcore.de>
// Copyright (c) 2021 Mike Sharov <msharov@users.sourceforge.net>
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
#include "ui.h"
#include "feedio.h"
#include "setup.h"
#include "uiutil.h"
#include <ncurses.h>
#include <signal.h>
#include <sys/wait.h>

//{{{ Global variables -------------------------------------------------

struct feed* _feed_list = NULL;
struct feed* _unfiltered_feed_list = NULL;	// Backup first pointer for filter mode.
				// Needs to be global so it can be used in the signal handler.
				// Must be set to NULL by default and whenever it's not used anymore!
bool _feed_list_changed = false;

struct settings _settings = {
    .keybindings = {
		    //{{{2 Default values for keybindings.
		    // If some are defined differently in the keybindings file
		    // they will be overwritten. If some are missing or broken
		    // these are sane defaults.
		    .about = 'A',
		    .addfeed = 'a',
		    .andxor = 'X',
		    .categorize = 'C',
		    .changefeedname = 'c',
		    .deletefeed = 'D',
		    .dfltbrowser = 'B',
		    .end = '>',
		    .enter = 'l',
		    .feedinfo = 'i',
		    .filter = 'f',
		    .filtercurrent = 'g',
		    .forcereload = 'T',
		    .help = 'h',
		    .home = '<',
		    .markallread = 'm',
		    .markread = 'm',
		    .markunread = 'M',
		    .movedown = 'N',
		    .moveup = 'P',
		    .newheadlines = 'H',
		    .next = 'n',
		    .nofilter = 'F',
		    .pdown = ' ',
		    .perfeedfilter = 'e',
		    .prev = 'p',
		    .prevmenu = 'q',
		    .pup = 'b',
		    .quit = 'q',
		    .reload = 'r',
		    .reloadall = 'R',
		    .sortfeeds = 's',
		    .typeahead = '/',
		    .urljump = 'o',
		    .urljump2 = 'O'
		    //}}}2
		     },
    .color = {
	      .feedtitle = -1,
	      .feedtitlebold = 0,
	      .newitems = 5,
	      .newitemsbold = 0,
	      .urljump = 4,
	      .urljumpbold = 0 },
};

//----------------------------------------------------------------------

static int last_signal = 0;

//}}}-------------------------------------------------------------------
//{{{ PID file management

enum EPIDAction { pid_file_delete, pid_file_create };

static void make_pidfile_name (char* fnbuf, size_t fnbufsz)
{
    const char* rundir = getenv ("XDG_RUNTIME_DIR");
    if (!rundir)
	rundir = getenv ("TMPDIR");
    if (!rundir)
	rundir = "/tmp";
    snprintf (fnbuf, fnbufsz, "%s/snownews.pid", rundir);
}

static void modifyPIDFile (enum EPIDAction action)
{
    char pid_path[PATH_MAX];
    make_pidfile_name (pid_path, sizeof (pid_path));
    if (action == pid_file_create) {
	FILE* file = fopen (pid_path, "w");
	if (!file) {
	    printf ("Unable to write PID file %s!", pid_path);
	    return;
	}
	fprintf (file, "%d", getpid());
	fclose (file);
    } else
	unlink (pid_path);
}

static void checkPIDFile (void)
{
    char pid_path[PATH_MAX];
    make_pidfile_name (pid_path, sizeof (pid_path));

    FILE* pidfile = fopen (pid_path, "r");
    if (!pidfile)
	return;
    char pidbuf[12];
    fgets (pidbuf, sizeof (pidbuf), pidfile);
    fclose (pidfile);
    pid_t pid = atoi (pidbuf);

    if (kill (pid, 0) == 0) {
	printf ("Snownews seems to be already running with process ID %d.\n", pid);
	printf ("A pid file exists at \"%s\".\n", pid_path);
	exit (2);
    } else {
	printf ("A pid file exists at \"%s\",\nbut Snownews doesn't seem to be running. Delete that file and start it again.\n", pid_path);
	printf ("Continue anyway? (y/n) ");
	char ybuf[2] = { };
	fgets (ybuf, sizeof (ybuf), stdin);
	if (ybuf[0] != 'y')
	    exit (2);
	modifyPIDFile (pid_file_delete);
    }
}

//}}}-------------------------------------------------------------------
//{{{ stderr log redirection

static int MakeStderrLogFileName (char* logfile, size_t logfilesz)
{
    const char* tmpdir = getenv ("TMPDIR");
    if (!tmpdir)
	tmpdir = "/tmp";
    unsigned r = snprintf (logfile, logfilesz, "%s/" SNOWNEWS_NAME ".log", tmpdir);
    return r >= logfilesz ? -1 : 0;
}

// If the log file is empty at exit, delete it
static void CleanupStderrLog (void)
{
    char logfile[PATH_MAX];
    if (0 == MakeStderrLogFileName (logfile, sizeof (logfile))) {
	struct stat st;
	if (0 == stat (logfile, &st) && st.st_size == 0)
	    unlink (logfile);
    }
}

// Redirects stderr into a log file in /tmp
static void RedirectStderrToLog (void)
{
    char logfile[PATH_MAX];
    if (0 > MakeStderrLogFileName (logfile, sizeof (logfile)))
	return;

    int fd = open (logfile, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0)
	return;

    dup2 (fd, STDERR_FILENO);
    close (fd);
    atexit (CleanupStderrLog);
}

//}}}-------------------------------------------------------------------
//{{{ Signal handling

// Deinit ncurses and quit.
_Noreturn void MainQuit (const char* func, const char* error)
{
    if (!error)		       // Only save settings if we didn't exit on error.
	WriteCache();
    endwin();		       // Make sure no ncurses function is called after this point!
    modifyPIDFile (pid_file_delete);

    if (last_signal)
	printf ("Exiting via signal %d.\n", last_signal);
    if (error) {
	printf (_("Aborting program execution!\nAn internal error occured. Snownews has quit, no changes has been saved!\n"));
	printf (_("Please submit a bugreport at https://github.com/msharov/snownews/issues\n"));
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
static _Noreturn void MainSignalHandler (int sig)
{
    last_signal = sig;

    // If there is a _unfiltered_feed_list!=NULL a filter is set. Reset _feed_list
    // so the correct list gets written on the disk when exisiting via SIGINT.
    if (_unfiltered_feed_list)
	_feed_list = _unfiltered_feed_list;
    MainQuit (NULL, "Signal");
}

// Automatic child reaper.
static void sigChildHandler (int sig __attribute__((unused)))
{
    // Wait for any child without blocking
    waitpid (-1, NULL, WNOHANG);
}

static void InstallSignalHandlers (void)
{
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
}

//}}}-------------------------------------------------------------------
//{{{ Command line options help

static void printHelp (void)
{
    printf (_("Snownews %s\n\n"), SNOWNEWS_VERSTRING);
    printf (_("usage: snownews [-huV] [--help|--update|--version]\n\n"));
    printf (_("\t--charset|-l\tForce using this charset.\n"));
    printf (_("\t--cursor-on|-c\tForce cursor always visible.\n"));
    printf (_("\t--help|-h\tPrint this help message.\n"));
    printf (_("\t--update|-u\tAutomatically update every feed.\n"));
    printf (_("\t--version|-V\tPrint version number and exit.\n"));
}

static void badOption (const char* arg)
{
    printf (_("Option %s requires an argument.\n"), arg);
}

//}}}-------------------------------------------------------------------
//{{{ srandrand

inline static uint32_t ror32 (uint32_t v, unsigned n)
{
    return (v >> n) | (v << (32 - n));
}

// Randomly initializes the random number generator
static void srandrand (void)
{
    struct timespec now;
    clock_gettime (CLOCK_REALTIME, &now);
    uint32_t seed = now.tv_sec;
    seed ^= getppid();
    seed = ror32 (seed, 16);
    seed ^= getpid();
    seed ^= now.tv_nsec;
    srand (seed);
}

//}}}-------------------------------------------------------------------

int main (int argc, char* argv[])
{
    InstallSignalHandlers();
    srandrand();
    setlocale (LC_ALL, "");
#ifdef LOCALEPATH
    bindtextdomain (SNOWNEWS_NAME, LOCALEPATH);
    textdomain (SNOWNEWS_NAME);
#endif
    RedirectStderrToLog();

    bool autoupdate = false;	// Automatically update feeds on app start... or not if set to 0.
    for (int i = 1; i < argc; ++i) {
	char* arg = argv[i];
	if (strcmp (arg, "--version") == 0 || strcmp (arg, "-V") == 0) {
	    printf (_("Snownews version %s\n\n"), SNOWNEWS_VERSION);
	    return EXIT_SUCCESS;
	} else if (strcmp (arg, "-u") == 0 || strcmp (arg, "--update") == 0) {
	    autoupdate = true;
	} else if (strcmp (arg, "-c") == 0 || strcmp (arg, "--cursor-on") == 0) {
	    _settings.cursor_always_visible = true;
	} else if (strcmp (arg, "-l") == 0 || strcmp (arg, "--charset") == 0) {
	    const char* chset = argv[i++];
	    if (chset)
		_settings.global_charset = chset;
	    else {
		badOption (arg);
		return EXIT_FAILURE;
	    }
	} else if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
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
