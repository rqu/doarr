#ifndef GUEST_FILE_H_
#define GUEST_FILE_H_

/*
 * C structure written by .pch.cpp generated files and read by io.c.
 */

#include "common.h"

struct guest_file {
	const unsigned char *gch_data;
	const unsigned char *gch_data_end;
	const char *const *compiler_args;
	int num_compiler_args;
	int pos_between_args;
	struct tmp_path gch_tmp_path;
};

#endif
