#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dcc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum option_syntax {
	NoArg = 0,
	Immediate = 1,
	Separate = 2,
	ImmOrSep = Immediate|Separate,
};

struct opt_action_param {
	struct config *config;
	const char *value;
	char *const *argp;
	bool separate;
};

typedef void(*opt_action_t)(struct opt_action_param);

struct option_key {
	const char *str;
	enum option_syntax syntax;
	const char *desc;
	opt_action_t action;
};

//

static void help(void);

static void fwd(struct config *config, const char *arg, bool also_runtime) {
	config->compiler_args[config->num_compiler_args++] = (struct compiler_arg) { arg, also_runtime };
}

static void reject_arg(struct config *config, const char *arg) {
	dcc_errf("unsupported option '%s'", arg);
	config->invalid = true;
}

static void forward(struct opt_action_param p) {
	fwd(p.config, p.argp[0], true);
	if(p.separate)
		fwd(p.config, p.argp[1], true);
}

static void forward_no_rt(struct opt_action_param p) {
	fwd(p.config, p.argp[0], false);
	if(p.separate)
		fwd(p.config, p.argp[1], false);
}

static void reject(struct opt_action_param p) {
	reject_arg(p.config, *p.argp);
}

static void set_output(struct opt_action_param p) {
	if(p.config->output) {
		dcc_err("too many '-o' options");
		p.config->invalid = true;
	} else {
		p.config->output = p.value;
	}
}

static void set_compile(struct opt_action_param p) {
	p.config->compile = true;
}

static void set_preproc(struct opt_action_param p) {
	p.config->preproc = true;
}

static void set_nowarn(struct opt_action_param p) {
	p.config->nowarn = true;
	forward(p);
}

static void set_verbose(struct opt_action_param p) {
	p.config->verbose = true;
	forward(p);
}

static void dummy_set_language(struct opt_action_param p) {
	bool value_is_cxx = !strcmp(p.value, "c++");
	if(value_is_cxx) {
		// ignore
	} else {
		dcc_errf("unsupported language '%s'", p.value);
		p.config->invalid = true;
	}
}

static void help_and_exit(struct opt_action_param p) {
	(void) p;
	help();
	exit(0);
}

//

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
const struct option_key option_keys[] = {
	{"-c", NoArg, "compile to object file (do not link)", set_compile},
	{"-d", Immediate, "compiler debugging", reject},
	{"-e", ImmOrSep, "entry point", reject},
	{"-f", Immediate, "various options", forward},
	{"-g", Immediate, "debug", forward},
	{"-h", ImmOrSep, "undocumented", reject},
	{"-l", ImmOrSep, "library", forward},
	{"-m", Immediate, "machine options", forward},
	{"-n", NoArg, "undocumented", reject},
	{"-o", ImmOrSep, "output file", set_output},
	{"-p", NoArg, "profiling", reject}, {"-pg", NoArg, "profiling", reject}, // TODO maybe allow
	{"-r", NoArg, "relocatable", reject},
	{"-s", NoArg, "strip symbols", reject},
	{"-t", NoArg, "linker trace", forward},
	{"-u", ImmOrSep, "undefine symbol", forward},
	{"-v", NoArg, "verbose", set_verbose},
	{"-w", NoArg, "disable warnings", set_nowarn},
	{"-x", ImmOrSep, "language", dummy_set_language},
	{"-z", ImmOrSep, "linker keyword", forward},
	{0},
	{"-A", ImmOrSep, "assertion", forward},
	{"-B", ImmOrSep, "compiler installation prefix", forward},
	{"-C", NoArg}, {"-CC", NoArg, "keep comments", forward},
	{"-D", ImmOrSep, "define macro", forward},
	{"-E", NoArg, "preprocess only (compile to C)", set_preproc},
	{"-F", ImmOrSep, "framework dir", forward},
	{"-H", NoArg, "verbose #include", forward},
	{"-I", ImmOrSep, "include path", forward},
	{"-J", ImmOrSep, "undocumented", reject},
	{"-L", ImmOrSep, "library path", forward},
	{"-M", Immediate, "rules for make", reject},
	{"-N", NoArg, "undocumented", reject},
	{"-O", Immediate, "optimization", forward},
	{"-P", NoArg, "no linemarkers", forward},
	{"-Q", NoArg, "compiler stats", forward},
	{"-R", ImmOrSep, "undocumented", reject},
	{"-S", NoArg, "produce assembly", reject},
	{"-T", ImmOrSep, "linker script", reject},
	{"-U", ImmOrSep, "undefine macro", forward},
	{"-W", Immediate, "warnings", forward},
	{"-X", NoArg, "undocumented", reject},
	{"-Z", NoArg, "unknown linker flag", reject},
	{0},
	{"-iquote", ImmOrSep}, {"-iprefix", ImmOrSep}, {"-iwithprefix", ImmOrSep}, {"-iwithprefixbefore", ImmOrSep}, {"-isysroot", ImmOrSep}, {"-imultilib", ImmOrSep, "include path", forward_no_rt},
	{"-include", ImmOrSep}, {"-imacros", ImmOrSep, "include file", forward_no_rt},
	{0},
	{"-std", Immediate}, {"--std", Immediate, "language standard version"},
	{"-pedantic", NoArg}, {"-pedantic-errors", NoArg, "language standard pedantry"},
	{0},
	{"-Wp,", Immediate}, {"-Xpreprocessor", Separate, "preprocessor option", forward},
	{"-Wl,", Immediate}, {"-Xlinker", Separate, "linker option", forward},
	{"-Wa,", Immediate}, {"-Xassembler", Separate, "assembler option", forward},
	{0},
	{"-help", Immediate}, {"--help", Immediate, "display help", help_and_exit},
};
#pragma GCC diagnostic pop

