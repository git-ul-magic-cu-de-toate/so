// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

/**
 * Concatenate parts of the word to obtain the command.
 */
char *get_word(word_t *s)
{
	char *string = NULL;
	int string_length = 0;

	const char *substring = NULL;
	int substring_length = 0;

	while (s != NULL) {
		if (s->expand == true) {
			substring = getenv(s->string);

			/* Prevents strlen from failing. */
			if (substring == NULL)
				substring = "";
		} else {
			substring = s->string;
		}

		substring_length = strlen(substring);

		string = realloc(string, string_length + substring_length + 1);
		DIE(string == NULL, "Error allocating word string.");

		string[string_length] = '\0';
		strcat(string, substring);

		string_length += substring_length;

		s = s->next_part;
	}

	return string;
}

/**
 * Concatenate command arguments in a NULL terminated list in order to pass
 * them directly to execv.
 */
char **get_argv(simple_command_t *command, int *size)
{
	char **argv;
	int argc;

	word_t *param;

	argc = 1;

	/* Get parameters number. */
	param = command->params;
	while (param != NULL) {
		param = param->next_word;
		argc++;
	}

	argv = calloc(argc + 1, sizeof(char *));
	DIE(argv == NULL, "Error allocating argv.");

	argv[0] = get_word(command->verb);
	DIE(argv[0] == NULL, "Error retrieving word.");

	param = command->params;
	argc = 1;
	while (param != NULL) {
		argv[argc] = get_word(param);
		DIE(argv[argc] == NULL, "Error retrieving word.");

		param = param->next_word;
		argc++;
	}

	*size = argc;

	return argv;
}

/**
 * Builds a string from a head word by adding all of its parts.
 */
char *build_string_from_parts(word_t *head_word)
{
	char *string = NULL;
	int parts_length = 0;

	// count the size of the actual string
	word_t *curr_word = head_word;

	while (curr_word) {
		if (curr_word->expand) {
			const char *temp_val = getenv(curr_word->string);

			if (temp_val)
				parts_length += strlen(temp_val);
		} else {
			parts_length += strlen(curr_word->string);
		}
		curr_word = curr_word->next_part;
	}

	// alocate the string
	if (!parts_length)
		return NULL;

	string = calloc((parts_length + 1), sizeof(*string));
	DIE(string == NULL, "error allocating string");

	// build the string
	curr_word = head_word;
	while (curr_word) {
		if (curr_word->expand) {
			const char *temp_val = getenv(curr_word->string);

			if (temp_val)
				strcat(string, temp_val);
		} else {
			strcat(string, curr_word->string);
		}
		curr_word = curr_word->next_part;
	}

	return string;
}

/**
 * Builds params for a command used by execvp syscall.
 */
char **get_params_string(word_t *verb, word_t *params)
{
	char **args;
	int params_count = 0;

	// count no params
	word_t *curr_word = params;

	while (curr_word) {
		params_count++;
		curr_word = curr_word->next_word;
	}

	// allocate args char array
	args = malloc((params_count + 2) * sizeof(char *));
	DIE(args == NULL, "error allocating args");

	// add the command as first entry in args array
	args[0] = strdup(verb->string);
	DIE(args[0] == NULL, "args[0] failed to allocated");

	curr_word = params;
	size_t i = 1;

	while (curr_word) {
		if (curr_word->expand) {
			char *temp_val = build_string_from_parts(curr_word);

			if (temp_val)
				args[i] = temp_val;
			else
				args[i] = NULL;
		} else {
			args[i] = strdup(curr_word->string);
		}
		i++;
		curr_word = curr_word->next_word;
	}

	// add NULL at the ending of args
	args[i] = NULL;

	return args;
}
