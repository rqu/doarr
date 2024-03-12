#ifndef DOARR_ANY_HPP_
#define DOARR_ANY_HPP_

/*
 * Union used to pass dynamic values to guest functions.
 */

#include <cstddef>

namespace doarr::internal {

union any {
	std::size_t i;
	double f;
	void *p;
};

}

#endif
