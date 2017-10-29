/*---------------------------------------------------------------*/
/* A simple shell                                                */
/*                                                               */
/* Process command lines (100 char max) with at most 16 "words"  */
/* and one command. No wildcard or shell variable processing.    */
/* No pipes, conditional or sequential command handling is done. */
/* Each word contains at most MAXWORDLEN characters.             */
/* Words are separated by spaces and/or tab characters.          */
/*---------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#define MAXLINELEN 100		  /* max chars in an input line */
#define NWORDS 16		  /* max words on command line */
#define MAXWORDLEN 64		  /* maximum word length */

extern char **environ;		  /* environment */

char line[MAXLINELEN + 1];	  /* input line */
char *words[NWORDS];              /* ptrs to words from the command line */
char *words2[NWORDS];
int second_flag = 0;

int nwds;			  /* # of words in the command line */
int nwds2;
char path[MAXWORDLEN];		  /* path to the command */
char path2[MAXWORDLEN];
char *argv[NWORDS + 1];		  /* argv structure for execve */

/*------------------------------------------------------------------*/
/* Get a line from the standard input. Return 1 on success, or 0 at */
/* end of file. If the line contains only whitespace, ignore it and */
/* get another line. If the line is too long, display a message and */
/* get another line. If read fails, diagnose the error and abort.   */
/*------------------------------------------------------------------*/
/* This function will display a prompt ("# ") if the input comes    */
/* from a terminal. Otherwise no prompt is displayed, but the       */
/* input is echoed to the standard output. If the input is from a   */
/* terminal, the input is automatically echoed (assuming we're in   */
/* "cooked" mode).                                                  */
/*------------------------------------------------------------------*/
int Getline(void)
{
	int n;		/* result of read system call */
	int len;		/* length of input line */
	int gotnb;		/* non-zero when non-whitespace was seen */
	char c;		/* current input character */
	char *msg;		/* error message */
	int isterm;		/* non-zero if input is from a terminal */

	second_flag = 0;

	isterm = isatty(0);		/* see if file descriptor 0 is a terminal */
	for (;;) {
		if (isterm)
			write(1, "# ", 2);
		gotnb = len = 0;
		for (;;) {

			n = read(0, &c, 1);		/* read one character */

			if (n == 0)			/* end of file? */
				return 0;		/* yes, so return 0 */

			if (n == -1) {		/* error reading? */
				perror("Error reading command line");
				exit(1);
			}

			if (!isterm)		/* if input not from a terminal */
				write(1, &c, 1);		/* echo the character */

			if (c == '\n')		/* end of line? */
				break;

			if (len >= MAXLINELEN) {	/* too many chars? */
				len++;			/* yes, so just update the length */
				continue;
			}

			if (c != ' ' && c != '\t')	/* was input not whitespace? */
				gotnb = 1;

			line[len++] = c;		/* save the input character */
		}

		if (len >= MAXLINELEN) {	/* if the input line was too long... */
			char *msg;
			msg = "Input line is too long.\n";
			write(2, msg, strlen(msg));
			continue;
		}
		if (gotnb == 0)			/* line contains only whitespace */
			continue;

		line[len] = '\0';		/* terminate the line */
		return 1;
	}
}

/*------------------------------------------------*/
/* Parse the command line into an array of words. */
/* Return 1 on success, or 0 if there is an error */
/* (i.e. there are too many words, or a word is   */
/* too long), diagnosing the error.               */
/* The words are identified by the pointed in the */
/* 'words' array, and 'nwds' = number of words.   */
/*------------------------------------------------*/
int parse(void)
{
	char *p;			/* pointer to current word */
	char *msg;			/* error message */

	nwds = 0;

	p = strtok(line, " \t");
	while (p != NULL) {
		if (nwds == NWORDS) {
			msg = "*** ERROR: Too many words.\n";
			write(2, msg, strlen(msg));
			return 0;
		}
		if (strlen(p) >= MAXWORDLEN) {
			msg = "*** ERROR: Word too long.\n";
			write(2, msg, strlen(msg));
			return 0;
		}
		words[nwds] = p;	/* save pointer to the word */
		nwds++;			/* increase the word count */
		p = strtok(NULL, " \t");	/* get pointer to next word, if any */
	}
	return 1;
}

