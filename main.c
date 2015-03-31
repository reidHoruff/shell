/**
 * Reid Horuff
 * 3-30-2015
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>

#define WRITE_PIPE 1
#define READ_PIPE 0
 
int run_command(char** arg_list, int read_fd, int write_fd) {
  pid_t child_pid; 
 
  child_pid = fork(); 
 
  if (child_pid == 0){
    if (read_fd != STDIN_FILENO){
      if(dup2(read_fd, STDIN_FILENO) != STDIN_FILENO){
	fprintf(stderr, "Error: failed to redirect stdin\n");
        return -1;
      }
    }
 
    if (write_fd != STDOUT_FILENO){
      if(dup2(write_fd, STDOUT_FILENO) != STDOUT_FILENO){
        fprintf(stderr, "Error: failed to redirect stdout\n");
        return -1;
      }
    }

    if (execvp (arg_list[0], arg_list) == -1) {
      fprintf (stderr, "command could not be found or failed to execute\n");
      abort(); 
    }
  } else {
    /*
     * the parent waits for the child to complete so that
     * the printing order looks correct
     */
    int return_status;    
    waitpid(child_pid, &return_status, 0);
    return child_pid; 
  } 
} 

typedef struct {
  char *command_list[100][100];
  int length;
} pipe_chain;

/*
 * copies portion of stdin to correct location in command list
 */
void copy_cmd(char* buf, int start, int end, char** command_list) {
  // allocate enough room for string and null termination
  *command_list = (char*)malloc(sizeof(char) * (end-start+1));
  memcpy(*command_list, buf+start, end-start);
}

/*
 * prints prompt and parses user input into tokens
 * of commands and arguments.
 *
 * returns NULL if parse fails.
 */
pipe_chain* get_input() {
  pipe_chain* pc = (pipe_chain*)malloc(sizeof(pipe_chain));

  /*
   * read from input
   */
  printf("OS$ ");
  char buf[1000];
  fgets(buf, 999, stdin);

  int cmd_index = 0;
  int arg_index = 0;

  int i = 0;
  int in_word = 0;
  int word_start = 0;
  int eol = 0;
  int lonely_pipe = 0;


  /*
   * parse input line
   */
  while (!eol) {
    switch (buf[i]) {
      case ' ':
        if (in_word) {
          copy_cmd(buf, word_start, i, &pc->command_list[cmd_index][arg_index]);
          arg_index++;
        }
        in_word = 0;
        break;

      case '|':
        if (in_word) {
          copy_cmd(buf, word_start, i, &pc->command_list[cmd_index][arg_index]);
          arg_index++;
        }
        cmd_index++;
        arg_index = 0;
        in_word = 0;
        lonely_pipe = 1;
        break;

      case 0:
      case '\n':
        if (in_word) {
          copy_cmd(buf, word_start, i, &pc->command_list[cmd_index][arg_index]);
          arg_index++;
        }
        in_word = 0;
        eol = 1;
        break;

      default:
        if (!in_word) {
          word_start = i;
          in_word = 1;
          lonely_pipe = 0;
        }
    }
    i++;
  }

  if (lonely_pipe) {
    return NULL;
  }

  pc->length = cmd_index + 1;

  return pc;
}
 
int main () {

  while (1) {
    /** record stdin and stdout so that they may
     * be restored after the child finished executing
     */
    int regular_stdout = dup(STDOUT_FILENO);
    int regular_stdin = dup(STDIN_FILENO);

    /*
     * parse stdin and create a list
     * of commands and arguments
     */
    pipe_chain* pc = get_input();

    if (pc == NULL) {
      fprintf(stderr, "Error: failed to parse input.\n");
      continue;
    }

    /**
     * iterate through pipeline.
     * create pipes to connect neighboring programs
     */
    int next_read_fd = STDIN_FILENO;
    int pipe_length = pc->length;

    int i;
    for (i = 0; i < pipe_length; i++) {
      int read_fd = next_read_fd;
      int write_fd = STDOUT_FILENO;
      char **command = pc->command_list[i];

      if (pipe_length > 1 && i < pipe_length-1) {
        int fds[2];
        if (pipe(fds) != 0) {
          fprintf(stderr, "Error: unable to pipe command\n");
          return -1;
        }

        next_read_fd = fds[READ_PIPE];
        write_fd = fds[WRITE_PIPE];

      }

      run_command(command, read_fd, write_fd);
      close(write_fd);
    }

    /**
     * restore regular stdin and stdout
     */
    dup2(regular_stdout, STDOUT_FILENO);
    dup2(regular_stdin, STDIN_FILENO);
    close(regular_stdout);
    close(regular_stdin);
  }

  return 0; 
}
