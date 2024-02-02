/*
  utcsh - The UTCS Shell

  <Put your name and CS login ID here>
*/

/* Read the additional functions from util.h. They may be beneficial to you
in the future */
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

/* Global variables */
/* The array for holding shell paths. Can be edited by the functions in util.c*/
char shell_paths[MAX_ENTRIES_IN_SHELLPATH][MAX_CHARS_PER_CMDLINE];
static char prompt[] = "utcsh> "; /* Command line prompt */
static char *default_shell_path[2] = {"/bin", NULL};
/* End Global Variables */

/* Convenience struct for describing a command. Modify this struct as you see
 * fit--add extra members to help you write your code. */
struct Command
{
  char **args;      /* Argument array for the command */
  char *outputFile; /* Redirect target for file (NULL means no redirect) */
  int redirectionError;
};

/* Here are the functions we recommend you implement */

char **tokenize_command_line (char *cmdline);
struct Command parse_command (char **tokens);
void eval (struct Command *cmd);
int try_exec_builtin (struct Command *cmd);
void exec_external_cmd (struct Command *cmd);

void handle_error();
int simple_cmd(struct Command *cmd);
char** split_concurrency(char* line);
void run(char* line);
char **free_args(char **arguments);

/* Main REPL: read, evaluate, and print. This function should remain relatively
   short: if it grows beyond 60 lines, you're doing too much in main() and
   should try to move some of that work into other functions. */
int main (int argc, char **argv)
{  
  set_shell_path (default_shell_path);  
  size_t linecap = 0;
  char* line = NULL;
  FILE* stream = NULL;
  bool script = false;
  if (argc == 2) {
    stream = fopen(argv[1], "r"); //derived from getline manpage
    if (stream != NULL) {
      script = true; 
    } 
    else {
      handle_error();
      exit(EXIT_FAILURE);
    }
  }
  else if (argc > 2) {
    handle_error();
    exit(EXIT_FAILURE);
  }
  else {
    stream = stdin;
  }

  int countContents = 0;
  while (1)
    {
      if (!script) {
        printf ("%s", prompt);
      }

      /* Read */
      if(getline(&line, &linecap, stream) == -1) {
        //handle fail
        if (errno == EINTR) {
          exit(EXIT_FAILURE);
        }
        if (feof(stream)) {
          if (countContents == 0) {
            handle_error();
            exit(EXIT_FAILURE);
          }
          break;
        }
        else {
          exit(EXIT_FAILURE);
        }
      }
      countContents += 1;
      run(line);
    }
    free(line);
    if (script) {
      fclose(stream);
    }
  return 0;
}

void run(char* line) {
  char** command_strings = split_concurrency(line);
  int num_commands = 0;
  for (int i = 0; command_strings[i] != NULL; i++) {
      num_commands++;
  }
  pid_t* pids = (pid_t*)malloc(num_commands * sizeof(pid_t));
  if (!pids) {
    exit(EXIT_FAILURE);
  }
  int parent = 0;
  for (int i = 0; command_strings[i] != NULL; i++) {
    struct Command command_to_eval = 
    parse_command(tokenize_command_line(command_strings[i]));
    if (command_to_eval.redirectionError) {
        handle_error();
    } else {
        if (simple_cmd(&command_to_eval)) {
            eval(&command_to_eval);  
        } else {
            pid_t pid = fork();
            if (pid == 0) {
                exec_external_cmd(&command_to_eval);
                exit(0); 
            } else if (pid > 0) {
                pids[parent++] = pid; 
            } else {
                handle_error();
                exit(EXIT_FAILURE);
            }
        }
    }
  }
  free(command_strings);
  for (int i = 0; i < num_commands; i++) {
    if (waitpid(pids[i], NULL, 0) == 0) {
      handle_error();
      exit(EXIT_FAILURE);
    }
    
  }
  free(pids);
}

int simple_cmd(struct Command *cmd) {
    if (cmd == NULL || cmd->args[0] == NULL) {
        return 0; 
    }
    char *command = cmd->args[0];
    if (strcmp(command, "exit") == 0 ||
        strcmp(command, "cd") == 0 ||
        strcmp(command, "path") == 0) {
        return 1; 
    }
    return 0;
}

char** split_concurrency(char *line) {
    int num_commands = 1;
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '&') num_commands++;
    }
    char **commands = malloc((num_commands + 1) * sizeof(char*));
    if (!commands) {
        exit(EXIT_FAILURE);
    }
    int index = 0;
    char *token = strtok(line, "&");
    while (token != NULL) {
        int empty_space = 1;
        for (int i = 0; token[i] != '\0'; i++) {
            char c = token[i];
            if (c != ' ' && c != '\t' && c != '\n' && 
            c != '\r' && c != '\f' && c != '\v'){
                empty_space = 0;
                break;
            }
        }
        if (!empty_space) {
            commands[index++] = token;
        }
        token = strtok(NULL, "&");
    }
    commands[index] = NULL; 
    return commands;
}

/* NOTE: In the skeleton code, all function bodies below this line are dummy
implementations made to avoid warnings. You should delete them and replace them
with your own implementation. */

/** Turn a command line into tokens with strtok
 *
 * This function turns a command line into an array of arguments, making it
 * much easier to process. First, you should figure out how many arguments you
 * have, then allocate a char** of sufficient size and fill it using strtok()
 */
