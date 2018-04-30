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

#include "config.h"
#include "ui-support.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

//----------------------------------------------------------------------

static int pipe_command_buf (const char* command, char** argv, const void* inbuf, size_t inbuf_size, char** outbuf, unsigned* outbuf_size);

//----------------------------------------------------------------------

// Load output of local script. Must be valid RSS.
int FilterExecURL (struct feed * cur_ptr) {
	char* command = strdup (cur_ptr->feedurl);
	char* freeme = command;

	strsep (&command, ":");

	char buf [BUFSIZ];
	snprintf (buf, sizeof(buf), _("Loading \"%s\""), command);
	UIStatus (buf, 0, 0);

	FILE* scriptoutput = popen (command, "r");
	free (freeme);
	if (!scriptoutput)
	    return -1;

	cur_ptr->content_length = 0;
	cur_ptr->feed = realloc (cur_ptr->feed, cur_ptr->content_length+1);
	cur_ptr->feed[cur_ptr->content_length] = '\0';

	while (!feof(scriptoutput) && fgets (buf, sizeof(buf), scriptoutput)) {
		size_t br = strlen(buf);
		cur_ptr->feed = realloc (cur_ptr->feed, cur_ptr->content_length+br+1);
		memcpy (&cur_ptr->feed[cur_ptr->content_length], buf, br+1);
		cur_ptr->content_length += br;
	}
	pclose (scriptoutput);

	// Set title and link structure to something.
	// To the feedurl in this case so the program shows something
	// as placeholder instead of crash.
	if (cur_ptr->title == NULL)
		cur_ptr->title = strdup (cur_ptr->feedurl);
	if (cur_ptr->link == NULL)
		cur_ptr->link = strdup (cur_ptr->feedurl);
	return 0;
}

int FilterPipeNG (struct feed * cur_ptr) {
	if (cur_ptr->perfeedfilter == NULL)
		return -1;

	char* data = malloc (cur_ptr->content_length+1);
	memcpy (data, cur_ptr->feed, cur_ptr->content_length);
	data[cur_ptr->content_length] = 0;

	free (cur_ptr->feed);
	cur_ptr->feed = NULL;

	char* filter = strdup (cur_ptr->perfeedfilter);
	char* command = strsep (&filter, " ");

	char** options = malloc (sizeof(char*));
	size_t nopts = 0;
	options[nopts++] = command;

	for (;;) {
		options = realloc (options, sizeof(char*)*(nopts+1));
		if (!(options[nopts++] = strsep(&filter, " ")))
			break;
	}

	int rc = pipe_command_buf (command, options,
	                       data, cur_ptr->content_length,
	                       &cur_ptr->feed, &cur_ptr->content_length);

	free (data);
	free (command);
	free (options);			// options[i] contains only pointers!
	return rc;
}

static int pipe_command_buf (const char* command, char** argv, const void* inbuf, size_t inbuf_size, char** outbuf, unsigned* outbuf_size)
{
	enum { READ_END, WRITE_END, N_ENDS };

	int input_pipe [N_ENDS], output_pipe [N_ENDS];
        if (0 != pipe(input_pipe)) {
                perror("Couldn't create input pipe");
                return -1;
        }
        if (0 != pipe(output_pipe)) {
		close (input_pipe[READ_END]);
		close (input_pipe[WRITE_END]);
                perror("Couldn't create output pipe");
                return -1;
        }

        int result = fork();
	if (result < 0) {
		close (input_pipe[READ_END]);
		close (input_pipe[WRITE_END]);
		close (output_pipe[READ_END]);
		close (output_pipe[WRITE_END]);
		perror ("fork");
		return -1;
	} else if (result == 0) {
                close(output_pipe[WRITE_END]);
                close(input_pipe[READ_END]);

                if (0 > dup2 (output_pipe[READ_END], STDIN_FILENO)) {
                        perror("dup2 error on output_pipe");
                        exit (EXIT_FAILURE);
                }
                close(output_pipe[READ_END]);

                if (0 > dup2 (input_pipe[WRITE_END], STDOUT_FILENO)) {
                        perror("dup2 error on input_pipe");
                        exit (EXIT_FAILURE);
                }
                close(input_pipe[WRITE_END]);

                if (0 > execvp(command, argv)) {
			char tmp [PATH_MAX];
			snprintf (tmp, sizeof(tmp), "Exec of \"%s\" failed", command);
                        perror (tmp);
                }
                exit (EXIT_FAILURE);
        } else {
                close(output_pipe[READ_END]);
                close(input_pipe[WRITE_END]);

                write(output_pipe[WRITE_END], inbuf, inbuf_size);
                close(output_pipe[WRITE_END]);

		char *buf = NULL, *buf_cur = NULL;
		int buf_size = 0, bytes_read = 0;
                do {
                        buf_size += BUFSIZ;
                        buf = realloc(buf, buf_size+1);
                        buf_cur = buf+buf_size-BUFSIZ;
                        result = read(input_pipe[READ_END], buf_cur, BUFSIZ);
                        if (result > 0) {
                        	bytes_read += result;
                                if (result < BUFSIZ) {
                                        buf_size -= (BUFSIZ - result);
                                        buf = realloc(buf, buf_size+1);
                                        break;
                                }
                        } else if (result < 0) {
                        	free (buf);
                        	buf = NULL;
                        	return -1;
                        }
                } while (result > 0);
                close(input_pipe[READ_END]);

                if (bytes_read == 0) {
                	free(buf);
                	return -1;
                } else {
			buf[buf_size] = '\0';
			*outbuf = buf;
			*outbuf_size = buf_size;
                }
        }
        return 0;
}
