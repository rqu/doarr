#ifndef DOARR_GUEST_FN_WRAPPER_HPP_
#define DOARR_GUEST_FN_WRAPPER_HPP_

/*
 * User-friendly wrapper for doarr::internal::guest_fn (guest_fn_.hpp).
 * This header contains a bare minimum definition, the rest is in import.hpp.
 * Instances of the wrapper are created in the generated .pch.cpp files
 * and thus need at least this definition available despite not using the member functions.
 */

#include "guest_fn_.hpp"

namespace doarr {

class guest_fn : internal::guest_fn {
	struct expr_pack;
	class instance;

public:
	explicit constexpr guest_fn(internal::guest_fn base) : internal::guest_fn(base) {}

	void operator()(auto&&...) const;

	auto call_v(auto&&...) const;

	auto call_r(auto&&...) const;

#ifdef __cpp_multidimensional_subscript
	instance operator[](auto&&...) const;
#endif

	instance operator[](expr_pack) const;
};

}

#endif
