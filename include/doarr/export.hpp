#ifndef DOARR_EXPORT_HPP_
#define DOARR_EXPORT_HPP_

/*
 * To be included by guest source files.
 */

// A function is appended to each source file at runtime,
// which requires the definition of `union any` (used to pass dynamic values)
// and noarr proto-structures, so we include them here.
#include "any_.hpp"

// define DOARR_EXPORT as empty macro unless already defined otherwise via commandline
#ifndef DOARR_EXPORT
#define DOARR_EXPORT
#endif

#endif
