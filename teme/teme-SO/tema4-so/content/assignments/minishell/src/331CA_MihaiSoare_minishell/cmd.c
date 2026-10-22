// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	exit(0);
}

/**
 * Internal change-directory command.
 * BONUS 1 IMPLEMENTED: ``cd`` with no arguments changes directory to home.
 * BONUS 2 IMPLEMENTED: ``cd -`` changes directory to oldpwd env variable path.
 */
static bool shell_cd(word_t *dir)
{
	const char *path = dir ? build_string_from_parts(dir) : getenv("HOME");
	char *oldpwd = NULL;
	int status;

	if (!path)
		return false;

	if (strcmp(path, "-") == 0)
		oldpwd = getenv("OLDPWD");

	if (oldpwd)
		status = chdir(oldpwd);
	else
		status = chdir(path);

	if (status == 0)
		return true;

	return false;
}

/**
 * Sets the environment variable from a command
 */
static int set_env(simple_command_t *s)
{
	int status;
	char *var_value = build_string_from_parts(s->verb->next_part->next_part);

	status = setenv(s->verb->string, var_value, REWRITE_ENV);

	free(var_value);

	return status;
}

/**
 * Runs a simple command by checking its type (cd, setting env, normal command)
 * by forking a new child process if its the case and returns the exit code.
 */
static int run_simple_command(simple_command_t *s, char **args)
{
	int status;

	// check if command is cd
	if (strcmp(s->verb->string, "cd") == 0)
		return !shell_cd(s->params);

	// check if command is setting env
	if (s->verb->next_part && strcmp(s->verb->next_part->string, "=") == 0) {
		status = set_env(s);

		return status;
	}

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return EXIT_FAILURE;
	case 0:
		execvp(s->verb->string, args);
		fprintf(stderr, "Execution failed for \'%s\'\n", s->verb->string);
		exit(EXIT_FAILURE);
	default:
		break;
	}

	waitpid(pid, &status, 0);

	return WEXITSTATUS(status);
}

/**
 * Cleanup the redirect filename strings used in redirecting output and erros
 * to files.
 */
static int cleanup_redirect(char *out_filename, char *err_filename)
{
	free(out_filename);
	free(err_filename);
	return EXIT_FAILURE;
}

/**
 * Redirect the output of a command to another file instead of the STDOUT or
 * older file descriptor opened.
 */
static int redirect_to_output_file(simple_command_t *s, char **args)
{
	char *out_filename, *err_filename;

	out_filename = build_string_from_parts(s->out);
	err_filename = build_string_from_parts(s->err);

	// builds the flags needed for creating a new file
	int flags = O_WRONLY | O_CREAT;

	if (s->io_flags & IO_OUT_APPEND ||
		(s->err && strcmp(out_filename, err_filename) == 0))
		flags |= O_APPEND;
	else
		flags |= O_TRUNC;

	// creates a backup fd
	int stdout_backup_fd = dup(STDOUT_FILENO);

	if (stdout_backup_fd == -1)
		return cleanup_redirect(out_filename, err_filename);

	// opens the new fd with the out file provided
	int output_fd = open(out_filename, flags, 0644);

	if (output_fd == -1)
		return cleanup_redirect(out_filename, err_filename);

	// connects the new fd opened to stdout
	if (dup2(output_fd, STDOUT_FILENO) == -1) {
		close(output_fd);
		return cleanup_redirect(out_filename, err_filename);
	}

	int status = run_simple_command(s, args);

	// clears the opened fd
	if (dup2(stdout_backup_fd, STDOUT_FILENO) == -1) {
		close(output_fd);
		return cleanup_redirect(out_filename, err_filename);
	}

	// closes the opened fd
	if (close(output_fd) == -1)
		return cleanup_redirect(out_filename, err_filename);

	return status;
}

/**
 * Redirect the errors of a command to another file instead of the STDERR or
 * older file descriptor opened.
 */
