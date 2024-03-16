#ifndef EXPR_UTIL_HPP_
#define EXPR_UTIL_HPP_

/*
 * Local utilities to work with expr and exprs instances. Defined in expr_all.cpp.
 */

#include <doarr/expr_base.hpp>
#include <doarr/any_.hpp>

#include <cstdio>

namespace doarr {

namespace runtime {

struct INTERNAL_VISIBILITY exs {
	static std::size_t num_params(const exprs &es) noexcept;
	static internal::any *extract_params(const exprs &es, internal::any *out) noexcept;
	static std::size_t write_to(const exprs &es, std::FILE *out, std::size_t param_idx) noexcept;
};

//

constexpr std::size_t hash(std::size_t v) noexcept {
	return v;
}

constexpr std::size_t hash_all(const auto &... component) {
	std::size_t r = 1;
	(..., (r *= 31, r += hash(component))); // hash(...) may refer to doarr::hash(expr|exprs) or the above
	return r;
}

}

}

#endif
