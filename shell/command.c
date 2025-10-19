#define _GNU_SOURCE

#include "command.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static size_t expand_capacity(size_t capacity) {
    if (capacity == 0) {
        return 8;
    } else {
        return capacity * 2;
    }
}

void command_deallocate(struct command *cmd) {
    free(cmd->token_buffer);
    free(cmd->token_offsets);
}

enum tokenizer_quote_state {
    TOKENIZER_QUOTE_STATE_NORMAL,
    TOKENIZER_QUOTE_STATE_IN_SINGLE_QUOTE,
    TOKENIZER_QUOTE_STATE_IN_DOUBLE_QUOTE
};

bool prompt_and_read_command(FILE *output, FILE *input, struct command *cmd) {
    bool success = false;

    cmd->token_buffer = NULL;
    cmd->token_offsets = NULL;
    cmd->num_tokens = 0;

    size_t token_buffer_idx = 0;
    size_t current_token_offset = 0;
    size_t token_buffer_capacity = 0;
    size_t tokens_capacity = 0;

    enum tokenizer_quote_state quote_state = TOKENIZER_QUOTE_STATE_NORMAL;
    bool in_escape = false;

    char *line = NULL;
    size_t line_allocation_length;

    do {
        if (output != NULL) {
            if (tokens_capacity == 0) {
                fprintf(output, "cash$$$$ ");
            } else {
                fprintf(output, "........ ");
            }
            fflush(output);
        }

        ssize_t line_length = getline(&line, &line_allocation_length, input);
        if (line_length < 0) {
            if (feof(input)) {
                if (output != NULL) {
                    fprintf(output, "\n");
                }
            } else {
                perror("[cash] getline");
            }
            goto out;
        }

        /*
         * getline() many not include a trailing newline if we are at EOF; we
         * add one here to remove edge cases later.
         */
        if (line[line_length - 1] != '\n') {
            if (line_allocation_length < ((size_t) line_length) + 2) {
                line_allocation_length = ((size_t) line_length) + 2;
                char *new_line = realloc(line, line_allocation_length);
                if (new_line == NULL) {
                    fprintf(stderr, "[cash] out of memory\n");
                    goto out;
                }
                line = new_line;
            }
            line[line_length] = '\n';
            line_length++;
            line[line_length] = '\0';
        }

        in_escape = false;
        for (size_t i = 0; i != (size_t) line_length; i++) {
            /* Make sure we have enough space to record the next character. */
            if (token_buffer_idx == token_buffer_capacity) {
                token_buffer_capacity = expand_capacity(token_buffer_capacity);
                char *new_token_buffer = reallocarray(
                    cmd->token_buffer, token_buffer_capacity, sizeof(char));
                if (new_token_buffer == NULL) {
                    fprintf(stderr, "[cash] out of memory\n");
                    goto out;
                }
                cmd->token_buffer = new_token_buffer;
            }

            switch (quote_state) {
            case TOKENIZER_QUOTE_STATE_NORMAL:
                if (isspace(line[i])) {
                    size_t current_token_length =
                        current_token_offset - token_buffer_idx;
                    if (current_token_length != 0) {
                        cmd->token_buffer[token_buffer_idx++] = '\0';

                        /* Save the current token. */
                        size_t token_idx = cmd->num_tokens++;
                        if (token_idx == tokens_capacity) {
                            tokens_capacity = expand_capacity(tokens_capacity);
                            size_t *new_token_offsets =
                                reallocarray(cmd->token_offsets,
                                             tokens_capacity, sizeof(size_t));
                            if (new_token_offsets == NULL) {
                                fprintf(stderr, "[cash] out of memory\n");
                                goto out;
                            }
                            cmd->token_offsets = new_token_offsets;
                        }
                        cmd->token_offsets[token_idx] = current_token_offset;
                    }
                    current_token_offset = token_buffer_idx;
                } else if (line[i] == '\\') {
                    if (in_escape) {
                        cmd->token_buffer[token_buffer_idx++] = '\\';
                        in_escape = false;
                    } else {
                        in_escape = true;
                    }
                } else if (line[i] == '\'') {
                    if (in_escape) {
                        cmd->token_buffer[token_buffer_idx++] = '\'';
                        in_escape = false;
                    } else {
                        quote_state = TOKENIZER_QUOTE_STATE_IN_SINGLE_QUOTE;
                    }
                } else if (line[i] == '"') {
                    if (in_escape) {
                        cmd->token_buffer[token_buffer_idx++] = '"';
                        in_escape = false;
                    } else {
                        quote_state = TOKENIZER_QUOTE_STATE_IN_DOUBLE_QUOTE;
                    }
                } else {
                    cmd->token_buffer[token_buffer_idx++] = line[i];
                }
                break;
            case TOKENIZER_QUOTE_STATE_IN_SINGLE_QUOTE:
                if (line[i] == '\'') {
                    quote_state = TOKENIZER_QUOTE_STATE_NORMAL;
                } else if (line[i] != '\\') {
                    cmd->token_buffer[token_buffer_idx++] = line[i];
                }
                break;
            case TOKENIZER_QUOTE_STATE_IN_DOUBLE_QUOTE:
                if (line[i] == '"') {
                    quote_state = TOKENIZER_QUOTE_STATE_NORMAL;
                } else if (line[i] != '\\') {
                    cmd->token_buffer[token_buffer_idx++] = line[i];
                }
            }
        }
    } while (quote_state != TOKENIZER_QUOTE_STATE_NORMAL || in_escape);

    /* getline() includes final newline, so no need to record last token. */
    success = true;

out:
    free(line);
    return success;
}