static int redirect_to_err_output_file(simple_command_t *s, char **args)
{
	char *out_filename, *err_filename;

	out_filename = build_string_from_parts(s->out);
	err_filename = build_string_from_parts(s->err);

	// builds flags for creating new file descriptor with provided filename
	int flags = O_WRONLY | O_CREAT;

	if (s->io_flags & IO_ERR_APPEND)
		flags = flags | O_APPEND;
	else
		flags = flags | O_TRUNC;

	// creates a backup fd of the old stderr file descriptor
	int stderr_backup_fd = dup(STDERR_FILENO);

	if (stderr_backup_fd == -1)
		return cleanup_redirect(out_filename, err_filename);

	// opens new file descriptor with provided err filename
	int err_output_fd = open(err_filename, flags, 0644);

	if (err_output_fd == -1)
		return cleanup_redirect(out_filename, err_filename);

	// connects new fd to stderr
	if (dup2(err_output_fd, STDERR_FILENO) == -1) {
		close(err_output_fd);
		return cleanup_redirect(out_filename, err_filename);
	}

	// if there is also a redirect to stdout, it is going to be done by another
	// provided function, so the command wont be run two times
	if (s->out)
		return EXIT_SUCCESS;

	int status = run_simple_command(s, args);

	// restores stderr fd
	if (dup2(stderr_backup_fd, STDERR_FILENO) == -1) {
		close(err_output_fd);
		return cleanup_redirect(out_filename, err_filename);
	}

	// closes opened fd
	if (close(err_output_fd) == -1)
		return cleanup_redirect(out_filename, err_filename);

	return status;
}

/**
 * Redirect the input provided to a command.
 */
static int redirect_with_input(simple_command_t *s, char **args)
{
	int status;

	char *in_filename = build_string_from_parts(s->in);

	// open filename
	int input_fd = open(in_filename, O_RDONLY);

	if (input_fd == -1) {
		free(in_filename);
		return EXIT_FAILURE;
	}

	// save a backup of the older stdin fd
	int stdin_backup = dup(STDIN_FILENO);

	if (stdin_backup == -1) {
		close(input_fd);
		free(in_filename);
		return EXIT_FAILURE;
	}

	// close the old stdin fileno and use the new one opened
	if (dup2(input_fd, STDIN_FILENO) == -1) {
		close(input_fd);
		free(in_filename);
		return EXIT_FAILURE;
	}

	// run only error or set err file descriptor
	if (s->err) {
		status = redirect_to_err_output_file(s, args);
		if (status == EXIT_FAILURE) {
			close(input_fd);
			free(in_filename);
			return EXIT_FAILURE;
		}
	}

	// run only out or run out and err redirect
	if (s->out) {
		status = redirect_to_output_file(s, args);
		if (status == EXIT_FAILURE) {
			close(input_fd);
			free(in_filename);
			return EXIT_FAILURE;
		}
	}

	// run only input
	if (!s->out && !s->err)
		status = run_simple_command(s, args);

	// regain older input stdin (usually of the shell)
	if (dup2(stdin_backup, STDIN_FILENO) == -1) {
		close(input_fd);
		free(in_filename);
		return EXIT_FAILURE;
	}

	// close the input fd
	if (close(input_fd) == -1) {
		free(in_filename);
		return EXIT_FAILURE;
	}

	free(in_filename);

	return status;
}

/**
 * Redirect output and stderr without providing input
 */
