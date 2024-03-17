#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dcc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char generated_file_prolog[] =
	"// generated code, consider not modifying!\n"
	"\n"
	"#ifndef doarr__seen_prolog\n"
	"#define doarr__seen_prolog\n"
	"\n"
	"#ifdef __cplusplus\n"
	"#warning \"This file is not intended to be compiled with a C++ compiler\"\n"
	"#endif\n"
	"\n"
#include "../build/runtime_struct_defs.str.h"
	"\n"
	"#endif\n"
	"\n"
	"static struct guest_file doarr__file_%i;\n" // tentative definition
	"\n"
;
static const char generated_file_fn_entry[] =
	"#ifndef doarr__seen_%2$i_%1$s\n"
	"#define doarr__seen_%2$i_%1$s\n"
	"const struct guest_fn %1$s = {\n"
	" .file = &doarr__file_%2$i,\n"
	" .name = \"%1$s\",\n"
	"};\n"
	"#endif\n"
	"\n"
;
static const char generated_file_epilog[] =
	"\n"
	"static struct guest_file doarr__file_%i = {\n"
	" .gch_data = %s,\n"
	" .gch_data_end = %s + %zu,\n"
	" .compiler_args = doarr__compiler_args,\n"
	" .num_compiler_args = sizeof doarr__compiler_args / sizeof *doarr__compiler_args,\n"
	" .pos_between_args = %i,\n"
	"};\n"
;

static bool generate_c_part1(FILE *out, int file_index, FILE *in, const char *const *compiler_args, bool *have_any_functions) {
	char *ident = dcc_scan_up_to_next_export(in);
	if(!ident) { // null = error
		return false;
	}
	if(!*ident) { // "" = clean eof
		*have_any_functions = false;
		return true;
	}

	fprintf(out, generated_file_prolog, file_index);

	do {
		_Pragma("GCC diagnostic push");
		_Pragma("GCC diagnostic ignored \"-Wformat\"");
		fprintf(out, generated_file_fn_entry, ident, file_index);
		_Pragma("GCC diagnostic pop");
		free(ident);

		ident = dcc_scan_up_to_next_export(in);
		if(!ident) // null = error
			return false;
	} while(*ident); // "" = clean eof

	fputs(
		"#ifndef doarr__seen_args\n"
		"#define doarr__seen_args\n"
		"static const char *const doarr__compiler_args[] = {\n"
	, out);
	for(const char * const*argp = compiler_args; *argp; argp++) {
		fputs(" \"", out);
		for(const char *p = *argp; *p; p++) {
			if(*p == '?' && p[1] == '?')
				fputs("?\"\"", out);
			else if(*p == '\"' || *p == '\\')
				fputc('\\', out), fputc(*p, out);
			else if(*p >= 32 && *p < 127)
				fputc(*p, out);
			else
				fprintf(out, "\\x%02x", (unsigned) (unsigned char) *p);
		}
		fputs("\",", out);
	}
	fputs("\n};\n#endif\n", out);

	*have_any_functions = true;
	return true;
}

static void generate_c_part2_bin(FILE *out, int file_index, size_t data_size, int pos_between_args) {
	static const char data_symbol[] = "_binary_"DOARR_PRECOMPILED"_start";
	fprintf(out, "\nextern const unsigned char %s[];\n", data_symbol);
	fprintf(out, generated_file_epilog, file_index, data_symbol, data_symbol, data_size, pos_between_args);
}

