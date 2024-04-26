#ifndef DOARR_EXPR_HPP_
#define DOARR_EXPR_HPP_

/*
 * User-friendly header-only strongly-typed wrappers for expr_base.hpp and expr_ctors.hpp.
 */

#include "expr_ctors.hpp"

namespace doarr {

template<typename T>
constexpr expr &&as_expr(T &&t) {
	return decltype(t)(t);
}

template<typename T>
constexpr const expr &as_expr(const T &t) {
	return decltype(t)(t);
}

template<typename T, typename Expected = expr>
concept Expr = requires(T t) {
	(as_expr<Expected>)((T &&) t);
};

//

struct num : expr {
	explicit num(Expr auto &&e) : expr(decltype(e)(e)) {}
	num(std::size_t v) : expr(int_expr(v)) {}
};

template<Expr Ret, Expr... Args>
struct fn : expr {
	explicit fn(Expr auto &&e) : expr(decltype(e)(e)) {}
	Ret operator()(Expr<Args> auto &&... args) const & {
		return Ret{call_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
	Ret operator()(Expr<Args> auto &&... args) && {
		return Ret{call_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
};

template<Expr Ret, Expr Args>
struct var_fn : expr {
	explicit var_fn(Expr auto &&e) : expr(decltype(e)(e)) {}
	Ret operator()(Expr<Args> auto &&... args) const & {
		return Ret{call_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
	Ret operator()(Expr<Args> auto &&... args) && {
		return Ret{call_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
};

template<Expr Ret, Expr... Args>
struct tmpl : expr {
	explicit tmpl(Expr auto &&e) : expr(decltype(e)(e)) {}
#ifdef __cpp_multidimensional_subscript
	Ret operator[](Expr<Args> auto &&... args) const & {
		return Ret{inst_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
	Ret operator[](Expr<Args> auto &&... args) && {
		return Ret{inst_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
#endif
private:
	struct args_pack : exprs {
		// implicit - to allow construction using just braces (or even without braces in case of single Args)
		constexpr args_pack(Expr<Args> auto &&... args) : exprs{(as_expr<Args>)(decltype(args)(args))...} {}
	};
public:
	Ret operator[](args_pack args) const & {
		return Ret{inst_expr(std::move(*this), std::move(args))};
	}
	Ret operator[](args_pack args) && {
		return Ret{inst_expr(std::move(*this), std::move(args))};
	}
};

template<Expr Ret, Expr Args>
struct var_tmpl : expr {
	explicit var_tmpl(Expr auto &&e) : expr(decltype(e)(e)) {}
#ifdef __cpp_multidimensional_subscript
	Ret operator[](Expr<Args> auto &&... args) const & {
		return Ret{inst_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
	Ret operator[](Expr<Args> auto &&... args) && {
		return Ret{inst_expr(std::move(*this), exprs{(as_expr<Args>)(decltype(args)(args))...})};
	}
#endif
private:
	struct args_pack : exprs {
		// implicit - to allow construction using just braces (or even without braces in case of single Args)
		constexpr args_pack(Expr<Args> auto &&... args) : exprs{(as_expr<Args>)(decltype(args)(args))...} {}
	};
public:
	Ret operator[](args_pack args) const & {
		return Ret{inst_expr(std::move(*this), std::move(args))};
	}
	Ret operator[](args_pack args) && {
		return Ret{inst_expr(std::move(*this), std::move(args))};
	}
};

struct dim : expr {
	explicit dim(Expr auto &&e) : expr(decltype(e)(e)) {}
	dim(char c) : expr(char_expr(c)) {}
};

struct proto_struct : expr {
	explicit proto_struct(Expr auto &&e) : expr(decltype(e)(e)) {}
	friend proto_struct operator^(Expr<proto_struct> auto &&inner, Expr<proto_struct> auto &&outer) {
		return proto_struct{infix_expr(infix_xor, (as_expr<proto_struct>)(decltype(inner)(inner)), (as_expr<proto_struct>)(decltype(outer)(outer)))};
	}
};

struct noarr_struct : expr {
	explicit noarr_struct(Expr auto &&e) : expr(decltype(e)(e)) {}
	friend proto_struct operator^(Expr<noarr_struct> auto &&inner, Expr<proto_struct> auto &&outer) {
		return proto_struct{infix_expr(infix_xor, (as_expr<noarr_struct>)(decltype(inner)(inner)), (as_expr<proto_struct>)(decltype(outer)(outer)))};
	}
};

struct type : expr {
	explicit type(Expr auto &&e) : expr(decltype(e)(e)) {}
	type(const char *qname) : expr(qname_expr(qname)) {}
};

struct ptr : expr {
	explicit ptr(Expr auto &&e) : expr(decltype(e)(e)) {}
	ptr(void *value) : expr(dyn_expr(value)) {}
};

//

inline num dyn(std::size_t value) {
	return num{dyn_expr(value)};
}

//

template<Expr... Args>
struct fn_from {
	fn_from() = delete;
	template<Expr Ret>
	using to = fn<Ret, Args...>;
};

template<Expr... Args>
struct tmpl_from {
	tmpl_from() = delete;
	template<Expr Ret>
	using to = tmpl<Ret, Args...>;
};

template<Expr Args>
struct var_fn_from {
	var_fn_from() = delete;
	template<Expr Ret>
	using to = var_fn<Ret, Args>;
};

template<Expr Args>
struct var_tmpl_from {
	var_tmpl_from() = delete;
	template<Expr Ret>
	using to = var_tmpl<Ret, Args>;
};

//

const struct {
	const tmpl_from<num>::to<num> lit {qname_expr("noarr::lit")};
	const tmpl_from<type>::to<fn_from<>::to<noarr_struct>> scalar {qname_expr("noarr::scalar")};
	const tmpl_from<dim>::to<fn_from<>::to<proto_struct>> vector {qname_expr("noarr::vector")};
	const tmpl_from<dim>::to<fn_from<num>::to<proto_struct>> sized_vector {qname_expr("noarr::sized_vector")};
	const var_tmpl_from<dim>::to<var_fn_from<num>::to<proto_struct>> bcast {qname_expr("noarr::bcast")};
	const var_tmpl_from<dim>::to<fn_from<>::to<proto_struct>> hoist {qname_expr("noarr::hoist")};
	const var_tmpl_from<dim>::to<var_fn_from<num>::to<proto_struct>> set_length {qname_expr("noarr::set_length")};
	const tmpl_from<dim, dim, dim>::to<fn_from<num>::to<proto_struct>> into_blocks {qname_expr("noarr::into_blocks")};
	const fn_from<noarr_struct, ptr>::to<expr> make_bag {qname_expr("noarr::make_bag")};
} noarr;

}

#endif
