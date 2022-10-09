#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

// Maximum number of characters an arguement of the command can be.
// TODO: Get rid of this.
#define MAX_COMMAND_ARG_LENGTH 512

struct watched_file {
	int   inotify_wd;
	char* filename;
};

struct program_args {
	unsigned file_count;
	char**   filenames;
	unsigned command_args_count;
	char**   command_args;
};

struct program_args* parse_args(int argc, char** argv)
{
	struct program_args* args = malloc(sizeof(struct program_args));
	if (!args) {
		perror("wf: malloc");
		exit(-errno);
	}

	memset(args, 0, sizeof(struct program_args));

	// This is wayyyy too much for what we need, but we'll
	// resize once we know the true length of the array
	args->filenames = malloc(sizeof(char*) * argc);
	if (!args->filenames) {
		perror("wf: malloc");
		exit(-errno);
	}

	// Same here
	args->command_args = malloc(sizeof(char*) * argc);
	if (!args->command_args) {
		perror("wf: malloc");
		exit(-errno);
	}

	bool is_parsing_command = false;

	for (int i = 1; i < argc; ++i) {
		if (!is_parsing_command) { // Parsing a file name

			if (strcmp(argv[i], "--") == 0) {
				// We encountered a "--", switching to command arg parsing
				is_parsing_command = true;
				continue;
			}

			// Normal file name "parsing"
			args->filenames[args->file_count++] = argv[i];

		} else { // Parsing a command arg
			unsigned long current_len = strlen(argv[i]);

			// We want to surround the arguement in '"'s to avoid issues with
			// spaces

			// Prepare a memory location to store the new string
			int new_arg_size =
			    current_len + 2 + 1; // +2 for the quotes, +1 for the \0
			char* new_arg = malloc(new_arg_size);

			snprintf(new_arg, new_arg_size, "\"%s\"", argv[i]);

			// Put the new string into the array
			args->command_args[args->command_args_count++] = new_arg;
		}
	}

	// Realloc the arrays to their appropriate size to save memory
	args->filenames =
	    realloc(args->filenames, args->file_count * sizeof(char*));
	args->command_args =
	    realloc(args->command_args, args->command_args_count * sizeof(char*));

	return args;
}

struct watched_file** watch_files(int inotify_fd, struct program_args* args)
{
	struct watched_file** files =
	    malloc(sizeof(struct watched_file*) * args->file_count);
	if (!files) {
		perror("wf: malloc");
		exit(errno);
	}

	memset(files, 0, sizeof(struct watched_file*) * args->file_count);

	for (size_t i = 0; i < args->file_count; ++i) {
		char* const current_filename = args->filenames[i];

		int inotify_wd = inotify_add_watch(
		    inotify_fd, current_filename, IN_MASK_CREATE | IN_MODIFY);
		if (inotify_wd < 0) {
			perror("wf: inotify_add_watch");
			fprintf(stderr, "Couldn't watch file %s", current_filename);
			continue;
		}

		files[i] = malloc(sizeof(struct watched_file));
		if (!files[i]) {
			perror("wf: malloc");
			exit(errno);
		}

		files[i]->filename   = current_filename;
		files[i]->inotify_wd = inotify_wd;
	}

	return files;
}

int run_shell_command_from_args(char**   command_args,
                                unsigned command_args_count)
{
	char* command = malloc(command_args_count * MAX_COMMAND_ARG_LENGTH);
	memset(command, 0, command_args_count * MAX_COMMAND_ARG_LENGTH);

	unsigned position_in_command = 0;

	for (unsigned i = 0; i < command_args_count; ++i) {
		char* current_command     = command_args[i];
		int   current_command_len = strlen(current_command);

		command[position_in_command++] = '"';

		// Copy `current_command_len` bytes from `current_command` to
		// `position_in_command` bytes into `command`,
		void* ret = memcpy(command + position_in_command,
		                   current_command,
		                   current_command_len);

		if (!ret) {
			perror("wf: memcpy");
			exit(errno);
		}

		position_in_command += current_command_len;

		command[position_in_command++] = '"';
		command[position_in_command++] = ' ';

		// TODO: Check we don't excede the allocated size of `command`
	}

	return system(command);
}

int main(int argc, char** argv)
{
	// TODO: Print error message
	if (argc < 2) return -1;

	struct program_args* args = parse_args(argc, argv);

	int inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		perror("wf: notify");
		return inotify_fd;
	}

	struct watched_file** watched_files = watch_files(inotify_fd, args);

	while (true) {
		struct inotify_event event = {0};

		// Read an inotify event
		ssize_t read_size = read(inotify_fd, &event, sizeof(event));
		if (read_size < 0) {
			perror("wf: read");
			return read_size;
		}

		// Safe C cast, we checked that read_size is >= 0
		if ((unsigned)read_size < sizeof(event)) {
			fprintf(stderr, "Read less data than expected...");
			continue;
		}

		if (event.mask & IN_MODIFY) {
			printf("File %d was updated\n", event.wd);
			run_shell_command_from_args(args->command_args,
			                            args->command_args_count);
		}
	}
}
