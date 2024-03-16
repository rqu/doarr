#ifndef GUEST_FILE_H_
#define GUEST_FILE_H_

/*
 * C structure written by .pch.cpp generated files and read by io.c.
 * Requires <stddef.h> to be included!
 */

#include "common.h"

struct guest_file {
	const unsigned char *gch_data;
	size_t gch_size;
	const char *const *compiler_args;
	size_t num_compiler_args;
	struct tmp_path gch_tmp_path;
};

#endif
