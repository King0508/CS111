#ifndef CASH_COMMAND_H_
#define CASH_COMMAND_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Represents a tokenized command input by the user. Do not directly
 * access its fields; instead, use the functions given below.
 */
struct command {
    char *token_buffer;
    size_t *token_offsets;
    size_t num_tokens;
};

/*
 * Reads command from INPUT, writing prompts to OUTPUT unless it is null.
 * Tokenizes the command and populates CMD with it. Returns true on success
 * and false on failure. Caller must deallocate resources allocated in CMD
 * in either case (see command_deallocate below.)
 */
bool prompt_and_read_command(FILE *output, FILE *input, struct command *cmd);

/*
 * Returns the number of tokens in CMD.
 */
static inline size_t command_get_num_tokens(const struct command *cmd) {
    return cmd->num_tokens;
}

/*
 * Returns the token at index INDEX in CMD, as a null-terminated string. The
 * memory for the returned token is internal to CMD, so the caller should not
 * free it.
 */
static inline const char *command_get_token_by_index(const struct command *cmd,
                                                     size_t index) {
    return &cmd->token_buffer[cmd->token_offsets[index]];
}

/*
 * Deallocate all resources internal to CMD so that it can be reused (e.g.,
 * via another call to prompt_and_read_command).
 */
void command_deallocate(struct command *cmd);

/*
 * Prints out all tokens in the command (useful for debugging).
 */
void command_fprint(const struct command *cmd, FILE *output);

#endif