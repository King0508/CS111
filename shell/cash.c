#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "command.h"

extern char **environ;
bool shell_is_interactive = true;

// Simple linked list to track background job PIDs
typedef struct bg_job {
    pid_t pid;
    struct bg_job *next;
} bg_job_t;

bg_job_t *bg_jobs = NULL;

// Add a background job to the list
static void add_bg_job(pid_t pid) {
    bg_job_t *job = malloc(sizeof(bg_job_t));
    if (job == NULL) {
        perror("malloc");
        return;
    }
    job->pid = pid;
    job->next = bg_jobs;
    bg_jobs = job;
}

// Wait for all background jobs to complete
static void wait_all_bg_jobs(void) {
    while (bg_jobs != NULL) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid <= 0) {
            break;
        }
        // Remove completed job from list
        bg_job_t **curr = &bg_jobs;
        while (*curr) {
            if ((*curr)->pid == pid) {
                bg_job_t *to_free = *curr;
                *curr = (*curr)->next;
                free(to_free);
                break;
            }
            curr = &(*curr)->next;
        }
    }
}

static void print_usage(void) {
    printf(u8"\U0001F309 \U0001F30A \U00002600\U0000FE0F "
           u8"cash: The California Shell "
           u8"\U0001F334 \U0001F43B \U0001F3D4\U0000FE0F\n");
    printf("Usage: cash [script.sh]\n");
    printf("\n");
    printf("Built-in commands:\n");
    printf("help: Print out this usage information.\n");
    printf("exit <code>: Exit the shell with optional exit code.\n");
    printf("cd <path>: Change directory (no path = home).\n");
    printf("pwd: Print working directory.\n");
    printf("wait: Wait for all background jobs to complete.\n");
    printf("\n");
}

// Resolve program path using PATH environment variable
static char *resolve_path(const char *program) {
    if (program == NULL) {
        return NULL;
    }

    // If program contains '/', it's already a path
    if (strchr(program, '/') != NULL) {
        return strdup(program);
    }

    // Get PATH environment variable
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return strdup(program);
    }

    // Make a copy of PATH since strtok modifies it
    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        return NULL;
    }

    char *save_ptr = NULL;
    char *dir = strtok_r(path_copy, ":", &save_ptr);
    while (dir != NULL) {
        // Build full path: dir + "/" + program
        size_t len = strlen(dir) + 1 + strlen(program) + 1;
        char *full_path = malloc(len);
        if (full_path == NULL) {
            free(path_copy);
            return NULL;
        }

        snprintf(full_path, len, "%s/%s", dir, program);

        // Check if file exists and is executable
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }

        free(full_path);
        dir = strtok_r(NULL, ":", &save_ptr);
    }

    free(path_copy);
    return strdup(program); // Return original if not found
}

// Setup signal handlers for the shell
static void setup_signal_handlers(void) {
    if (!shell_is_interactive) {
        return;
    }

    // Ignore signals that should not affect the shell
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

// Reset signal handlers to default (for child processes)
static void reset_signal_handlers(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

// Spawn a process to execute a command
static void spawn_process(const struct command *cmd) {
    size_t num_tokens = command_get_num_tokens(cmd);
    if (num_tokens == 0) {
        return;
    }

    // Build argv array - just copy pointers, no allocation needed for strings
    char **argv = malloc((num_tokens + 1) * sizeof(char *));
    if (argv == NULL) {
        perror("malloc");
        return;
    }

    size_t argv_idx = 0;
    char *input_file = NULL;
    char *output_file = NULL;
    bool background = false;

    // Parse tokens
    for (size_t i = 0; i < num_tokens; i++) {
        const char *token = command_get_token_by_index(cmd, i);

        if (strcmp(token, "<") == 0 && i + 1 < num_tokens) {
            input_file = (char *) command_get_token_by_index(cmd, i + 1);
            i++;
        } else if (strcmp(token, ">") == 0 && i + 1 < num_tokens) {
            output_file = (char *) command_get_token_by_index(cmd, i + 1);
            i++;
        } else if (strcmp(token, "&") == 0) {
            background = true;
        } else {
            argv[argv_idx++] = (char *) token;
        }
    }
    argv[argv_idx] = NULL;

    if (argv_idx == 0) {
        free(argv);
        return;
    }

    // Resolve program path
    char *program = resolve_path(argv[0]);
    if (program == NULL) {
        fprintf(stderr, "%s: command not found\n", argv[0]);
        free(argv);
        return;
    }

    // Fork
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        free(program);
        free(argv);
        return;
    } else if (pid == 0) {
        // CHILD PROCESS

        // Reset signal handlers to default
        reset_signal_handlers();

        // Create new process group
        setpgid(0, 0);

        // Give terminal control to foreground job
        if (!background && shell_is_interactive) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }

        // Handle input redirection
        if (input_file != NULL) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror(input_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Handle output redirection
        if (output_file != NULL) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror(output_file);
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Execute
        execve(program, argv, environ);

        // If execve returns, it failed
        perror(program);
        exit(EXIT_FAILURE);
    } else {
        // PARENT PROCESS

        // Set child's process group
        setpgid(pid, pid);

        if (background) {
            // Background job - add to tracking list
            add_bg_job(pid);
        } else {
            // Foreground job - give it terminal control and wait
            if (shell_is_interactive) {
                tcsetpgrp(STDIN_FILENO, pid);
            }

            int status;
            waitpid(pid, &status, 0);

            // Take back terminal control
            if (shell_is_interactive) {
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }

        // Clean up
        free(program);
        free(argv);
    }
}

static bool handle_builtin_command(const struct command *cmd) {
    const char *first_token = command_get_token_by_index(cmd, 0);
    size_t num_tokens = command_get_num_tokens(cmd);

    // Help command
    if (strcmp(first_token, "help") == 0) {
        print_usage();
        return true;
    }
    // Exit command
    else if (strcmp(first_token, "exit") == 0) {
        int exit_code = 0;
        if (num_tokens > 1) {
            exit_code = atoi(command_get_token_by_index(cmd, 1));
        }
        exit(exit_code);
    }
    // PWD command
    else if (strcmp(first_token, "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("getcwd");
        }
        return true;
    }
    // CD command
    else if (strcmp(first_token, "cd") == 0) {
        const char *path;

        if (num_tokens == 1) {
            path = getenv("HOME");
            if (path == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return true;
            }
        } else {
            path = command_get_token_by_index(cmd, 1);
        }

        if (chdir(path) != 0) {
            perror("cd");
        }
        return true;
    }
    // Wait command - wait for all background jobs
    else if (strcmp(first_token, "wait") == 0) {
        wait_all_bg_jobs();
        return true;
    } else {
        return false;
    }
}

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        print_usage();
        return EXIT_FAILURE;
    }

    FILE *input_stream = stdin;
    FILE *output_stream = stdout;
    if (argc == 2) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
        shell_is_interactive = false;
    }
    if (!isatty(STDIN_FILENO)) {
        shell_is_interactive = false;
    }
    if (!shell_is_interactive) {
        output_stream = NULL;
    }

    // Setup signal handlers
    setup_signal_handlers();

    struct command cmd;
    while (prompt_and_read_command(output_stream, input_stream, &cmd)) {
        if (command_get_num_tokens(&cmd) > 0) {
            if (!handle_builtin_command(&cmd)) {
                spawn_process(&cmd);
            }
        }
        command_deallocate(&cmd);
    }
    command_deallocate(&cmd);

    fclose(input_stream);

    return EXIT_SUCCESS;
}