static bool startswith(const char *full, const char *prefix) {
	while(*prefix) {
		if(*full++ != *prefix++)
			return false;
	}
	return true;
}

static struct option_key lookup_opt(const char *arg) {
	const char *desc = desc;
	opt_action_t action = action;
	for(int i = LEN(option_keys) - 1; i >= 0; i--) {
		struct option_key curr = option_keys[i];
		if(!curr.str)
			continue;
		if(curr.desc)
			desc = curr.desc;
		if(curr.action)
			action = curr.action;
		if(startswith(arg, curr.str))
			return (struct option_key) { curr.str, curr.syntax, desc, action, };
	}
	return (struct option_key) {0};
}

static void help(void) {
	puts("Example usage:");
	puts("  dcc -c INPUT_FILES...");
	puts("    Compile to object files, one per INPUT_FILE. Each output");
	puts("    file name is created by replacing the extension with '.o'.");
	puts("  dcc -c INPUT_FILES... -o OUTPUT_FILE");
	puts("    Compile to single object file.");
	puts("  dcc -E INPUT_FILES...");
	puts("    Compile to C code, written to standard output.");
	puts("  dcc -E INPUT_FILES... -o OUTPUT_FILE");
	puts("    Compile to C code, written to OUTPUT_FILE.");
	puts("");
	puts("Using '-c' will generally produce a large file, which may");
	puts("be slow to process later (although runtime is not affected).");
	puts("As such, '-E' should be preferred over '-c'.");
	puts("");
	puts("Most other C++ compiler options may be used as well.");
	puts("They are generally just forwarded to the C++ compiler.");
	puts("");
	puts("Summary of supported options:");
	for(int i = 0; i < LEN(option_keys); i++) {
		struct option_key key = option_keys[i];
		if(!key.str) {
			puts("");
			continue;
		}
		if(key.action == reject) {
			continue;
		}
		switch(key.syntax) {
			case NoArg:
				printf("  %s\n", key.str);
				break;
			case Immediate:
				printf("  %s...\n", key.str);
				break;
			case ImmOrSep:
			case Separate:
				printf("  %s ...\n", key.str);
				break;
			default:
				abort();
		}
		if(key.desc) {
			printf("    %s\n", key.desc);
		}
	}
	puts("");
	puts("Supported environment variables:");
	for(int i = 0; i < NumEnvVars; i++) {
		printf("  %s (defaults to '%s')\n", env_vars[i].name, env_vars[i].default_value);
		printf("    %s\n", env_vars[i].desc);
	}
}

void dcc_parse_args(char **argp, struct config *config, struct input_file *out_inputs, int *out_num_inputs) {
	int num_inputs = 0;
	for(char *arg; arg = *argp; argp++) {
		if(*arg == '@') {
			reject_arg(config, arg);
			dcc_err("@file arguments are not implemented");
			continue;
		}
		if(*arg == '-') {
			struct option_key key = lookup_opt(arg);
			if(key.str == NULL) {
				reject_arg(config, arg);
				continue;
			}
			char *value = arg + strlen(key.str);
			switch(key.syntax) {
				case Immediate:
					key.action((struct opt_action_param){config, value, argp, .separate = false});
					continue;
				case ImmOrSep:
					if(*value)
						key.action((struct opt_action_param){config, value, argp, .separate = false});
					else
						goto value_in_next_arg;
					continue;
				case Separate:
					if(*value)
						reject_arg(config, arg);
					else
						goto value_in_next_arg;
					continue;
				case NoArg:
					if(*value)
						reject_arg(config, arg);
					else
						key.action((struct opt_action_param){config, NULL, argp, .separate = false});
					continue;
				value_in_next_arg:
					if(value = argp[1]) {
						key.action((struct opt_action_param){config, value, argp, .separate = true});
						argp++; // one more shift in addition to the for-loop-increment
					} else {
						dcc_errf("missing value for option '%s' (%s)", key.str, key.desc);
						config->invalid = true;
					}
					continue;
			}
			abort();
		}
		out_inputs[num_inputs++] = (struct input_file) {
			.name = arg,
			.pos_between_opts = config->num_compiler_args,
		};
	}
	*out_num_inputs = num_inputs;
}
