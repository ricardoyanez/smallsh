/*
 * Author: Ricardo Yanez <yanezr@oregonstate.edu>
 * Date: 11/1/2021
 */

#ifndef __SMALLSH_H
#define __SMALLSH_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

#include <sys/types.h>

#include <sys/wait.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>

#include <time.h>

#define _NUM_ARGS 512      // maximum number of arguments in command line
#define _BUFFER_SIZE 2048  // maximum size of command line
#define _NUM_PROCIDS 1024  // maximum number of processes saved

// this global variable is needed because we
// cannot pass values to signal handlers
bool fgonly = false;

void rdir_stdin (char *pathname, int flags);
void rdir_stdout (char *pathname, int flags, mode_t mode);

#endif