/*--------------------------------------------------------------------------*/
/* Put in 'path' the relative or absolute path to the command identified by */
/* words[0]. If the file identified by 'path' is not executable, return -1. */
/* Otherwise return 0.                                                      */
/*--------------------------------------------------------------------------*/
int execok(void)
{
	int i, j;
	char *p;
	char *pathenv;
	int second_index = -1;

	for (i = 0; i < nwds; i++) {
		if (!strcmp(words[i], "|")) {
			second_flag = 1;
			second_index = i + 1;
		}
	}
	
	if (second_flag == 1) {
		nwds2 = 0;

		for (i = second_index; i < nwds; i++)
			words2[nwds2++] = words[i];

		nwds -= nwds2;
		nwds--;
	}

	int ret1, ret2 = 0;
	/*-------------------------------------------------------*/
	/* If words[0] is already a relative or absolute path... */
	/*-------------------------------------------------------*/
	if (strchr(words[0], '/') != NULL) {		/* if it has no '/' */
		strcpy(path, words[0]);			/* copy it to path */
		ret1 = access(path, X_OK);		/* return executable status */

		if (second_flag == 1) {
			if (strchr(words2[0], '/') != NULL) {
				strcpy(path2, words2[0]);
				ret2 = access(path2, X_OK);
			}
		}
		return ret1 + ret2;
	}

	/*-------------------------------------------------------------------*/
	/* Otherwise search for a valid executable in the PATH directories.  */
	/* We do this by getting a copy of the value of the PATH environment */
	/* variable, and checking each directory identified there to see it  */
	/* contains an executable file named word[0]. If a directory does    */
	/* have such a file, return 0. Otherwise, return -1. In either case, */
	/* always free the storage allocated for the value of PATH.          */
	/*-------------------------------------------------------------------*/
	pathenv = strdup(getenv("PATH"));		/* get copy of PATH value */
	p = strtok(pathenv, ":");			/* find first directory */
	while (p != NULL) {
		strcpy(path, p);				/* copy directory to path */
		strcat(path, "/");			/* append a slash */
		strcat(path, words[0]);			/* append executable's name */
		if (access(path, X_OK) == 0) {		/* if it's executable */
			if (second_flag == 1) {
				pathenv = strdup(getenv("PATH"));
				p = strtok(pathenv, ":");
				while (p != NULL) {
					strcpy(path2, p);
					strcat(path2, "/");
					strcat(path2, words2[0]);
					if (access(path2, X_OK) == 0) {
						free(pathenv);
						return 0;
					}
					p = strtok(NULL, ":");
				}
			}
			free(pathenv);
			return 0;
		}
		p = strtok(NULL, ":");			/* get next directory */
	}

	if (second_flag == 1) {
		pathenv = strdup(getenv("PATH"));		/* get copy of PATH value */
		p = strtok(pathenv, ":");			/* find first directory */
		while (p != NULL) {
			strcpy(path2, p);				/* copy directory to path */
			strcat(path2, "/");			/* append a slash */
			strcat(path2, words2[0]);			/* append executable's name */
			if (access(path2, X_OK) == 0) {		/* if it's executable */
				free(pathenv);				/* free PATH cop */
				return 0;				    /* and return 0 */
			}
			p = strtok(NULL, ":");			/* get next directory */
		}
	}

	free(pathenv);				/* free PATH copy */
	return -1;					/* say we didn't find it */
}

/*--------------------------------------------------*/
/* Execute the command, if possible.                */
/* If it is not executable, return -1.              */
/* Otherwise return 0 if it completed successfully, */
/* or 1 if it did not.                              */
/*--------------------------------------------------*/
int execute(void)
{
	int i, j;
	pid_t pid;
	int fd[2];
	char *msg;

	if (execok() == 0) {
		words[nwds] = NULL;
		words2[nwds2] = NULL;

		/*
		printf("path : %s\n", path);
		while (words[i] != NULL) {
			printf("%d : %s\n", i + 1, words[i]);
			i++;
		}
		*/
		if (second_flag == 1) { // there are pipe in instruction
			/*
			printf("path2 : %s\n", path2);
			while (words2[i] != NULL) {
				printf("%d : %s\n", i + 1, words2[i]);
				i++;
			}
			*/

			pipe(fd);

			pid = fork();

			if (pid == 0) {
				dup2(fd[1], STDOUT_FILENO);
				close(fd[0]);
				close(fd[1]);
				pid = execve(path, words, environ);
				
				exit(1);
				wait(&pid);
			}
			else {
				pid = fork();

				if (pid == 0) {
					dup2(fd[0], STDIN_FILENO);
					close(fd[1]);
					close(fd[0]);
					pid = execve(path2, words2, environ);
					
					exit(1);
					wait(&pid);
				}
				else {
					int status;
					close(fd[0]);
					close(fd[1]);
					waitpid(pid, &status, 0);
				}
			}
		}
		else { // there are no pipes
			pid = fork();

			if (pid == 0) {
				pid = execve(path, words, environ);

				exit(1);
			}
		}
		wait(&pid);
	}
	else { // if execok return value is FALSE
		/*----------------------------------------------------------*/
		/* Command cannot be executed. Display appropriate message. */
		/*----------------------------------------------------------*/
		msg = "*** ERROR: '";
		write(2, msg, strlen(msg));
		write(2, words[0], strlen(words[0]));
		write(2, words2[0], strlen(words2[0]));
		msg = "' cannot be executed.\n\n";
		write(2, msg, strlen(msg));
	}
}

int main(int argc, char *argv[])
{
	while (Getline()) {			/* get command line */
		if (!parse())			/* parse into words */
			continue;			    /* some problem... */
		execute();			/* execute the command */
	}
	write(1, "\n", 1);
	return 0;
}
