#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dcc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef O_PATH
#define DCC_O_PATH O_PATH
#else
#define DCC_O_PATH O_RDONLY
#endif

static char *concat(const char *a, const char *b) {
	size_t alen = strlen(a);
	size_t blen = strlen(b);
	char *res = malloc(alen + blen + 1);
	if(res) {
		memcpy(res, a, alen);
		memcpy(res + alen, b, blen + 1);
	}
	return res;
}

const struct env_var_info env_vars[] = {
	[EnvVarCXX] = {"DCC_CXX", "c++", "C++ compiler - used for both precompilation and runtime specialization"},
	[EnvVarCC] = {"CC", "cc", "C compiler - used internally during build"},
	[EnvVarLD] = {"LD", "ld", "linker - used internally during build"},
	[EnvVarOBJCOPY] = {"OBJCOPY", "objcopy", "objcopy - used internally during build"},
	[EnvVarPATH] = {"PATH", "/usr/local/bin:/usr/bin:/bin", "used to resolve the above programs"},
	[EnvVarTMP] = {"TMP", "/tmp", "directory to store temporary files"},
};

static bool is_executable(int fd, const char *msg) {
	struct stat statbuf;
	if(fstat(fd, &statbuf)) {
		dcc_perror_s(RT_ERR "fstat", msg);
		return false;
	}
	return S_ISREG(statbuf.st_mode) && statbuf.st_mode&0444;
}

static bool find_tools_slash(struct tool tools[NumTools]) {
	for(int i = 0; i < NumTools; i++) {
		const char *name = tools[i].name;
		if(strchr(name, '/')) {
			int fd = open(name, DCC_O_PATH|O_CLOEXEC|O_NOCTTY|O_NONBLOCK);
			if(fd < 0) {
				return dcc_perror_s("cannot open tool binary", name);
			}
			if(!is_executable(fd, name)) {
				dcc_errf("'%s' may not be an executable file", name);
				dcc_close(fd, name);
				return false;
			}
			tools[i].exec_fd = fd;
		}
	}
	return true;
}

static void find_tools_path(const char *dir, struct tool tools[NumTools]) {
	int dir_fd = *dir ? open(dir, DCC_O_PATH|O_CLOEXEC|O_DIRECTORY) : AT_FDCWD;
	if(dir_fd < 0) switch(errno) {
		case ENOENT:
		case EACCES:
		case ENOTDIR:
			return; // ignore
		default:
			dcc_perror_s("cannot open PATH directory", dir);
			return;
	}
	for(int i = 0; i < NumTools; i++) {
		const char *name = tools[i].name;
		if(!tools[i].exec_fd) { // not found yet
			int fd = openat(dir_fd, name, DCC_O_PATH|O_CLOEXEC|O_NOCTTY|O_NONBLOCK);
			if(fd < 0) {
				continue; // not in this directory
			}
			if(!is_executable(fd, name)) {
				dcc_close(fd, name);
				continue;
			}
			tools[i].exec_fd = fd;
		}
	}
}

static bool find_all_tools(char *env[NumEnvVars], struct tool tools[NumTools]) {
	tools[ToolCXX].name = env[EnvVarCXX];
	tools[ToolCC].name = env[EnvVarCC];
	tools[ToolLD].name = env[EnvVarLD];
	tools[ToolObjcopy].name = env[EnvVarOBJCOPY];

	if(!find_tools_slash(tools)) {
		// some tool was missing,
		// error already reported
		return false;
	}

	char *path = env[EnvVarPATH];
	env[EnvVarPATH] = NULL;

	for(char *elem = path, *path_end = path + strlen(path);;) {
		char *elem_end = memchr(elem, ':', path_end - elem);
		if(elem_end) {
			// place temporary terminator in place of the colon
			*elem_end = '\0';
		}
		find_tools_path(elem, tools);
		if(!elem_end) {
			// this was the last element
			break;
		}
		// restore the colon
		*elem_end = ':';
		elem = elem_end + 1;
	}

	bool have_all_tools = true;
	for(int i = 0; i < NumTools; i++) {
		if(!tools[i].exec_fd) {
			dcc_errf("no '%s' in '%s'", tools[i].name, path);
			have_all_tools = false;
		}
	}
	if(!have_all_tools)
		return false;

	env[EnvVarPATH] = path;
	return true;
}

