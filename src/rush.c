/*
Felicia Drysdale: Operating Systems Project 1
Section: 001
This is a simple Unix shell. The shell creates a child process that executes the command 
you entered and then prompts for more user input when the child process has finished.
*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Function prototypes
char *read_input(void);
char **parse(char *input);
int execute_with_redirection(char **total_subs);
int execute_command(char **total_subs);
void built_in(char **total_subs);
void clear_search_paths();
void redirection(char **total_subs, char *outputFileName);
int is_only_whitespace(char *str);
char ***parallel_commands(char *input, int *com_count);
int built(char **args);
void erroralert();

#define MAX_CHARS 255
#define MAX_PATHS 100
char *search_paths[MAX_PATHS];

//Function for Main loop that runs until the user enters "exit"
int main(int argc, char *argv[]) {

    if (argc > 1) {
        erroralert();
        return 1;
    }

    char *line;
    char ***subs;
    int command_count;

    // Initialize search paths with /bin
    for (int i = 0; i < MAX_PATHS+1; i++) {
        search_paths[i] = NULL; 
    }
    search_paths[0] = strdup("/bin"); //default

    //While loop to go until command says to exit 
    while (1) {

        //Print shell output
        printf("rush> ");
        fflush(stdout);

        // Read input
        line = read_input();

        // If the line consists of only whitespace, print "rush" again
        if (is_only_whitespace(line)) {
            free(line);
            continue;
        }

        // Parse the input using parallel commands, which then calls rest of commands
        subs = parallel_commands(line, &command_count);

        //Determine processes while looping through commands
        for (int i = 0; i < command_count; i++) {
            if (subs[i] == NULL || subs[i][0] == NULL) {
                continue;
            }
            if (built(subs[i])) {
                built_in(subs[i]);
            } else {
                pid_t pid = fork();
                if (pid == 0) {
                    execute_with_redirection(subs[i]);
                    exit(0);
                } else if (pid < 0) {
                    erroralert(); 
                }
            }
        }

        // Wait for all child processes to complete
        while (command_count-- > 0) {
            wait(NULL);
        }

        //free mem
        free(line);
        for (int i = 0; i < command_count; i++) {
            free(subs[i]);
        }

        free(subs);
    }
}

//Check built in functions 
int built(char **args) {
    if ((strcmp(args[0], "exit") == 0) ||(strcmp(args[0], "cd") ==0) || (strcmp(args[0], "path") == 0)) {
        return 1;
    } else{
        return 0;
    } 
}

//Give error message (save space)
void erroralert() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    fflush(stdout);
    fflush(stderr); 
}

//Handle parallel commands
char ***parallel_commands(char *input, int *com_count) {
    int buffer_size = 50;
    int position = 0;
    char ***commGroups = malloc(buffer_size * sizeof(char**));
    char *command;

    if (!commGroups) {
        erroralert();
        exit(EXIT_FAILURE);
    }

    //Parse through via & symbol, allocate mem
    command = strsep(&input, "&"); 
    while (command != NULL) {
        if (position >= buffer_size) {
            buffer_size += 50;
            commGroups = realloc(commGroups, buffer_size * sizeof(char**));
            if (!commGroups) {
                erroralert();
                exit(EXIT_FAILURE);
            }
        }

        // Parse the individual command group into arguments and store in the array.
        commGroups[position] = parse(command);
        if (!commGroups[position]) {
            // Free allocated memory before returning NULL
            for (int i = 0; i < position; i++) {
                if (commGroups[i]) {
                    free(commGroups[i]);
                }
            }

            //free mem
            free(commGroups);
            erroralert();
            exit(EXIT_FAILURE);
        }
        position++;
        // Continue to the next command group, if any.
        command = strsep(&input, "&");
    }

    *com_count = position;
    return commGroups;
}

//Redirection handling for process
void redirection(char **total_subs, char *outputFileName) {
    // Fork a new process
    pid_t child_pid = fork();

    if (child_pid < 0) {
        erroralert();
        exit(EXIT_FAILURE); 
        return;
    }

    if (child_pid == 0) {
        // Child process

        //Open the output file for writing and redirect stdout to the file
        int redirect_comms = open(outputFileName, O_CREAT | O_TRUNC | O_WRONLY);
        dup2(redirect_comms, STDOUT_FILENO);

        //clean up process table
        close(redirect_comms);

        // Execute the command with the redirected output
        if (execvp(total_subs[0], total_subs) == -1) {
            erroralert();
            exit(EXIT_FAILURE); 
        }
        
    } else {
        // Parent process
        // Wait for the child process to finish
        waitpid(child_pid, NULL, 0);
    }
}

//Function to read lines of input prior to parse
char *read_input(void) {

    size_t buffer = 0;
    ssize_t read_line;
    char *line = NULL;

    //Allocates memory
    read_line = getline(&line, &buffer, stdin);

    //Read line gets error
    if (read_line == -1) {
        erroralert();
    }

    //If over char limit
    if (read_line > MAX_CHARS + 1) {
        erroralert();;
    }

    return line;
}

//Function to tokenize input string based on specified delimiter(s)
char **parse(char *input) {

    char *token;
    char **total_subs = malloc(MAX_CHARS * sizeof(char *));
    int count = 0;

    //Delim is spaces
    const char *delim = " \t\n";

    if (total_subs == NULL || input == NULL) {
        // Allocation failed or input is NULL
        return NULL;
    }

    //Main loop to tokenize input string
    while ((token = strsep(&input, delim)) != NULL) {
        //Skip empty tokens
        if (*token != '\0') {
            // Allocate memory for the token
            total_subs[count] = malloc(strlen(token) + 1);
            if (total_subs[count] == NULL) {
                // Memory allocation failed
                // Clean up allocated memory before returning NULL
                for (int i = 0; i < count; i++) {
                    free(total_subs[i]);
                }
                free(total_subs);
                return NULL;
            }

            // Dynamically reallocate memory for total_subs if needed
            if (count >= MAX_CHARS) {
                total_subs = realloc(total_subs, (count + 1) * sizeof(char *));
                if (!total_subs) {
                    // Memory reallocation failed, handle error
                    // Free allocated memory and return NULL
                    for (int i = 0; i < count; i++) {
                        free(total_subs[i]);
                    }
                    free(total_subs);
                    return NULL;
                }
            }

            // Copy the token to the allocated memory
            strcpy(total_subs[count], token);
            count++;
        }
    }

    // Null-terminate the array of tokens
    total_subs[count] = NULL;
    return total_subs;
}


// Function to execute non-built-in commands
int execute_with_redirection(char **total_subs) {
    // Check if output redirection is requested
    int need_redirection = 0;
    char *outputFileName = NULL;
    int i = 0;
    int carrots = 0;

    //Loop to check through commands
    while (total_subs[i] != NULL) {
        if (strcmp(total_subs[i], ">") == 0) {
             carrots += 1;

            //Check if there was already a redirection (we cant have two)
            if (carrots > 1 || total_subs[i + 1] == NULL || total_subs[i + 2] != NULL) {
                erroralert();
                exit(EXIT_FAILURE);
            }

            // Output redirection requested
            need_redirection = 1;
            outputFileName = total_subs[i + 1];
            // Remove redirection symbols from total_subs
            total_subs[i] = NULL;
            total_subs[i + 1] = NULL;
            break;

        }
        i++;
    }

    if (need_redirection) {
        // Redirect output and execute command
        redirection(total_subs, outputFileName);
    } else {
        // No output redirection requested, execute command normally
        return execute_command(total_subs);
    }

    return 1;
}

// Function to execute a command without output redirection
int execute_command(char **total_subs) {
    pid_t child_pid = fork();

    if (child_pid < 0) {
        // Error handling
        erroralert();
        return 0;
    }

    if (child_pid == 0) {
        // Child process

        //Make path
        char fullPath[MAX_CHARS];
        int found = 0;

        //Loop and construct the path using snprintf
        for (int i = 0; search_paths[i] != NULL && !found; i++) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", search_paths[i], total_subs[0]);

            // Check if the constructed path points to an executable file.
            if (access(fullPath, X_OK) == 0) {
                found = 1;
                break; 
            }
        }

        //If no executable was not found, error
        if (!found) {
            erroralert(); 
            exit(EXIT_FAILURE);
        }

        //New process
        if (execv(fullPath, total_subs) == -1) {
            erroralert(); 
            exit(EXIT_FAILURE); 
        }
    } else {
        // Parent process
        waitpid(child_pid, NULL, 0);
    }

    return 1;
}

//Function for built in commands
void built_in(char **total_subs) {

    int num_commands  = 3;
    char *commands[num_commands];
    int index = -1;

    //built in commands
    commands[0] = "exit";
    commands[1] = "cd";
    commands[2] = "path";

    //Checks each command to see if it exists within the user input
    //Command will always be first index of array subs 
    for (int i=0; i < num_commands; i++) {
        if (strcmp(total_subs[0], commands[i]) == 0) {
            index = i;
            break;
        }
    }

    //What each command does
    if (index == 0)  {
        //Check if exit is followed by anything (then it is error)
        if (total_subs[1] == NULL) {
            exit(0);     
            exit(EXIT_FAILURE);   
        } else {
            erroralert();
        }

    } else if (index == 1)  {
        // Check if there is exactly one argument after "cd"
        //CHECK THIS LATER maybe NULL is an issue
        if (total_subs[1] == NULL || total_subs[2] != NULL) {
            erroralert();
        } else {
            //chdir must return 0 to be valid
            if ((chdir(total_subs[1])) == -1) {
                erroralert();
                //Change directory
                chdir(total_subs[1]);
                //printf("Changed directory to %s\n", total_subs[1]);
            }
        }

    } else if (index == 2)  {
        //If search path is empty, only can use built in commands
        if (total_subs[1] != NULL) {
            //overwrites, so clear path
            clear_search_paths();
            int i;
            for (i = 1; total_subs[i] != NULL && i - 1 < MAX_PATHS; i++) {
                // Allocate memory for the new path
                search_paths[i - 1] = malloc(strlen(total_subs[i]) + 1);

                // Check if memory allocation is successful
                if (search_paths[i - 1] != NULL) {
                    // Copy the path to the allocated memory
                    strcpy(search_paths[i - 1], total_subs[i]);
                } else {
                    // Memory allocation failed, handle error
                    // Free previously allocated memory before returning
                    clear_search_paths();
                    erroralert();
                    return;
                }
            }
            // Set the next element to NULL to mark the end of the list
            search_paths[i - 1] = NULL;
        } else {
            erroralert();
        }


    } else {
        // External command, not a built-in one
        if (execute_command(total_subs) != 1) {
            erroralert();
        }
    }

}

//Function to clear search paths because path always overwrites
void clear_search_paths() {

    for (int i = 0; i < MAX_PATHS; i++) {
        // Free the memory for each search path
        free(search_paths[i]);

        // Set the entry to NULL to mark it as empty
        search_paths[i] = NULL;
    }

}

// Function to check if a string consists of only whitespace characters
int is_only_whitespace(char *str) {
    while (*str != '\0') {
        if (!isspace((unsigned char)*str)) {
            return 0; 
        }
        str++;
    }
    return 1;
}