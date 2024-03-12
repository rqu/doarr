#ifndef DOARR_GUEST_FN_HPP_
#define DOARR_GUEST_FN_HPP_

/*
 * Definition of doarr::internal::guest_fn. Instances created by generated .pch.cpp files 
 * and wrapped in doarr::guest_fn (guest_fn_wrapper_.hpp).
 * Unwrapped instances are then used by call.cpp.
 */

namespace doarr::internal {

struct guest_fn {
	void *const file; // points to C struct guest_file
	const char *const name;
};

}

#endif
