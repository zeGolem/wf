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

		} else // Parsing a command arg
			args->command_args[args->command_args_count++] = argv[i];
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

void run_command(char**               command_args,
                 unsigned             command_args_count,
                 struct watched_file* file)
{
	// Add one to the args count, as we need to add a NULL to signal the end of
	// the array
	char** final_command_args =
	    malloc((command_args_count + 1) * sizeof(char*));
	if (!final_command_args) {
		perror("wf: malloc");
		exit(-errno);
	}

	memset(final_command_args, 0, command_args_count * sizeof(char*));

	// For each command arguement
	for (unsigned i = 0; i < command_args_count; ++i) {
		char* current_arg = command_args[i];

		// If the current arg doesn't contain a '%'
		if (strchr(current_arg, '%') == 0) {
			// Just store the arguement as is, no need to do any substitutions
			final_command_args[i] = current_arg;

		} else { // Parse the '%' substitutions
			unsigned new_arg_capacity = 512;
			char*    new_arg          = malloc(new_arg_capacity * sizeof(char));
			if (!new_arg) {
				perror("wf: malloc");
				exit(-errno);
			}

			memset(new_arg, 0, new_arg_capacity * sizeof(char));

			// Current position in the `new_arg` array
			unsigned new_arg_pos = 0;

			// For each `c` in `current_arg`
			for (char* c = current_arg; *c; ++c) {
				if (*c != '%') {
					// Check we have enough space to add one character to
					// new_arg
					if (new_arg_pos + 1 >= new_arg_capacity) {
						// If we don't, increase the capacity and realloc
						// new_arg
						new_arg_capacity += 512;
						new_arg = realloc(new_arg, new_arg_capacity);
						if (!new_arg) {
							perror("wf: realloc");
							exit(-errno);
						}
					}

					// We know we have enough space, we can copy the current
					// character to the new_arg
					new_arg[new_arg_pos++] = *c;

					continue;
				}

				// We know `c` is a '%' here!

				// Go forward one character and check its value
				// This will make `c` the character following the '%', we can
				// choose which substitution to do
				switch (*(++c)) {
				case '%': { // "%%" will be interpreted as "%" in the new_arg
					// Check we have enough space to add one character to
					// new_arg
					if (new_arg_pos + 1 >= new_arg_capacity) {
						// If we don't, increase the capacity and realloc
						// new_arg
						new_arg_capacity += 512;
						new_arg = realloc(new_arg, new_arg_capacity);
						if (!new_arg) {
							perror("wf: realloc");
							exit(-errno);
						}
					}

					// We know we have enough space, we can add the '%' to the
					// `new_arg`.
					new_arg[new_arg_pos] = *c;
				} break;

				case 'F': { // "%F" will be replaced by the file's name
					unsigned filename_length = strlen(file->filename);

					// Check we have enough space to add the filename to
					// new_arg
					if (new_arg_pos + filename_length >= new_arg_capacity) {
						// If we don't, increase the capacity and realloc
						// new_arg
						new_arg_capacity += 512;
						new_arg = realloc(new_arg, new_arg_capacity);
						if (!new_arg) {
							perror("wf: realloc");
							exit(-errno);
						}
					}

					// Copy `filename_length` characters from the file's name to
					// the new arg, at the current position
					if (!memcpy(&new_arg[new_arg_pos],
					            file->filename,
					            filename_length)) {
						// If memcpy failed
						perror("wf: memcpy");
						exit(-errno);
					}

					new_arg_pos += filename_length;

				} break;

				default: {
					fprintf(
					    stderr, "Unknown substitution: %%%c, skipping\n", *c);
				} break;
				}
			}

			// Resize the string to the right size
			// Add one to the pos because of the final NULL byte
			new_arg = realloc(new_arg, new_arg_pos + 1);

			if (!new_arg) { // TODO: Can we ignore this error? The string is
				            // already ready, so we could just reuse the
				            // previously allocated memory...
				perror("wf: realloc");
				exit(-errno);
			}

			final_command_args[i] = new_arg;
		}
	}

	// Add a final NULL pointer at the end of the arguements array, to signal
	// the end
	final_command_args[command_args_count] = 0;

	// We have all arguements ready, it's time to fork to spawn a child

	pid_t current_pid = fork();

	if (current_pid < 0) {
		perror("wf: fork");
		exit(-errno);
	}

	if (current_pid == 0) {
		// In the child process

		// Spawn the desired command
		execvp(final_command_args[0], final_command_args);
	}

	// In the parrent, just return
	return;
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

		if (event.mask & IN_MODIFY) { // If a file was modified
			// For each watched file
			for (size_t i = 0; i < args->file_count; ++i) {
				// If the file isn't the one that was modified, skip it
				if (event.wd != watched_files[i]->inotify_wd) continue;

				// Run the shell command for the file that was updated
				run_command(args->command_args,
				            args->command_args_count,
				            watched_files[i]);
			}
		}
	}
}
