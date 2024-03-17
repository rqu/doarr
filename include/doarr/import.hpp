#ifndef DOARR_IMPORT_HPP_
#define DOARR_IMPORT_HPP_

/*
 * To be included by client files that launches runtime-compiled code.
 */

#include "call_.hpp"

#include <utility>

namespace doarr {

class imported : internal::guest_fn {
	struct expr_pack : exprs {
		// implicit - to allow construction using just braces (or even without braces in case of single value)
		constexpr expr_pack(auto&&... items) : exprs{decltype(items)(items)...} {}
	};

	class instance {
		const imported *fn;
		exprs tmpl_args;

		explicit instance(const imported *fn, exprs &&tmpl_args) : fn(fn), tmpl_args(std::move(tmpl_args)) {}

	public:
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

		friend imported;
	};

public:
	void operator()(auto&&... args) const {
		call_void(this, false, exprs{}, exprs{decltype(args)(args).to_expr()...});
	}

	auto call_v(auto&&... args) const {
		// TODO call function and return value
		static_assert(sizeof...(args) < 0, "not implemented yet");
		return nullptr;
	}

	auto call_r(auto&&... args) const {
		// TODO call function and return reference
		static_assert(sizeof...(args) < 0, "not implemented yet");
		return nullptr;
	}

#ifdef __cpp_multidimensional_subscript
	instance operator[](auto&&... args) const {
		return instance{this, exprs{decltype(args)(args).to_expr()...}};
	}
#endif

	instance operator[](expr_pack args) const {
		return instance{this, std::move(args)};
	}
};

}

#endif
