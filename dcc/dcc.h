#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define DOARR_PRECOMPILED "doarr__precompiled"

#if __STDC_VERSION__ >= 201112L
#define noreturn _Noreturn
#else
#warning "no noreturn"
#define noreturn
#endif

#define LEN(ARRAY) ((int)(sizeof(ARRAY) / sizeof*(ARRAY)))

struct input_file {
	const char *name;
	int pos_between_opts;
};

enum tool_id {
	ToolCXX,
	ToolCC,
	ToolLD,
	ToolObjcopy,
	NumTools
};

struct tool {
	const char *name;
	int exec_fd;
};

enum env_var {
	EnvVarCXX,
	EnvVarCC,
	EnvVarLD,
	EnvVarOBJCOPY,
	EnvVarPATH,
	EnvVarTMP,
	NumEnvVars
};

extern const struct env_var_info {
	const char *name;
	const char *default_value;
	const char *desc;
} env_vars[NumEnvVars];

struct compiler_arg {
	const char *arg;
	bool also_runtime;
};

struct config {
	struct compiler_arg *compiler_args;
	int num_compiler_args;
	const char *output;
	bool compile, preproc, nowarn, verbose, invalid;

	struct tool tools[NumTools];

	int dev_null;
	int tmp_fd;
	const char *tmp_pch_abs;
};

enum { TmpOutputNameLen = 64 };

// dcc_options.c

void dcc_parse_args(char **argp, struct config *config, struct input_file *out_inputs, int *out_num_inputs);

// dcc_scan.c

char *dcc_scan_up_to_next_export(FILE *in);

// dcc_gen.c

size_t dcc_gen_required_arg_buff_size(const struct config *config, int num_files);
bool dcc_gen_main(const struct config *config, int num_files, const struct input_file input_files[num_files], char tmp_output_files[num_files][TmpOutputNameLen], const char **arg_buff);

// dcc_util.c

noreturn void dcc_fexecv(int fd, const char *const *argv);
bool dcc_unlink(int dir_fd, const char *basename);
void dcc_unlink_if_ex(int dir_fd, const char *basename);
bool dcc_wait(pid_t *pid, const char *name);
bool dcc_write(int fd, const char* buf, size_t count);
void dcc_close(int fd, const char *msg);
bool dcc_map(int dir_fd, const char *basename, const void **out_data, size_t *out_size);
void dcc_unmap(const void *data, size_t size);
FILE *dcc_fopenat(int dir_fd, const char *basename, int flags, int mode);

extern const char *dcc_argv0;
static inline bool always_false(int unused) { (void) unused; return false; }
#define dcc_errf(FMT, ...) always_false(fprintf(stderr, "%s: "FMT"\n", dcc_argv0, __VA_ARGS__))
bool dcc_err(const char *msg);
bool dcc_perror(const char *msg);
bool dcc_perror_s(const char *msg, const char *arg);
bool dcc_err_a(const char *msg, const char *const *args);
void dcc_oom(void);

static inline const char *my_basename(const char *path) {
	const char *last_slash = strrchr(path, '/');
	return last_slash ? last_slash + 1 : path;
}

#define STRINGIFY_(TOKS) #TOKS
#define STRINGIFY(MACRO) STRINGIFY_(MACRO)
#ifdef NDEBUG
#define RT_ERR "runtime error: "
#else
#define RT_ERR "runtime error at " __FILE__ ":" STRINGIFY(__LINE__) ": "
#endif
