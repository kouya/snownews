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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "config.h"

#include "ui-support.h"

char *pipe_command_string(char const *command, char * const argv[], char const *data);
int pipe_command_buf(char const *command, char * const argv[],
                     void const *inbuf, int inbuf_size, 
                     void **outbuf, int *outbuf_size);

// Load output of local script. Must be valid RSS.
int FilterExecURL (struct feed * cur_ptr) {
	int len = 1;
	char *command;
	char *freeme;
	char buf[4096];
	FILE *scriptoutput;
	
	command = strdup (cur_ptr->feedurl);
	freeme = command;
	
	strsep (&command, ":");
	
	snprintf (buf, sizeof(buf), _("Loading \"%s\""), command);
	UIStatus (buf, 0, 0);
	
	// Make room for new stuff. Ah, how I enjoy freeing null pointers!
	free (cur_ptr->feed);
	
	cur_ptr->feed = malloc (1);
	cur_ptr->feed[0] = '\0';
	
	scriptoutput = popen (command, "r");
	
	while (!feof(scriptoutput)) {
		if ((fgets (buf, sizeof(buf), scriptoutput)) == NULL)
			break;
		
		len += 4096;
		cur_ptr->feed = realloc (cur_ptr->feed, len);
		
		strcat (cur_ptr->feed, buf);
	}

	// Set title and link structure to something.
	// To the feedurl in this case so the program shows something
	// as placeholder instead of crash.
	if (cur_ptr->title == NULL)
		cur_ptr->title = strdup (cur_ptr->feedurl);
	if (cur_ptr->link == NULL)
		cur_ptr->link = strdup (cur_ptr->feedurl);
			
	pclose (scriptoutput);
	free (freeme);
	
	// Need to set content length so using a filter on an execurl works.
	cur_ptr->content_length = strlen(cur_ptr->feed);
	
	return 0;
}

// Replaces content in cur_ptr->feed with output of script in
// cur_ptr->pipethrough.
// This should probably use pipes, but I couldn't get it to work.
int FilterPipe (struct feed * cur_ptr) {
	int len = 0;
	int retval;
	int fd;
	char tmp[512];
	char command[512];
	char buf[4096];
	char tmpfile[] = "/tmp/.snownews.tmp";
	FILE *file;

	// Don't call me anymore.
	assert(0);

	snprintf (command, sizeof(command), "%s < %s", cur_ptr->perfeedfilter, tmpfile);
	
	// Make sure there is no file with the name we're going to write to.
	unlink (tmpfile);
	
	// Write contents we're going to process to a temp file.
	// Now use O_EXCL to avoid any tmpfile/symlink attacks.
	fd = open (tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd == -1) {
		snprintf (tmp, sizeof(tmp), _("Could not write temp file for filter: %s"), strerror(errno));
		UIStatus (tmp, 2, 1);
		return 1;
	}
	file = fdopen (fd, "w");
		
	fwrite (cur_ptr->feed, cur_ptr->content_length, 1, file);
	fclose (file);
	
	free (cur_ptr->feed);
	
	cur_ptr->feed = malloc (1);
	cur_ptr->feed[0] = '\0';
	
	// Pipe temp file contents to process and popen it.
	file = popen (command, "r");
	
	while (!feof(file)) {
		// Strange, valgrind blows up if I use the usual fgets() read here.
		retval = fread (buf, 1, sizeof(buf), file);
		if (retval == 0)
			break;
		cur_ptr->feed = realloc (cur_ptr->feed, len+retval + 1);
		memcpy (cur_ptr->feed+len, buf, retval);
		len += retval;
		if (retval != 4096)
			break;
	}
	cur_ptr->feed[len] = '\0';
	
	pclose (file);
	
	// Clean up.
	unlink (tmpfile);
	
	return 0;
}

int FilterPipeNG (struct feed * cur_ptr) {
	char *data;
	char *filter;
	char *command;
	char **options = NULL;
	int i = 0, rc;
	
	if (cur_ptr->perfeedfilter == NULL)
		return -1;

	data = malloc (cur_ptr->content_length+1);
	memcpy (data, cur_ptr->feed, cur_ptr->content_length);
	data[cur_ptr->content_length] = 0;
	
	free (cur_ptr->feed);
	cur_ptr->feed = NULL;
	
	filter = strdup (cur_ptr->perfeedfilter);
	command = strsep (&filter, " ");
	
	options = malloc(sizeof(char *));
	options[0] = command;
	i++;
	
	for (;;) {
		options = realloc(options, sizeof(char *)*(i+1));
		options[i] = strsep(&filter, " ");
		if (options[i] == NULL)
			break;
		i++;
	}
	
	rc = pipe_command_buf (command, options,
	                       data, cur_ptr->content_length,
	                       (void **)&cur_ptr->feed, &cur_ptr->content_length);
	
	
	free (data);
	free (command);
	free (options);			// options[i] contains only pointers!
	
	return rc;
}

int pipe_command_buf(char const *command, char * const argv[],
                     void const *inbuf, int inbuf_size, 
                     void **outbuf, int *outbuf_size)
{
        static int const READ_END = 0;
        static int const WRITE_END = 1;
        static int const READ_CHUNK_SIZE = 4096;
        int output_pipe[2], input_pipe[2];
        int result;
        void *buf = NULL, *buf_cur = NULL;
        int buf_size = 0;
        int bytes_read = 0;
        char tmp[128];

        result = pipe(input_pipe);
        if (result == -1) {
                perror("Couldn't create input pipe");
                return -1;
        }

        result = pipe(output_pipe);
        if (result == -1) {
                perror("Couldn't create output pipe");
                return -1;
        }

        result = fork();
        if (result == 0) {
                close(output_pipe[WRITE_END]);
                close(input_pipe[READ_END]);

                result = dup2(output_pipe[READ_END], fileno(stdin));
                if (result == -1) {
                        perror("dup2 error on output_pipe");
                        exit(1);
                }
                close(output_pipe[READ_END]);

                result = dup2(input_pipe[WRITE_END], fileno(stdout));
                if (result == -1) {
                        perror("dup2 error on input_pipe");
                        exit(1);
                }
                close(input_pipe[WRITE_END]);

                result = execvp(command, argv);
                if (result == -1) {
                		snprintf (tmp, sizeof(tmp), "Exec of \"%s\" failed", command);
                        perror(tmp);
                        exit(1);
                }
                
                exit(1);
        }
        else {
                close(output_pipe[READ_END]);
                close(input_pipe[WRITE_END]);

                write(output_pipe[WRITE_END], inbuf, inbuf_size);
                close(output_pipe[WRITE_END]);

                do {
                        buf_size += READ_CHUNK_SIZE;
                        buf = realloc(buf, buf_size+1);
                        buf_cur = buf+buf_size-READ_CHUNK_SIZE;
                        result = read(input_pipe[READ_END], buf_cur, READ_CHUNK_SIZE);
                        if (result > 0) {
                        	bytes_read += result;
                                if (result < READ_CHUNK_SIZE) {
                                        buf_size -= (READ_CHUNK_SIZE - result);
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
					((char *)buf)[buf_size] = '\0';
	
					*outbuf = buf;
					*outbuf_size = buf_size;
                }
        }

        return 0;
}

char *pipe_command_string(char const *command, char * const argv[], char const *data)
{
        void *buf = NULL;
        int buf_size;
        char *result = NULL;

        if (pipe_command_buf(command, argv, data, strlen(data), &buf, &buf_size) == 0) {
                result = malloc(buf_size+1);
                memcpy(result, buf, buf_size);
                result[buf_size] = 0;
        }
        free(buf);

        return result;
}
