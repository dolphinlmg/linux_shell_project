#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CMD_ARG 15
#define BUFSIZ 256

const char *prompt = "myshell> ";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
int is_signal_caught = 0;
int is_background;

struct sigaction act_new;
struct sigaction act_old;

int is_child = 0;

typedef enum {
    DEFAULT,
    CD,
    EXIT
} CMD_TYPE;

int is_pipe_exist(char** cmdv) {
    int ret = 0;

    for (int i = 0; cmdv[i] != NULL; i++) 
        if (!strcmp(cmdv[i], "|")) ret++;

    return ret;
}

void child_handler(int sig) {
    int status;
    pid_t pid;
    while (pid = waitpid(-1, &status, WNOHANG) > 0);
}

void int_handler(int sig) {
    write(1, "\n", 1);
    is_signal_caught = 1;
}

void fatal(char *str){
	perror(str);
	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){	
  int i = 0;
  int numtokens = 0;
  char *snew = NULL;

  if( (s==NULL) || (delimiters==NULL) ) return -1;

  snew = s + strspn(s, delimiters);	/* Skip delimiters */
  if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
    return numtokens;
	
  numtokens = 1;
  
  while(1){
     if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
	break;
     if(numtokens == (MAX_LIST-1)) return -1;
     numtokens++;
  }
  return numtokens;
}

int is_background_cmd(char *cmd) {
    if (cmd[strlen(cmd) - 1] == '&') {
        cmd[strlen(cmd) - 1] = '\0';
        return 1;
    }
    return 0;
}

int get_cmd_num(const char* cmd) {
    if (!strcmp(cmd, "cd"))
        return 1;

    if (!strcmp(cmd, "exit"))
        return 2;

    return 0;
}

int redirect_out(char** cmdv) {
    int i = 0; 
    int fd;

    for (; cmdv[i] != NULL; i++) {
        if (!strcmp(cmdv[i], ">"))
            break;
    }

    if (cmdv[i] == NULL) return -1;

    if (cmdv[++i] == NULL) return -2;

    if ((fd = open(cmdv[i], O_WRONLY | O_CREAT | O_TRUNC, 0640)) == -1) {
        perror("redirect_out");
        return -3;
    }

    if (dup2(fd, 1) < 0)
        perror("dup2");

    close(fd);
    cmdv[i] = NULL;
    cmdv[--i] = NULL; 

    for (; cmdv[i + 2] != NULL; i++) {
        cmdv[i] = cmdv[i + 2];
    }

    cmdv[i] = NULL;

    return 0;
}

int redirect_in(char** cmdv) {
    int i = 0; 
    int fd;

    for (; cmdv[i] != NULL; i++) {
        if (!strcmp(cmdv[i], "<"))
            break;
    }

    if (cmdv[i] == NULL) return -1;

    if (cmdv[++i] == NULL) return -2;

    if ((fd = open(cmdv[i], O_RDONLY)) == -1) {
        perror("redirect_in");
        return -3;
    }

    if (dup2(fd, 0) < 0)
        perror("dup2");

    close(fd);
    cmdv[i] = NULL;
    cmdv[--i] = NULL; 

    for (; cmdv[i + 2] != NULL; i++) {
        cmdv[i] = cmdv[i + 2];
    }

    cmdv[i] = NULL;

    return 0;
}

void pipe_chain(char** cmdv, int* fds) {
    int last_pipe = -1;
    int pipes[2]; // me - pipe - prev
    char* cmd[MAX_CMD_ARG];

    for (int i = 0; cmdv[i] != NULL; i++) {
        if (!strcmp(cmdv[i], "|")) last_pipe = i;
    }

    if (last_pipe == -1) {
        redirect_in(cmdv);

        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);

        printf("last");
        execvp(cmdv[0], cmdv);
        perror("pipe execvp");
    } else {
        for (int i = 0; cmdv[i + last_pipe + 1] != NULL; i++) {
            cmd[i] = cmdv[i + last_pipe + 1];
            cmd[i+1] = NULL;
        } 

        cmdv[last_pipe] = NULL;

        pipe(pipes);

        switch (fork()) {
            case 0:
                pipe_chain(cmdv, pipes);
                break;
            case -1:
                perror("pipe fork");
                break;
            default:
                dup2(pipes[0], 0);
                close(pipes[0]);
                close(pipes[1]);
                if (fds == NULL) {
                    redirect_out(cmd);
                } else {
                    dup2(fds[1], 1);
                    close(fds[1]);
                    close(fds[0]);
                }

                execvp(cmd[0], cmd);
                perror("pipe_chain execvp");
                break;
        }

    }
}

void exec_pipe(char** cmdv) {
    int pid;
    switch (pid = fork()) {
        case 0:
            sigaction(SIGINT, &act_old, NULL);
            sigaction(SIGQUIT, &act_old, NULL);

            pipe_chain(cmdv, NULL);
            break;
        case -1:
            perror("pipe fork()");
            break;
        default:
            if (!is_background) 
                wait(NULL);
            break;
    }
}

int main(int argc, char**argv){
  int i=0;
  pid_t pid, cpid;
  CMD_TYPE type;

  signal(SIGCHLD, (void*)child_handler);
  
  act_new.sa_handler = int_handler;
  sigemptyset(&act_new.sa_mask);

  sigaction(SIGINT, &act_new, &act_old);
  sigaction(SIGQUIT, &act_new, NULL);

  while (1) {
    is_signal_caught = 0;
  	fputs(prompt, stdout);
	fgets(cmdline, BUFSIZ, stdin);

    if (is_signal_caught) {
        is_signal_caught = 0;
        continue;
    }

	cmdline[strlen(cmdline) -1] = '\0';

    if (strlen(cmdline) == 0) continue;

    is_background = is_background_cmd(cmdline);
	makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);

    // debug
    

    // debug

    if (type = (CMD_TYPE) get_cmd_num(cmdvector[0])) {
        switch(type) {
            case CD:
                if (chdir(cmdvector[1]))
                    fatal("main(): ");
                break;
            case EXIT:
                exit(0);
                break;
            default:
                break;
        }

        continue;
    }

    if (is_pipe_exist(cmdvector)) {
        exec_pipe(cmdvector);
        continue;
    }

	switch(pid=fork()){
	case 0:
        sigaction(SIGINT, &act_old, NULL);
        sigaction(SIGQUIT, &act_old, NULL);

        redirect_out(cmdvector);
        redirect_in(cmdvector);
        
	    execvp(cmdvector[0], cmdvector);
	    fatal("main()");
	case -1:
  		fatal("main()");
    default:
        if (!is_background) 
            wait(NULL);
	}
  }
  return 0;
}
