#include "io.h"
#include "guest_file.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if __STDC_VERSION__ >= 201112L
#define noreturn _Noreturn
#else
#define noreturn
#endif

#ifdef __GNUC__
#define noinline __attribute__((noinline))
#else
#define noinline
#endif

static const struct tmp_path tmp_path_template = {"/tmp/doarr.XXXXXX\0aaaaaaaaaaaaa"};

int doarr_io_init(struct doarr_io_ctx *ctx) {
	ctx->tmp_path = tmp_path_template;
	char *tmp_path = ctx->tmp_path.chars;

	if(!mkdtemp(tmp_path)) {
		perror("Could not create temporary directory: mkdtemp");
		return -1;
	}

	int fds[2];
	if(pipe(fds)) {
		perror("pipe");
		if(rmdir(tmp_path))
			perror("rmdir");
		return -1;
	}
	switch(fork()) {
		case -1: { // error
			perror("fork");
			if(rmdir(tmp_path))
				perror("rmdir");
			return -1;
		}
		case 0: { // child
			// block signals
			{
				sigset_t set;
				if(sigfillset(&set) || sigprocmask(SIG_BLOCK, &set, NULL)) {
					perror("sigfillset/sigprocmask");
					// exit child, do not return!
					_exit(127);
				}
			}
			// close pipe write end
			close(fds[1]);
			// wait for the pipe to close
			{
				char unused;
				read(fds[0], &unused, 1);
			}
			// remove the directory and its contents
			{
				const char *const argv[] = {"rm", "-rf", tmp_path, NULL};
				const char *const envp[] = {NULL};
				execve("/bin/rm", (char **) argv, (char **) envp);
			}
			// execve only returns on error
			perror("execve(rm)");
			// exit child, do not return!
			_exit(127);
			for(;;);
		}
		default: { // parent
			// close pipe read end
			close(fds[0]);
			// leak pipe write end - it will be closed by the OS when the process exits
			break;
		}
	}

	tmp_path[strlen(tmp_path)] = '/';
	return 0;
}

static void tmp_path_inc(struct doarr_io_ctx *ctx) {
	// increment in big-endian base26 with digits 'a'..'z', prefixed with '/', examples:
	// - .../aaaa -> .../aaab
	// - .../aaaz -> .../aaba
	// - .../iizz -> .../ijaa
	// - .../zzzz -> *abort*

	// point to the last character (size-1 is null terminator)
	char *c = &ctx->tmp_path.chars[tmp_path_size - 2];

	// replace trailing 'z's with 'a's (like replacing trailing 9s with 0s in decimal)
	while(*c == 'z')
		*c-- = 'a';

	// check for overflow (should not happen within the next few millenia, but just to be sure)
	if(*c == '/')
		abort();

	// increment the right-most non-'z'
	(*c)++;
}

static int full_write(int fd, const unsigned char *begin, const unsigned char *end) {
	while(begin != end) {
		ssize_t w = write(fd, begin, end - begin);
		if(w == -1 || w == 0)
			return -1;
		begin += w;
	}
	return 0;
}

void doarr_tmp_path(struct doarr_io_ctx *ctx, const char ext[tmp_ext_size], struct tmp_full_path *out_path) {
	memcpy(out_path->chars, ctx->tmp_path.chars, tmp_path_len);
	memcpy(out_path->chars + tmp_path_len, ext, tmp_ext_size);
	tmp_path_inc(ctx);
}

const char *doarr_extract_precompiled_or_null(struct doarr_io_ctx *ctx, struct guest_file *file) {
	if(*file->gch_tmp_path.chars)
		return file->gch_tmp_path.chars;

	struct tmp_path path = ctx->tmp_path;
	struct tmp_full_path full_path;
	doarr_tmp_path(ctx, ".gch", &full_path);

	int fd = open(full_path.chars, O_WRONLY|O_CLOEXEC|O_CREAT|O_EXCL|O_NOFOLLOW, 0400);
	if(fd < 0) {
		perror("Cannot extract precompiled header: open");
		return NULL;
	}
	if(full_write(fd, file->gch_data, file->gch_data_end) < 0) {
		perror("Cannot extract precompiled header: write");
		return NULL;
	}
	if(close(fd) < 0) {
		perror("Cannot extract precompiled header: close after write");
		return NULL;
	}

	file->gch_tmp_path = path;
	return file->gch_tmp_path.chars;
}

static noinline noreturn void execute_compiler(const char *cxx_file_name, const char *so_file_name, const struct guest_file *file) {
	size_t n = file->num_compiler_args;
	const char *argv[n + 4]; // VLA!
	{
		size_t k = file->pos_between_args;
		const char *const *in_arg = file->compiler_args, **out_arg = argv;
		for(size_t i = 0; i < k; i++)
			*out_arg++ = *in_arg++;
		*out_arg++ = cxx_file_name;
		for(size_t i = k; i < n; i++)
			*out_arg++ = *in_arg++;
		*out_arg++ = "-o";
		*out_arg++ = so_file_name;
		*out_arg++ = NULL;
	}

	const char *compiler_name = *argv;

	execv(compiler_name, (char **) argv);

	// execve only returns on error
	perror("Error while executing compiler: execv");

	_exit(127);
}

static bool compile(const char *cxx_file_name, const char *so_file_name, const struct guest_file *file) {
	pid_t pid = fork();
	switch(pid) {
		case -1: { // error
			perror("Error while executing compiler: fork");
			return false;
		}
		case 0: { // child
			execute_compiler(cxx_file_name, so_file_name, file);
			for(;;);
		}
		default: { // parent
			siginfo_t info;
			if(waitid(P_PID, pid, &info, WEXITED)) {
				perror("waitid");
				return false;
			}
			if(info.si_code == CLD_EXITED) {
				if(info.si_status == 0) {
					return true;
				} else {
					fprintf(stderr, "Compiler exited with status %i\n", (int) info.si_status);
					return false;
				}
			} else { // signal
				fprintf(stderr, "Compiler killed by signal %i\n", (int) info.si_status);
				return false;
			}
		}
	}
}

static void try_remove(const char *file_name) {
	if(unlink(file_name))
		perror("unlink");
}

int doarr_compile_and_load(struct doarr_io_ctx *ctx, struct tmp_full_path *cxx_file_name, const struct guest_file *file, void **out_handle, void **out_fn) {
	struct tmp_path so_file_name = ctx->tmp_path;
	tmp_path_inc(ctx);

	// compile c++ to shared library
	bool compiled_ok = compile(cxx_file_name->chars, so_file_name.chars, file);
	try_remove(cxx_file_name->chars);
	if(!compiled_ok)
		return 1;

	// load shared library
	void *handle = dlopen(so_file_name.chars, RTLD_NOW);
	if(!handle)
		fprintf(stderr, "dlopen: %s\n", dlerror());
	try_remove(so_file_name.chars);
	if(!handle)
		return 2;

	// lookup generated entry point
	void *fn = dlsym(handle, "DOARR_EXPORT");
	if(!fn) {
		fprintf(stderr, "dlsym: %s\n", dlerror());
		dlclose(handle);
		return 2;
	}

	*out_handle = handle;
	*out_fn = fn;
	return 0;
}
