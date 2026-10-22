/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _UTILS_H
#define _UTILS_H

#include "../util/parser/parser.h"


/* Useful macro for handling error codes. */
#define DIE(assertion, call_description)			\
	do {							\
		if (assertion) {				\
			fprintf(stderr, "(%s, %s, %d): ",	\
				__FILE__, __func__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);			\
		}						\
	} while (0)

/**
 * Concatenate parts of the word to obtain the command.
 */
char *get_word(word_t *s);

/**
 * Concatenate command arguments in a NULL terminated list in order to pass
 * them directly to execv.
 */
char **get_argv(simple_command_t *command, int *size);

/**
 * Builds params for a command used by execvp syscall.
 */
char **get_params_string(word_t *verb, word_t *params);

/**
 * Builds a string from a head word by adding all of its parts.
 */
char *build_string_from_parts(word_t *head_word);

#endif /* _UTILS_H */