static char *fd_to_abs_path(int dev_fd, int fd) {
	char decimal[3 * sizeof fd];
	sprintf(decimal, "%i", fd);
	size_t capacity = 32;
	char *result = malloc(capacity);
	for(;;) {
		if(!result) return dcc_oom(), NULL;
		ssize_t size = readlinkat(dev_fd, decimal, result, capacity);
		if(size < 0) {
			dcc_perror("cannot obtain full path to the compiler binary: readlinkat");
			free(result);
			return NULL;
		}
		if((size_t) size < capacity) {
			result[size] = '\0';
			return result;
		}
		capacity = capacity / 2 * 3;
		result = realloc(result, capacity);
	}
}

int main(int argc, char **argv) {
	int status = 0;
	dcc_argv0 = my_basename(*argv);

	struct input_file *inputs = malloc(argc * sizeof *inputs);
	if(!inputs) return dcc_oom(), 1;
	int num_files = 0;

	struct config config = {
		.compiler_args = malloc(argc * sizeof *config.compiler_args),
	};
	if(!config.compiler_args) return dcc_oom(), 1;

	config.dev_null = open("/dev/null", O_RDWR|O_CLOEXEC);
	if(config.dev_null < 3) {
		if(config.dev_null < 0) {
			dcc_perror("cannot open '/dev/null'");
			return 1;
		} else {
			// there were not even 3 open fds - we do not have stderr
			return 1;
		}
	}

	int dev_fd = open("/dev/fd", DCC_O_PATH|O_CLOEXEC|O_DIRECTORY);
	if(dev_fd < 0) {
		dcc_perror("cannot open '/dev/fd'");
		return 1;
	}

	dcc_parse_args(argv + 1, &config, inputs, &num_files);
	argv = NULL;

	if(config.invalid) {
		// error already reported
		return 1;
	}
	if(!num_files) {
		dcc_err("no input files");
		return 1;
	}
	if(config.compile == config.preproc) {
		dcc_err("exactly one of -c (compile to object file) and -E (compile to C) is required");
		return 1;
	}

	// pointer to packed arrays
	char (*tmp_outputs)[TmpOutputNameLen] = malloc(num_files * sizeof *tmp_outputs);
	for(int i = 0; i < num_files; i++) {
		snprintf(tmp_outputs[i], sizeof tmp_outputs[i], "%i.%s", i, my_basename(inputs[i].name));
	}

	char *env[NumEnvVars];
	for(int i = 0; i < NumEnvVars; i++) {
		const char *v = getenv(env_vars[i].name);
		env[i] = strdup(v ? v : env_vars[i].default_value);
		if(!env[i]) return dcc_oom(), 1;
	}

	if(!find_all_tools(env, config.tools)) {
		return 1;
	}
	const char **arg_buff = malloc(dcc_gen_required_arg_buff_size(&config, num_files) * sizeof *arg_buff);
	if(!arg_buff) return dcc_oom(), 1;
	*arg_buff = fd_to_abs_path(dev_fd, config.tools[ToolCXX].exec_fd);
	if(!*arg_buff) {
		return 1;
	}

	close(dev_fd);

	if(strlen(env[EnvVarTMP]) >= 1024) {
		dcc_errf("path name too long: '%s'", env[EnvVarTMP]);
		return 1;
	}
	char *tmp = concat(env[EnvVarTMP], "/dcc.XXXXXX");
	if(!tmp) return dcc_oom(), 1;
	if(!mkdtemp(tmp)) {
		dcc_perror("cannot create temporary subdirectory: mkdtemp");
		return 1;
	}
	config.tmp_fd = open(tmp, DCC_O_PATH|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW);
	if(config.tmp_fd < 0) {
		dcc_perror_s("cannot open temporary subdirectory: open", tmp);
		status = 1;
		goto rm_tmp;
	}

	config.tmp_pch_abs = concat(tmp, "/" DOARR_PRECOMPILED);
	if(!config.tmp_pch_abs) {
		dcc_oom(), status = 1;
		goto rm_tmp;
	}

	if(!dcc_gen_main(&config, num_files, inputs, tmp_outputs, arg_buff)) {
		status = 1;
	}

rm_tmp:
	if(rmdir(tmp)) {
		dcc_perror_s("cannot remove temporary subdirectory: rmdir", tmp);
		status = 1;
	}
	return status;
}