static void generate_c_part2_txt(FILE *out, int file_index, const unsigned char *data, size_t data_size, int pos_between_args) {
	static const char data_symbol_f[] = DOARR_PRECOMPILED"_%i";
	char data_symbol[sizeof data_symbol_f + 3 * sizeof file_index];
	sprintf(data_symbol, data_symbol_f, file_index);
	fprintf(out, "\nstatic const unsigned char %s[%zu];\n", data_symbol, data_size); // tentative definition
	fprintf(out, generated_file_epilog, file_index, data_symbol, data_symbol, data_size, pos_between_args);
	fprintf(out, "\nstatic const unsigned char %s[%zu] = \"\\\n", data_symbol, data_size);
	enum {
		bytes_per_line = 32, // bytes of input
		line_len = 4*bytes_per_line, // chars of output - 4 chars encode 1 byte
	};
	for(size_t y = 0; y < data_size/bytes_per_line; y++) {
		char line[line_len + 2]; // 2 for backslash newline
		char *p = line;
		for(int x = 0; x < bytes_per_line; x++) {
			unsigned char byte = data[y*bytes_per_line + x];
			*p++ = '\\';
			*p++ = '0' + (byte>>6 & 7);
			*p++ = '0' + (byte>>3 & 7);
			*p++ = '0' + (byte>>0 & 7);
		}
		*p++ = '\\';
		*p++ = '\n';
		fwrite(line, sizeof line, 1, out);
	}
	for(size_t i = data_size/bytes_per_line*bytes_per_line; i < data_size; i++) {
		fprintf(out, "\\%03o", data[i]);
	}
	fprintf(out, "\";\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *const common_opts[] = {"-shared", "-fPIC", "-fvisibility=hidden", "-O3"};
static const char *const export_finder_opts[] = {"-E", "-P", "-x" "c++-header"};
static const char *const precompiler_opts[] = {"-x" "c++-header"};

static size_t zmax(size_t a, size_t b) {
	return a > b ? a : b;
}

static size_t max_cxx_args(const struct config *config) {
	return
		+ 1   // program name
		+ 1   // const char *argv1, const char *argv2
		+ LEN(common_opts)
		+ zmax(LEN(export_finder_opts), LEN(precompiler_opts)) // one of them gets passed as specific_opts
		+ (config->num_compiler_args + 1)  // compiler_args, with the input_file.name mixed inbetween them
		+ 1   // terminating null pointer
	;
}

static noreturn void exec_cxx(const struct config *config, const char *argv1, const char *argv2, /*common opts,*/ const char *const *specific_opts, int num_specific_opts, struct input_file input_file, const char **arg_buff) {
	const struct compiler_arg *compiler_args = config->compiler_args;
	int num_compiler_args = config->num_compiler_args;

	const char **arg_ptr = arg_buff + 1;
	if(argv1)
		*arg_ptr++ = argv1;
	if(argv2)
		*arg_ptr++ = argv2;
	for(int i = 0; i < LEN(common_opts); i++)
		*arg_ptr++ = common_opts[i];
	for(int i = 0; i < num_specific_opts; i++)
		*arg_ptr++ = specific_opts[i];
	for(int i = 0; i < input_file.pos_between_opts; i++)
		*arg_ptr++ = compiler_args[i].arg;
	*arg_ptr++ = input_file.name;
	for(int i = input_file.pos_between_opts; i < num_compiler_args; i++)
		*arg_ptr++ = compiler_args[i].arg;
	*arg_ptr = NULL;

	if(config->verbose)
		dcc_err_a("Executing compiler:", arg_buff);

	dcc_fexecv(config->tools[ToolCXX].exec_fd, arg_buff);
}

static void build_runtime_args(const struct config *config, const char **arg_buff, int *pos_between_rt_args) {
	const struct compiler_arg *compiler_args = config->compiler_args;
	int num_compiler_args = config->num_compiler_args;

	*pos_between_rt_args += 1 + LEN(common_opts);

	const char **arg_ptr = arg_buff + 1;
	for(int i = 0; i < LEN(common_opts); i++)
		*arg_ptr++ = common_opts[i];
	for(int i = 0; i < num_compiler_args; i++)
		if(compiler_args[i].also_runtime)
			*arg_ptr++ = compiler_args[i].arg;
		else
			*pos_between_rt_args -= 1;
	*arg_ptr = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum { Child = 0, Err = -1 };

#define TBA (const char *) (size_t) !!

static bool run_tool(const struct config *config, enum tool_id ti, const char **args) {
	struct tool tool = config->tools[ti];
	pid_t pid = fork();
	switch(pid) {
		case Child:
			if(fchdir(config->tmp_fd))
				dcc_perror(RT_ERR "run_tool fchdir"), _exit(127);
			*args = tool.name;
			if(config->verbose)
				dcc_err_a("Executing tool:", args);
			dcc_fexecv(tool.exec_fd, args);
		case Err:
			dcc_perror_s(RT_ERR "run_tool fork", *args);
			return false;
	}
	return dcc_wait(&pid, tool.name);
}

static pid_t start_precompiler(const struct config *config, struct input_file file, const char **arg_buff, const char *out_name) {
	pid_t pid = fork();
	switch(pid) {
		case Child:
			exec_cxx(config, "-o", out_name, /*common opts,*/ precompiler_opts, LEN(precompiler_opts), file, arg_buff);
		case Err:
			dcc_perror(RT_ERR "precom fork");
			return Err;
	}
	return pid;
}

static pid_t start_preprocessor(const struct config *config, struct input_file file, const char **arg_buff, FILE **out_out_file) {
	bool discard_stderr = !config->verbose;
	enum { ReadEnd, WriteEnd };
	int pp[2];
	if(pipe(pp)) {
		dcc_perror(RT_ERR "preproc pipe");
		return Err;
	}
	pid_t pid = fork();
	switch(pid) {
		case Child:
			if(dup2(pp[WriteEnd], STDOUT_FILENO) < 0)
				dcc_perror(RT_ERR "preproc dup2 stdout"), _exit(127);
			if(discard_stderr && dup2(config->dev_null, STDERR_FILENO) < 0)
				dcc_perror(RT_ERR "preproc dup2 stderr"), _exit(127);
			close(pp[ReadEnd]);
			close(pp[WriteEnd]);
			exec_cxx(config, NULL, NULL, /*common opts,*/ export_finder_opts, LEN(export_finder_opts), file, arg_buff);
		case Err:
			dcc_perror(RT_ERR "preproc fork");
			dcc_close(pp[ReadEnd], "preproc read");
			dcc_close(pp[WriteEnd], "preproc write");
			return Err;
	}
	if(close(pp[WriteEnd])) {
		dcc_perror(RT_ERR "preproc close");
		// cannot ignore this - we would never get EOF - return early
		return pid;
	}
	if(!(*out_out_file = fdopen(pp[ReadEnd], "r"))) {
		dcc_perror(RT_ERR "preproc fdopen");
	}
	return pid;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool process_single_file(const struct config *config, int file_index, struct input_file file, const char **arg_buff, const char *out_filename) {
	static const char tmp_pch[] = DOARR_PRECOMPILED;
	static const char tmp_c[] = "tmp.c";
	static const char tmp_o[] = "tmp.o";

	pid_t precompiler_pid = -1;
	pid_t preprocessor_pid = -1;
	FILE *c_file = NULL;
	FILE *preprocessor_output = NULL;

	precompiler_pid = start_precompiler(config, file, arg_buff, config->tmp_pch_abs);
	if(precompiler_pid < 0)
		goto error;

	preprocessor_pid = start_preprocessor(config, file, arg_buff, &preprocessor_output);
	if(preprocessor_pid < 0 || !preprocessor_output)
		goto error;

	assert(config->preproc ^ config->compile);
	const char *c_file_name = config->preproc ? out_filename : tmp_c;
	c_file = dcc_fopenat(config->tmp_fd, c_file_name, O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW, 0400);

	bool have_any_functions;

	int pos_between_rt_args = file.pos_between_opts;
	build_runtime_args(config, arg_buff, &pos_between_rt_args);
	if(config->verbose)
		dcc_err_a("Runtime compiler args:", arg_buff);
	if(!generate_c_part1(c_file, file_index, preprocessor_output, arg_buff, &have_any_functions))
		goto error;

	if(!dcc_wait(&preprocessor_pid, "preprocessor"))
		goto error;

	if(!dcc_wait(&precompiler_pid, "precompiler"))
		goto error;

	if(!have_any_functions) {
		if(!config->nowarn)
			dcc_errf("warning: no exported functions found in '%s'", file.name);

		fclose(c_file);
		c_file = NULL;

		if(!dcc_unlink(config->tmp_fd, tmp_pch))
			goto error;

		if(config->preproc)
			return true;

		if(!dcc_unlink(config->tmp_fd, tmp_c))
			goto error;

		if(!run_tool(config, ToolLD, (const char *[]) {
			TBA "ld", "-r",
			"/dev/null",
			"-o", out_filename,
			NULL,
		})) goto error;

		return true;
	}

	if(config->preproc) {
		const void *data;
		size_t size;
		if(!dcc_map(config->tmp_fd, tmp_pch, &data, &size))
			goto error;
		if(!dcc_unlink(config->tmp_fd, tmp_pch))
			goto error;
		generate_c_part2_txt(c_file, file_index, data, size, pos_between_rt_args);
		dcc_unmap(data, size);
	} else {
		struct stat statbuf;
		if(fstatat(config->tmp_fd, tmp_pch, &statbuf, 0)) {
			dcc_perror_s(RT_ERR "fstatat", tmp_pch);
			goto error;
		}
		generate_c_part2_bin(c_file, file_index, statbuf.st_size, pos_between_rt_args);
	}

	if(true) {
		bool err = ferror(c_file) | fclose(c_file);
		c_file = NULL;
		if(err) {
			dcc_errf(RT_ERR "error writing '%s'", c_file_name);
			goto error;
		}
	}

	if(config->preproc) {
		return true;
	}

	if(!run_tool(config, ToolCC, (const char *[]) {
		TBA "cc", "-c",
		"-xc", tmp_c,
		"-o", tmp_o,
		NULL,
	})) goto error;

	if(!dcc_unlink(config->tmp_fd, tmp_c))
		goto error;

	if(!run_tool(config, ToolLD, (const char *[]) {
		TBA "ld", "-r",
		tmp_o,
		"-b" "binary", tmp_pch,
		"-o", out_filename,
		"-z" "noexecstack",
		NULL,
	})) goto error;

	if(!dcc_unlink(config->tmp_fd, tmp_o))
		goto error;
	if(!dcc_unlink(config->tmp_fd, tmp_pch))
		goto error;

	if(!run_tool(config, ToolObjcopy, (const char *[]) {
		TBA "objcopy",
		"-L" "_binary_"DOARR_PRECOMPILED"_start",
		"-N" "_binary_"DOARR_PRECOMPILED"_end",
		"-N" "_binary_"DOARR_PRECOMPILED"_size",
		"-g",
		out_filename,
		NULL,
	})) goto error;

	return true;

error:
	dcc_errf("error while processing '%s'", file.name);
	if(c_file)
		fclose(c_file);
	if(preprocessor_output)
		fclose(preprocessor_output);
	if(preprocessor_pid != -1)
		dcc_wait(&preprocessor_pid, "preprocessor");
	if(precompiler_pid != -1)
		dcc_wait(&precompiler_pid, "precompiler");
	dcc_unlink_if_ex(config->tmp_fd, tmp_pch);
	dcc_unlink_if_ex(config->tmp_fd, tmp_c);
	dcc_unlink_if_ex(config->tmp_fd, tmp_o);
	dcc_unlink_if_ex(config->tmp_fd, out_filename);
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool dcc_unlink_all(int dir_fd, int num_files, char names[num_files][TmpOutputNameLen]) {
	bool ok = true;
	for(int i = 0; i < num_files; i++)
		ok &= dcc_unlink(dir_fd, names[i]);
	return ok;
}

static bool cat_unlink_close(int dir_fd, int num_files, char names[num_files][TmpOutputNameLen], int out_fd, const char *msg) {
	for(int i = 0; i < num_files; i++) {
		const void *data;
		size_t size;
		if(!dcc_map(dir_fd, names[i], &data, &size)) {
			goto error_early;
		}
		if(!dcc_unlink(dir_fd, names[i])) {
			goto error_late;
		}
		if(!dcc_write(out_fd, data, size)) {
			dcc_perror_s("cannot write output: write", msg);
			goto error_late;
		}
		dcc_unmap(data, size);
		continue;
	error_late:
		dcc_unmap(data, size);
		i++;
	error_early:
		dcc_unlink_all(dir_fd, num_files - i, names + i);
		close(out_fd);
		return false;
	}
	if(close(out_fd)) {
		dcc_perror_s("cannot write output: close", msg);
		return false;
	}
	return true;
}

static bool move_from_tmp(int src_dir_fd, char src_name_ptr[1][TmpOutputNameLen], const char *dest_name) {
	if(renameat(src_dir_fd, *src_name_ptr, AT_FDCWD, dest_name)) {
		int dest_fd = open(dest_name, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0666);
		if(dest_fd < 0) {
			dcc_perror_s("cannot write output: open", *src_name_ptr);
			dcc_unlink(src_dir_fd, *src_name_ptr);
			return false;
		}
		return cat_unlink_close(src_dir_fd, 1, src_name_ptr, dest_fd, *src_name_ptr);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t dcc_gen_required_arg_buff_size(const struct config *config, int num_files) {
	size_t for_cxx = max_cxx_args(config);

	if(config->compile && config->output) {
		// we will also need to run the linker to combine to single output
		size_t for_ld =
			+ 2   // "ld", "-r"
			+ num_files
			+ 2   // "-o", output
			+ 1   // terminating null pointer
		;
		return zmax(for_cxx, for_ld);
	} else {
		return for_cxx;
	}
}

bool dcc_gen_main(const struct config *config, int num_files, const struct input_file input_files[num_files], char tmp_output_files[num_files][TmpOutputNameLen], const char **arg_buff) {
	for(int i = 0; i < num_files; i++) {
		if(!process_single_file(config, i, input_files[i], arg_buff, tmp_output_files[i])) {
			dcc_unlink_all(config->tmp_fd, i, tmp_output_files);
			return false;
		}
	}

	assert(config->preproc ^ config->compile);

	if(config->output) {
		if(config->compile) {
			// combine object files using ld
			char tmp_output_combined[TmpOutputNameLen] = "all.o";

			const char **argp = arg_buff;
			*argp++ = TBA "ld"; // run_tool will fill in program name
			*argp++ = "-r"; // relocatable
			for(int i = 0; i < num_files; i++)
				*argp++ = tmp_output_files[i];
			*argp++ = "-o"; // output
			*argp++ = tmp_output_combined;
			*argp++ = NULL;

			bool ok = run_tool(config, ToolLD, arg_buff);
			ok &= dcc_unlink_all(config->tmp_fd, num_files, tmp_output_files);

			if(!ok) {
				dcc_unlink_if_ex(config->tmp_fd, tmp_output_combined);
				return false;
			}

			return move_from_tmp(config->tmp_fd, &tmp_output_combined, config->output);
		} else {
			int out_fd = open(config->output, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0666);
			if(out_fd < 0) {
				dcc_perror_s("cannot write output: open", config->output);
				dcc_unlink_all(config->tmp_fd, num_files, tmp_output_files);
				return false;
			}
			return cat_unlink_close(config->tmp_fd, num_files, tmp_output_files, out_fd, config->output);
		}
	} else {
		if(config->compile) {
			bool ok = true;
			for(int i = 0; i < num_files; i++) {
				static const char ext[] = ".o";
				const char *input_name = my_basename(input_files[i].name);
				assert(*input_name);
				const char *dot = strrchr(input_name + 1, '.');
				size_t len_noext = dot ? (size_t)(dot - input_name) : strlen(input_name);
				char output_name[len_noext + sizeof ext]; // VLA, but usually limited to 256 bytes
				memcpy(output_name, input_name, len_noext);
				memcpy(output_name + len_noext, ext, sizeof ext);
				if(!move_from_tmp(config->tmp_fd, &tmp_output_files[i], output_name))
					ok = false;
			}
			return ok;
		} else {
			return cat_unlink_close(config->tmp_fd, num_files, tmp_output_files, STDOUT_FILENO, "stdout");
		}
	}
}
