/**
 * Reid Horuff
 * 3-30-2015
 *
 * compile with $./make.sh
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
#include <sys/wait.h>

typedef int bool;
#define true 1
#define false 0

#define WRITE_PIPE 1
#define READ_PIPE 0

#define INPUT_BUFFER_LEN 1000
 
void run_command(char** arg_list, int read_fd, int write_fd) {
  pid_t child_pid; 
 
  child_pid = fork(); 
 
  if (child_pid == 0){
    if (read_fd != STDIN_FILENO){
      if(dup2(read_fd, STDIN_FILENO) != STDIN_FILENO){
        fprintf(stderr, "Error: failed to redirect stdin\n");
        return;
      }
    }
 
    if (write_fd != STDOUT_FILENO){
      if(dup2(write_fd, STDOUT_FILENO) != STDOUT_FILENO){
        fprintf(stderr, "Error: failed to redirect stdout\n");
        return;
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
  } 
} 

typedef struct {
  char *command_list[100][100];
  int length;
} pipe_chain;

/**
 * strings were allocated along command_list
 * and must be manually freed before pipe_chain
 * can be freed.
 */
void free_pipe_chain(pipe_chain* pc) {
  int i, j;
  for (i = 0; i < pc->length; i++) {
    for (j = 0; pc->command_list[i][j]; j++) {
      free(pc->command_list[i][j]);
    }
  }
  free(pc);
}

/*
 * copies portion of stdin to correct location in command list
 */
void copy_cmd(char* buf, int start, int end, char** command_list) {
  // allocate enough room for string and null termination
  *command_list = (char*)calloc(end-start+1, sizeof(char));
  memcpy(*command_list, buf+start, end-start);
}

/*
 * prints prompt and parses user input into tokens
 * of commands and arguments.
 *
 * returns NULL if parse fails.
 */
pipe_chain* parse_input(char buf[]) {
  pipe_chain* pc = (pipe_chain*)malloc(sizeof(pipe_chain));

  int cmd_index = 0;
  int arg_index = 0;

  int i = 0;

  bool in_word = false;
  bool eol = false;
  bool lonely_pipe = false;
  int word_start = 0;

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
        in_word = false;
        break;

      case '|':
      case '>':
        if (in_word) {
          copy_cmd(buf, word_start, i, &pc->command_list[cmd_index][arg_index]);
          arg_index++;
        }
        cmd_index++;
        arg_index = 0;
        in_word = false;
        lonely_pipe = true;
        break;

      case 0:
      case '\n':
        if (in_word) {
          copy_cmd(buf, word_start, i, &pc->command_list[cmd_index][arg_index]);
          arg_index++;
        }
        in_word = false;
        eol = 1;
        break;

      default:
        if (!in_word) {
          word_start = i;
          in_word = true;
          lonely_pipe = false;
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

  /*
   * input buffer
   */
  char buf[INPUT_BUFFER_LEN];

  while (true) {
    /** 
     * record stdin and stdout so that they may
     * be restored after the child finished executing
     */
    int regular_stdout = dup(STDOUT_FILENO);
    int regular_stdin = dup(STDIN_FILENO);

    /*
     * print prompt and read stdin
     */
    printf("os$ ");
    fgets(buf, INPUT_BUFFER_LEN, stdin);
    if (strcmp(buf, "exit\n") == 0) {
      exit(1);
    } 

    /*
     * parse input 
     */
    pipe_chain* pc = parse_input(buf);
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

      /*
       * if there are multiple programs and we're not executing the last one.
       * then:
       * Create pipe, execute current program with stdout=pipe intput,
       * set pipe output to be next stdin.
       */
      if (pipe_length > 1 && i < pipe_length-1) {
        int _pipe[2];
        if (pipe(_pipe) != 0) {
          fprintf(stderr, "Error: unable to create pipe.\n");
          return -1;
        }

        next_read_fd = _pipe[READ_PIPE];
        write_fd = _pipe[WRITE_PIPE];

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

    free_pipe_chain(pc);
  }

  return 0; 
}
