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

#include <ncurses.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include "config.h"
#include "interface.h"
#include "ui-support.h"
#include "main.h"
#include "updatecheck.h"
#include "setup.h"
#include "io-internal.h"
#include "getopt.h"

extern struct feed *first_bak;		/* For use with the signal handler. */
static int const pid_file_create = 1;
static int const pid_file_delete = 0;

static int last_signal = 0;

int cursor_always_visible = 0;

char *forced_target_charset = NULL;

void modifyPIDFile (int action) {
	char pid_path[512];
	FILE *file;
	
	snprintf(pid_path, sizeof(pid_path), "%s/.snownews/pid", getenv("HOME"));
	if (action == pid_file_create) {
		file = fopen(pid_path, "w");
		if (file == NULL) {
			printf("Unable to write PID file %s!", pid_path);
			return;
		}
		fprintf(file, "%d", getpid());
		fclose(file);
	} else {
		unlink(pid_path);
	}
}

void checkPIDFile (void) {
	char pid_path[256];
	char buf[16];
	char *choice;
	int pid;
	FILE *pidfile;
	struct stat filetest;
	
	snprintf(pid_path, sizeof(pid_path), "%s/.snownews/pid", getenv("HOME"));
	
	if (stat (pid_path, &filetest) == 0) {
		pidfile = fopen(pid_path, "r");
		if (pidfile) {
			fgets(buf, sizeof(buf), pidfile);
			pid = atoi(buf);
			
			if (kill(pid, 0) == 0) {
				printf("Snownews seems to be already running with process ID %d.\n", pid);
				printf("A pid file exists at \"%s\".\n", pid_path);
				exit(2);
			} else {
				printf("A pid file exists at \"%s\",\nbut Snownews doesn't seem to be running. Delete that file and start it again.\n", pid_path);
				printf("Continue anyway? (y/n) ");
				choice = fgets(buf, 2, stdin);
				if (choice[0] == 'y') {
					modifyPIDFile(pid_file_delete);
					return;
				} else {
					exit(2);
				}
			}
		}
	}
}

/* Deinit ncurses and quit. */
void MainQuit (const char * func, const char * error) {
	if (error == NULL) {
		/* Only save settings if we didn't exit on error. */
		WriteCache();
	}
	clear();
	refresh();
	endwin();		/* Make sure no ncurses function is called after this point! */
	
	modifyPIDFile(pid_file_delete);
	
	if (last_signal)
		printf ("Exiting via signal %d.\n", last_signal);
	
	if (error == NULL) {		
		printf (_("Bye.\n\n"));
		
		/* Do this after everything else. If it doesn't work or hang
		   user can ctrl-c without interfering with the program operation
		   (like writing caches). */
		AutoVersionCheck();
		
		exit(0);
	} else {
		printf (_("Aborting program execution!\nAn internal error occured. Snownews has quit, no changes has been saved!\n"));
		printf (_("This shouldn't happen, please submit a bugreport to kiza@kcore.de, tell me what you where doing and include the output below in your mail.\n"));
		printf ("----\n");
		/* Please don't localize! I don't want to receive Japanese error messages.
		 * Thanks. :)
		 */
		printf ("While executing: %s\n", func);
		printf ("Error as reported by the system: %s\n\n", error);
		exit(1);
	}
}

/* Signal handler function. */
void MainSignalHandler (int sig) {
	last_signal = sig;
	
	/* If there is a first_bak!=NULL a filter is set. Reset first_ptr
	   so the correct list gets written on the disk when exisiting via SIGINT. */
	if (first_bak != NULL)
		first_ptr = first_bak;
	MainQuit(NULL, NULL);
}

/* Automatic child reaper. */
static void sigChildHandler (int sig __attribute__((unused))) {
	/* Wait for any child without blocking */
	waitpid (-1, NULL, WNOHANG);
}

void printHelp (void) {
	printf (_("Snownews version %s\n\n"), VERSION);
	printf (_("usage: snownews [-huV] [--help|--update|--version]\n\n"));
	printf (_("\t--charset|-l\tForce using this charset.\n"));
	printf (_("\t--cursor-on|-c\tForce cursor always visible.\n"));
	printf (_("\t--help|-h\tPrint this help message.\n"));
	printf (_("\t--update|-u\tAutomatically update every feed.\n"));
	printf (_("\t--version|-V\tPrint version number and exit.\n"));
}
void badOption (const char * arg) {
	printf (_("Option %s requires an argument.\n"), arg);
}

int main (int argc, char *argv[]) {
#ifdef SIGWINCH
	struct sigaction act;
#endif

	int autoupdate = 0;		/* Automatically update feeds on app start... or not if set to 0. */
	int numfeeds;			/* Number of feeds loaded from Config(). */
	int i = 0;	
	char *arg;
	char *foo;
	
#ifdef LOCALEPATH
	setlocale (LC_ALL, "");
	bindtextdomain ("snownews", LOCALEPATH);
	textdomain ("snownews");
#endif
	
	if (argc > 1) {
		i = 1;
		while (i < argc) {
			arg = argv[i];
			if (strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0) {
				printf (_("Snownews version %s\n\n"), VERSION);
				return 0;
			} else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--update") == 0) {
				autoupdate = 1;
				i++;
				continue;
			} else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--cursor-on") == 0) {
				cursor_always_visible = 1;
				i++;
				continue;
			} else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--charset") == 0) {
				foo = argv[i+1];
				if (foo) {
					forced_target_charset = foo;
					i += 2;
				} else {
					badOption(arg);
					exit(1);
				}
				continue;
			} else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				printHelp();
				return 0;
			} else {
				printf ("Unkown option given: \"%s\".\n", arg);
				printHelp();
				return 0;
			}
		}
	}
	
	checkPIDFile();
	
	/* Create PID file. */
	modifyPIDFile(pid_file_create);

	signal (SIGHUP, MainSignalHandler);
	signal (SIGINT, MainSignalHandler);
	signal (SIGTERM, MainSignalHandler);
	
	/* Un-broken pipify */
	signal (SIGPIPE, SIG_IGN);
	
	signal (SIGCHLD, sigChildHandler);
	
#ifdef SIGWINCH
	/* Set up SIGWINCH handler */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_winch;
	sigaction(SIGWINCH, &act, NULL);
#endif

	InitCurses();
	
	/* Check if configfiles exist and create/read them. */
	numfeeds = Config();
			
	LoadAllFeeds (numfeeds);

	if (autoupdate)
		UpdateAllFeeds();
	
	/* Init the pRNG. See about.c for usages of rand() ;) */
	srand(time(0));
	
	/* Give control to main program loop. */
	UIMainInterface();

	/* We really shouldn't be here at all... ah well. */
	MainQuit(NULL, NULL);
	return 0;
}
