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
		explicit instance(const instance &) = delete;
		explicit instance(instance &&) = delete;
		void operator =(const instance &) = delete;
		void operator =(instance &&) = delete;

	public:
		void operator()(auto&&... args) && {
			call(fn, true, std::move(tmpl_args), exprs{decltype(args)(args).to_expr()...});
		}

		friend imported;
	};

	explicit imported(const imported &) = delete;
	explicit imported(imported &&) = delete;
	void operator =(const imported &) = delete;
	void operator =(imported &&) = delete;

public:
	void operator()(auto&&... args) const {
		call(this, false, exprs{}, exprs{decltype(args)(args).to_expr()...});
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