char **tokenize_command_line (char *cmdline)
{  
  int length = strlen(cmdline);
  if (cmdline[length - 1] == '\n') {
    cmdline[length - 1] = '\0';
  }  
  //loop through the string and count the amount of tokens
  const char *sep = " \t\n";
  //Allocate memory for the argument char** array
  char **arguments = malloc((MAX_WORDS_PER_CMD + 1) * sizeof(char*));
  if (!arguments) {
    exit(EXIT_FAILURE);
  }
  //Tokenize it and fill the char array
  arguments[MAX_WORDS_PER_CMD] = NULL;
  char* final_tok = strtok(cmdline, sep);
  char** tmp_ptr = arguments;
  while (final_tok != NULL) {    
    *tmp_ptr = final_tok;
    tmp_ptr++;
    if (tmp_ptr > arguments + MAX_WORDS_PER_CMD) {
      handle_error();
      free(arguments);
      exit(EXIT_FAILURE);
    }
    final_tok = strtok(NULL, sep);
  }  
  return arguments;
}

/** Turn tokens into a command.
 *
 * The `struct Command` represents a command to execute. This is the preferred
 * format for storing information about a command, though you are free to change
 * it. This function takes a sequence of tokens and turns them into a struct
 * Command.
 */
struct Command parse_command (char **tokens)
{  
  struct Command *dummy = malloc(sizeof(struct Command));
  if (!dummy) {
    exit(EXIT_FAILURE);
  }
  dummy->args = tokens;
  dummy->outputFile = NULL;
  dummy->redirectionError = 0;  

  int redirectionCount = 0;
  int i = 0;
  int trackEverythingElse = 0;

  while (tokens[i] != NULL) {
    if (strcmp(tokens[i], ">") == 0) {
      if (i == 0) {
        dummy->redirectionError = 1;
        return *dummy;
      }
      redirectionCount++;
      if (redirectionCount > 1) {
          dummy->redirectionError = 1;
          break; 
      }
      if (tokens[i + 1] != NULL) {
        if (tokens [i + 2] != NULL) {
          dummy->redirectionError = 1;
          return *dummy;
        }
        dummy->outputFile = tokens[i + 1];
        i += 2; 
        continue;
      } 
      else {
          //No file
          dummy->redirectionError = 1;
          break; 
      }
    } 
    else {
        // Not > case
        tokens[trackEverythingElse++] = tokens[i++];
    }
  }
  if (!dummy->redirectionError) {
      tokens[trackEverythingElse] = NULL;
  }
  return *dummy;
}

/** Evaluate a single command
 *
 * Both built-ins and external commands can be passed to this function--it
 * should work out what the correct type is and take the appropriate action.
 */
void eval (struct Command *cmd)
{
  if (cmd == NULL) {
    return;
  }
  char* argument = cmd->args[0];
  if (argument == NULL) {    
    return;
  }  
  if(strcmp(argument, "exit") == 0) {
    if (cmd->args[1] != NULL) {      
      handle_error();
    }
    else {
      exit(0);
    }
  }
  else if (strcmp(argument, "cd") == 0) {
    if (cmd->args[2] != NULL || cmd->args[1] == NULL) {      
      handle_error();
    }
    else {
      int success = chdir(cmd->args[1]);
      if (success != 0) {        
        handle_error();
      }
    }
  }
  else if (strcmp(argument, "path") == 0) {
    int success = set_shell_path(cmd->args + 1);
    if (success == 0) {
      exit(EXIT_FAILURE);
    }
  }
  else {
    exec_external_cmd(cmd);
  }
}

void handle_error() {
  char emsg[30] = "An error has occurred\n";
  int nbytes_written = write(STDERR_FILENO, emsg, strlen(emsg)); 
  if(nbytes_written != (int)strlen(emsg)){
    exit(EXIT_FAILURE);  // Shouldn't really happen -- if it does, error is unrecoverable
  }  
}

/** Execute built-in commands
 *
 * If the command is a built-in command, execute it and return 1 if appropriate
 * If the command is not a built-in command, do nothing and return 0
 */
int try_exec_builtin (struct Command *cmd)
{
  (void) cmd;
  return 0;
}

/** Execute an external command
 *
 * Execute an external command by fork-and-exec. Should also take care of
 * output redirection, if any is requested
 */
void exec_external_cmd (struct Command *cmd)
{
  char *command = cmd->args[0];
  char *real_path = NULL;

  // Check if the command is an absolute path
  if (!is_absolute_path(cmd->args[0])) {
    //printf("%c\n", shell_paths[1][1]);
    for (int i = 0; *shell_paths[i] != '\0'; i++) {
      real_path = exe_exists_in_dir(shell_paths[i], command, false);
      if (real_path != NULL) {
        break;
      }
    }
  }
  else {
    real_path = command;
  }

  if (real_path == NULL) {
    handle_error();
  }
  else {
    pid_t pid = fork();
    if (pid == -1) {
      handle_error();
      exit(EXIT_FAILURE);
      return;
    }
    if (pid == 0) {
      if (cmd->outputFile != NULL) {
        int file_descriptor = 
        open(cmd->outputFile, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        if (file_descriptor == -1) {
          handle_error();
          exit(EXIT_FAILURE);
        } 
        else if (dup2(file_descriptor, STDOUT_FILENO) == -1) {
          handle_error();
          exit(EXIT_FAILURE);
        } 
        else if (dup2(file_descriptor, STDERR_FILENO) == -1) {
          handle_error();
          exit(EXIT_FAILURE);
        } 
        if (close(file_descriptor) == -1) {
          handle_error();
          exit(EXIT_FAILURE);
        }
      }
      if (execv(real_path, cmd->args) == -1) {
          handle_error();
          exit(EXIT_FAILURE);
      }
    } 
    else {
      int status;
      if (waitpid(pid, &status, 0) == -1) {
        handle_error();
        exit(EXIT_FAILURE);
      }      
    }
  }
}

