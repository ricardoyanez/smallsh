/*
 * Author: Ricardo Yanez <yanezr@oregonstate.edu>
 * Date: 11/1/2021

 * Course: CS 344 (Operating Systems I)
 * Assignment: Assignment 3
 * Program: smallsh
 *
 */

#include "smallsh.h"

/* signal handler (copied from Exploration) */
void handle_SIGTSTP( int signo ){
  char *msg;
  if ( fgonly ) {
    msg = "Exiting foreground-only mode\n";
    fgonly = false;
  }
  else {
    msg = "Entering foreground-only mode (& is now ignored)\n";
    fgonly = true;
  }
  write(STDOUT_FILENO, msg,strlen(msg));
}

/* structure for process IDs */
struct procid {
  pid_t childPid;
  int status;
  bool alive;
};

int main () {

  char **argv = calloc(_NUM_ARGS,sizeof(char*));
  int argc = 0;
  char *sav;
  char *buffer = calloc(_BUFFER_SIZE,sizeof(char));

  // signal handling (copied from Exploration)
  struct sigaction SIGINT_action = {{0}}, SIGTSTP_action = {{0}};

  // ignore SIGINT
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);

  // handle SIGTSTP
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // define array of proccess IDs
  struct procid *procids = calloc(_NUM_PROCIDS,sizeof(struct procid));
  int nproc = 0;

  bool on = true;

  do {

    // reset buffer
    buffer[0] = '\0';

    // prompt for a command line
    write(STDOUT_FILENO,": ",2);
    fgets(buffer,_BUFFER_SIZE,stdin);

    // remove the newline character
    if ( strlen(buffer) ) {
      buffer[strlen(buffer)-1] = '\0';
    }

    // decode command-line

    // variable substitution $$
    char *tmpbuf = calloc(_BUFFER_SIZE,sizeof(char));
    if ( strstr(buffer,"$$") != NULL ) {
      while ( strcspn(buffer,"$$") < strlen(buffer)-1 ) {
	strtok_r(buffer,"$$",&sav);
	sprintf(tmpbuf,"%s%d%s",buffer,getpid(),sav+1);
	strcpy(buffer,tmpbuf);
      }
    }
    free(tmpbuf);

    // pass command and arguments to argv[]
    argv[argc++] = strtok_r(buffer," ",&sav);
    while ( strlen(sav) > 0 ) {
      argv[argc] = sav;
      strtok_r(argv[argc++]," ",&sav);
    }

    // set the last string to NULL
    argv[argc] = '\0';

    if ( argv[0] != NULL ) {

      bool redirect_stdin = false;
      bool redirect_stdout = false;
      bool background = false;
      int ifil, ofil;
      int wstatus;

      // built-in commands exit, cd and status

      // exit built-in command
      if ( strcmp(argv[0],"exit") == 0 ) {
	// kill all children before exiting
	for ( int i = 0 ; i < nproc ; i++ ) {
	  kill(procids[i].childPid,SIGKILL);
	}
	on = false;
      }

      // cd built-in command
      else if ( strcmp(argv[0],"cd") == 0 ) {
	// cd with argument
	if ( argc > 1 ) {
	  if ( chdir(argv[1]) == -1 ) {
	    perror(argv[0]);
	  }
	}
	// cd to $HOME
	else {
	  chdir(getenv("HOME"));
	}
      }

      // status built-in command
      else if ( strcmp(argv[0],"status") == 0 ) {
	if ( nproc > 0 ) {
	  // get status of previous child
	  wstatus = procids[nproc-1].status;
	  if ( WIFEXITED(wstatus) ) {
	    printf("exit value %d\n",WEXITSTATUS(wstatus));
	    fflush(stdout);
	  }
	  if ( WIFSIGNALED(wstatus) ) {
	    printf("terminated by signal %d\n",WTERMSIG(wstatus));
	    fflush(stdout);
	  }
	}
      }

      // execute all other commands by forking
      else {

	// background
	if ( strcmp(argv[argc-1],"&") == 0 ) {
	  // remove & from command line
	  argv[--argc] = '\0';
	  // background only if not in foreground-only mode
	  if ( !fgonly ) {
	    background = true;
	  }
	}

	// check for redirections and save the index of filenames
	for ( int i = 0 ; i < argc ; i++ ) {
	  if ( strcmp(argv[i],"<") == 0 ) {
	    ifil = i+1;
	    redirect_stdin = true;
	  }
	  else if ( strcmp(argv[i],">") == 0 ) {
	    ofil = i+1;
	    redirect_stdout = true;
	  }
	}

	pid_t childPid;
	pid_t parentPid = getpid();
	pid_t spawnPid = fork();

	switch( spawnPid ) {

	case -1:
	  perror("fork");
	  exit(1);
	  break;

	  // child process
	case 0:

	  // the child process ignores SIGTSTP
	  SIGTSTP_action.sa_handler = SIG_IGN;
	  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	  // in foreground allow the child to receive SIGINT
	  if ( !background ) {
	    SIGINT_action.sa_handler = SIG_DFL;
	    sigaction(SIGINT, &SIGINT_action, NULL);
	  }

	  // comments
	  if ( strchr(argv[0],'#') != NULL ) {
	    exit(0);
	  }

	  // redirecting

	  // in foreground redirect to file
	  if ( redirect_stdin ) {
	    rdir_stdin(argv[ifil],O_RDONLY);
	  }
	  if ( redirect_stdout ) {
	    rdir_stdout(argv[ofil],O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR+S_IWUSR+S_IRGRP+S_IROTH);
	  }
	  // in background redirect to /dev/null if not to file
	  if ( background ) {
	    if ( !redirect_stdin ) {
	      rdir_stdin("/dev/null",O_RDONLY);
	    }
	    if ( !redirect_stdout ) {
	      rdir_stdout("/dev/null",O_WRONLY|O_CREAT,S_IRUSR+S_IWUSR+S_IRGRP+S_IWGRP+S_IROTH+S_IWOTH);
	    }
	  }

	  // foreground redirect
	  if ( redirect_stdin || redirect_stdout ) {
	    execlp(argv[0],argv[0],NULL);
	    perror(argv[0]);
	    exit(1);
	  }
	  // all other cases
	  else {
	    execvp(argv[0], argv);
	    perror(argv[0]);
	    exit(1);
	  }

	  // terminate
	  exit(0);
	  break;

	  // parent process
	default:

	  // save process ID
	  procids[nproc].childPid = spawnPid;

	  // background process
	  if ( background ) {
	    childPid = waitpid(spawnPid, &wstatus, WNOHANG);
	    // background process
	    printf("background pid is %d\n",spawnPid);
	    fflush(stdout);
	    procids[nproc].alive = true;
	  }

	  // foreground process
	  else {
	    childPid = waitpid(spawnPid, &wstatus, 0);
	    procids[nproc].status = wstatus;

	    // child terminated normally
	    if ( WIFEXITED(wstatus) ) {
	      procids[nproc].alive = false;
	    }
	    // child has been signaled to terminate
	    if ( WIFSIGNALED(wstatus) ) {
	      procids[nproc].alive = false;
	      printf("terminated by signal %d\n",WTERMSIG(wstatus));
	      fflush(stdout);
	    }

	  }
	  nproc++;
	  break;
	}

	if ( parentPid == getpid() ) {

	  // monitor background processes
	  for ( int i = 0 ; i < nproc ; i++ ) {
	    // child is alive
	    if ( procids[i].alive ) {
	      childPid = waitpid(procids[i].childPid, &wstatus, WNOHANG);
	      if ( childPid == procids[i].childPid ) {
		// background child exited
		if ( WIFEXITED(wstatus) ) {
		  printf("background pid %d is done: exit value %d\n",procids[i].childPid,WEXITSTATUS(wstatus));
		  // kill the process to avoid defunct processes
		  kill(procids[i].childPid,SIGKILL);
		}
		// background child was signaled to terminate
		if ( WIFSIGNALED(wstatus) ) {
		  printf("background pid %d is done: terminated by signal %d\n",procids[i].childPid,WTERMSIG(wstatus));
		}
		fflush(stdout);
		// child is deceased
		procids[i].alive = false;
	      }
	    }
	  }
	}
      }
    }

    if ( nproc > _NUM_PROCIDS ) {
      printf("increase _NUM_PROCIDS or change data structure\n");
    }

    // reset argv
    for ( int i = 0 ; i < argc ; i++ ) {
      argv[i] = '\0';
    }
    argc = 0;

  } while ( on );

  // free allocated memory
  free(buffer);
  for ( int i = 0 ; i < _NUM_ARGS ; i++ ) {
    if ( argv[i] != NULL ) {
      free(argv[i]);
    }
  }
  free(argv);
  free(procids);

  return 0;
}

/*
  function to open a file and duplicate stdin
*/
void rdir_stdin (char *pathname, int flags) {
  int fd = open(pathname,flags);
  if ( fd == -1 ) {
    printf("cannot open %s for input\n",pathname);
    fflush(stdout);
    exit(1);
  }
  if ( dup2(fd,STDIN_FILENO) == -1 ) {
    perror("dup2");
    exit(2);
  }
}

/*
  function to open a file and duplicate stdout
*/
void rdir_stdout (char *pathname, int flags, mode_t mode) {
  int fd = open(pathname,flags,mode);
  if ( fd == -1 ) {
    printf("cannot open %s for output\n",pathname);
    fflush(stdout);
    exit(1);
  }
  if ( dup2(fd,STDOUT_FILENO) == -1 ) {
    perror("dup2");
    exit(2);
  }
}
