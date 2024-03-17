#ifndef DOARR_CALL_HPP_
#define DOARR_CALL_HPP_

/*
 * Functions called by import.hpp header-only types and defined in call.cpp.
 */

#include "expr_base.hpp"

#include <exception>

namespace doarr {

// may be thrown by the functions below
struct compilation_error : std::exception {};

namespace internal {

struct guest_fn {
	void *const file; // points to C struct guest_file
	const char *const name;
};

void call_void(const guest_fn *fn, bool have_tmpl_args, exprs &&tmpl_args, exprs &&call_args);

}

}

#endif
