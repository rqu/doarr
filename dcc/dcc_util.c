#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dcc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

noreturn void dcc_fexecv(int fd, const char *const *argv) {
	extern char **environ;
	fexecve(fd, (char *const *) argv, environ);
	// fexecve returned? there was an error
	if(errno == ENOENT) {
		// fd refers to a script? drop the CLOEXEC flag
		int flags = fcntl(fd, F_GETFD);
		if(flags < 0) {
			dcc_perror_s(RT_ERR "fcntl F_GETFD", *argv);
			goto error;
		}
		if(fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC)) {
			dcc_perror_s(RT_ERR "fcntl F_SETFD", *argv);
			goto error;
		}
		// try again
		fexecve(fd, (char *const *) argv, environ);
	}
	dcc_perror_s("fexecve", *argv);
error:
	_exit(127);
}

bool dcc_unlink(int dir_fd, const char *basename) {
	if(unlinkat(dir_fd, basename, 0)) {
		dcc_perror_s(RT_ERR "unlinkat", basename);
		return false;
	}
	return true;
}

void dcc_unlink_if_ex(int dir_fd, const char *basename) {
	if(unlinkat(dir_fd, basename, 0) && errno != ENOENT) {
		dcc_perror_s(RT_ERR "unlinkat", basename);
	}
}

bool dcc_wait(pid_t *pid, const char *name) {
	siginfo_t info;
	if(waitid(P_PID, *pid, &info, WEXITED)) {
		dcc_perror_s(RT_ERR "waitid", name);
		*pid = -1;
		return false;
	}
	*pid = -1;
	if(info.si_code == CLD_EXITED) {
		if(info.si_status == 0) {
			return true;
		} else {
			dcc_errf("%s exited with status %i", name, (int) info.si_status);
			return false;
		}
	} else { // signal
		dcc_errf("%s killed by signal %i", name, (int) info.si_status);
		return false;
	}
}

bool dcc_write(int fd, const char* buf, size_t count) {
	while(count) {
		ssize_t w = write(fd, buf, count);
		if(w <= 0)
			return false;
		buf += w;
		count -= w;
	}
	return true;
}

void dcc_close(int fd, const char *msg) {
	if(close(fd))
		dcc_perror_s(RT_ERR "close", msg);
}

bool dcc_map(int dir_fd, const char *basename, const void **out_data, size_t *out_size) {
	int fd = openat(dir_fd, basename, O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NOCTTY);
	if(fd < 0) {
		dcc_perror_s(RT_ERR "dcc_map open", basename);
		return false;
	}
	ssize_t size = lseek(fd, 0, SEEK_END);
	if(size < 0) {
		dcc_perror_s(RT_ERR "dcc_map lseek", basename);
		dcc_close(fd, basename);
		return false;
	}
	if(size == 0) {
		*out_data = (const void *) 42;
		*out_size = 0;
		return true;
	}
	void *data = mmap(NULL, size, PROT_READ, MAP_SHARED|MAP_NORESERVE, fd, 0);
	if(data == MAP_FAILED) {
		dcc_perror_s(RT_ERR "dcc_map mmap", basename);
		dcc_close(fd, basename);
		return false;
	}
	dcc_close(fd, basename);
	*out_data = data;
	*out_size = size;
	return true;
}

void dcc_unmap(const void *data, size_t size) {
	if(size) {
		if(munmap((void *) data, size))
			dcc_perror(RT_ERR "dcc_unmap");
	} else {
		assert(data == (const void *) 42);
	}
}

FILE *dcc_fopenat(int dir_fd, const char *basename, int flags, int mode) {
	int fd = openat(dir_fd, basename, flags, mode);
	if(fd < 0) {
		dcc_perror_s(RT_ERR "openat", basename);
		return NULL;
	}
	FILE *file = fdopen(fd, "w");
	if(!file) {
		dcc_perror_s(RT_ERR "fdopen", basename);
		dcc_close(fd, basename);
	}
	return file;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// assigned in main
const char *dcc_argv0;

bool dcc_err(const char *msg) {
	fprintf(stderr, "%s: %s\n", dcc_argv0, msg);
	return false;
}

bool dcc_perror(const char *msg) {
	fprintf(stderr, "%s: %s: %s\n", dcc_argv0, msg, strerror(errno));
	return false;
}

bool dcc_perror_s(const char *msg, const char *arg) {
	fprintf(stderr, "%s: %s '%s': %s\n", dcc_argv0, msg, arg, strerror(errno));
	return false;
}

bool dcc_err_a(const char *msg, const char *const *args) {
	FILE *f = stderr;
	fprintf(f, "%s: %s", dcc_argv0, msg);
	for(size_t i = 0; args[i]; i++)
		fputc(' ', f), fputs(args[i], f);
	fputc('\n', f);
	return false;
}

void dcc_oom(void) {
	static const char msg[] = "dcc: out of memory\n";
	// do not use stdio, since that could fail too
	write(STDERR_FILENO, msg, sizeof msg - 1);
}