static int redirect_without_input(simple_command_t *s, char **args)
{
	int status;

	if (s->err)
		status = redirect_to_err_output_file(s, args);

	if (s->out)
		status = redirect_to_output_file(s, args);

	return status;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	int status;
	char **args = NULL;

	// sanity checks
	if (strcmp(s->verb->string, "exit") == 0
		|| strcmp(s->verb->string, "quit") == 0) {
		shell_exit();
	}

	// get an array of args from params input struct
	if (s->params) {
		args = get_params_string(s->verb, s->params);
	} else {
		args = malloc(sizeof(char *) * 2);
		args[0] = strdup(s->verb->string);
		args[1] = NULL;
	}

	// run simple command without out / in / err redirect
	if (!s->in && !s->out && !s->err)
		status = run_simple_command(s, args);
	else if (s->in)
		status = redirect_with_input(s, args);
	else
		status = redirect_without_input(s, args);

	return status;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int status;
	pid_t pid1 = fork();

	if (pid1 == -1) {
		return EXIT_FAILURE;
	} else if (pid1 == 0) {
		int status1 = parse_command(cmd1, level, father);

		exit(status1);
	}

	pid_t pid2 = fork();

	if (pid2 == -1) {
		return EXIT_FAILURE;
	} else if (pid2 == 0) {
		int status2 = parse_command(cmd2, level, father);

		exit(status2);
	}

	// wait for both child processes to finish
	waitpid(pid1, &status, 0);
	waitpid(pid2, &status, 0);

	// return exit code of the second command
	if (WEXITSTATUS(status) == 0)
		return true;

	return false;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int status, rc;
	int pipedes[2];

	rc = pipe(pipedes);
	DIE(rc == -1, "pipe failed");

	// fork a first child proccess that runs the first command and
	// redirects output to second command
	pid_t pid1 = fork();

	if (pid1 == -1) {
		rc = close(pipedes[0]);
		DIE(rc == -1, "close pipedes[STDIN] failed");
		rc = close(pipedes[1]);
		DIE(rc == -1, "close pipedes[STDOUT] failed\n");
		return EXIT_FAILURE;
	} else if (pid1 == 0) {
		rc = close(pipedes[0]);
		DIE(rc == -1, "close pipedes[STDOUT] failed\n");

		rc = dup2(pipedes[1], STDOUT_FILENO);
		DIE(rc == -1, "close dup2 failed\n");

		int status1 = parse_command(cmd1, level + 1, father);

		rc = close(pipedes[1]);
		DIE(rc == -1, "close pipedes[STDOUT] failed\n");
		exit(status1);
	}

	// fork a second child process that runs the second command
	// with input from the first child proccess
	pid_t pid2 = fork();

	if (pid2 == -1) {
		rc = close(pipedes[0]);
		DIE(rc == -1, "close pipedes[STDIN] failed");
		close(pipedes[1]);
		DIE(rc == -1, "close pipedes[STDOUT] failed\n");
		return EXIT_FAILURE;
	} else if (pid2 == 0) {
		close(pipedes[1]);
		DIE(rc == -1, "close pipedes[STDOUT] failed\n");

		rc = dup2(pipedes[0], STDIN_FILENO);
		DIE(rc == -1, "close dup2 failed\n");

		int status2 = parse_command(cmd2, level + 1, father);

		rc = close(pipedes[0]);
		DIE(rc == -1, "close pipedes[STDIN] failed\n");
		exit(status2);
	}

	// close the pipe descriptors
	rc = close(pipedes[0]);
	DIE(rc == -1, "close pipedes[STDIN] failed");
	rc = close(pipedes[1]);
	DIE(rc == -1, "close pipedes[STDOUT] failed\n");

	// wait for both child process to end
	waitpid(pid1, &status, 0);
	waitpid(pid2, &status, 0);

	// return the exit code of the second command
	if (WEXITSTATUS(status) == 0)
		return true;

	return false;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	int status;

	if (c->op == OP_NONE) {
		// execute simple command
		status = parse_simple(c->scmd, level, father);

		return status;
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		// execute commands one after another
		status = parse_command(c->cmd1, level + 1, c);

		status = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		// execute run in parallel and negate the output to match the exit codes
		status = !run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:
		// execute the second command only if the first one returns non-zero
		if (c->cmd1) {
			int status1 = parse_command(c->cmd1, level + 1, c);

			if (status1 != 0 && c->cmd2)
				status = parse_command(c->cmd2, level + 1, c);
		}
		break;

	case OP_CONDITIONAL_ZERO:
		// execute the second command only if the first one returns zero
		if (c->cmd1) {
			int status1 = parse_command(c->cmd1, level + 1, c);

			if (status1 == 0 && c->cmd2)
				status = parse_command(c->cmd2, level + 1, c);
		}
		break;

	case OP_PIPE:
		// execute run on pipe and negate the output to match the exit codes
		status = !run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return status;
}
