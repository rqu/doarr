#ifndef DOARR_IMPORT_HPP_
#define DOARR_IMPORT_HPP_

/*
 * To be included by client files that launches runtime-compiled code.
 */

#include "call_.hpp"
#include "guest_fn_wrapper_.hpp"

#include <utility>
#include <vector>

struct doarr::guest_fn::expr_pack : exprs {
	constexpr expr_pack(auto&&... items) : exprs{decltype(items)(items)...} {}
};

class doarr::guest_fn::instance {
	const guest_fn *fn;
	exprs tmpl_args;

public:
	// implicit - to allow construction using just braces (or even without braces in case of single value)
	explicit instance(const guest_fn *fn, exprs &&tmpl_args) : fn(fn), tmpl_args(std::move(tmpl_args)) {}

	void operator()(auto&&... args) && {
		call_void(fn, true, std::move(tmpl_args), exprs{decltype(args)(args).to_expr()...});
	}

	auto call_v(auto&&... args) && {
		// TODO call function and return value
		static_assert(sizeof...(args) < 0, "not implemented yet");
		return nullptr;
	}

	auto call_r(auto&&... args) && {
		// TODO call function and return reference
		static_assert(sizeof...(args) < 0, "not implemented yet");
		return nullptr;
	}
};


void doarr::guest_fn::operator()(auto&&... args) const {
	call_void(this, false, exprs{}, exprs{decltype(args)(args).to_expr()...});
}

auto doarr::guest_fn::call_v(auto&&... args) const {
	// TODO call function and return value
	static_assert(sizeof...(args) < 0, "not implemented yet");
	return nullptr;
}

auto doarr::guest_fn::call_r(auto&&... args) const {
	// TODO call function and return reference
	static_assert(sizeof...(args) < 0, "not implemented yet");
	return nullptr;
}

#ifdef __cpp_multidimensional_subscript
doarr::guest_fn::instance doarr::guest_fn::operator[](auto&&... args) const {
	return instance{this, exprs{decltype(args)(args).to_expr()...}};
}
#endif

doarr::guest_fn::instance doarr::guest_fn::operator[](expr_pack args) const {
	return instance{this, std::move(args)};
}

namespace doarr {

using imported = guest_fn;

}

#endif
