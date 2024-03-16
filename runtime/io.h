#ifndef IO_H_
#define IO_H_

/*
 * C functions defined in io.c and used by call.cpp.
 */

#include "common.h"

struct doarr_io_ctx {
	struct tmp_path tmp_path;
};

enum {
	tmp_path_size = sizeof(struct tmp_path),
	tmp_path_len = tmp_path_size - 1,
	tmp_ext_len = 4,
	tmp_ext_size = tmp_ext_len + 1,
	tmp_full_path_len = tmp_path_len + tmp_ext_len,
	tmp_full_path_size = tmp_full_path_len + 1,
};

struct tmp_full_path {
	char chars[tmp_full_path_size];
};

INTERNAL_VISIBILITY int doarr_io_init(struct doarr_io_ctx *ctx);
INTERNAL_VISIBILITY void doarr_tmp_path(struct doarr_io_ctx *ctx, const char ext[tmp_ext_size], struct tmp_full_path *out_path);
INTERNAL_VISIBILITY const char *doarr_extract_precompiled_or_null(struct doarr_io_ctx *ctx, struct guest_file *file);
INTERNAL_VISIBILITY int doarr_compile_and_load(struct doarr_io_ctx *ctx, struct tmp_full_path *cxx_file_name, const struct guest_file *file, void **out_handle, void **out_fn);

#endif
