#ifndef DOARR_EXPR_BASE_HPP_
#define DOARR_EXPR_BASE_HPP_

/*
 * Public interface of bare expr. See expr.hpp for user-friendly wrappers.
 */

#include <memory>

namespace doarr {

class expr;

namespace internal {
	class expr_impl;
	class expr_impl_base {
		std::size_t refcount;
		std::size_t hash;

		constexpr explicit expr_impl_base(std::size_t) noexcept;
		explicit expr_impl_base(const expr_impl_base &) = delete;
		explicit expr_impl_base(expr_impl_base &&) = delete;
		void operator =(const expr_impl_base &) = delete;
		void operator =(expr_impl_base &&) = delete;

		friend expr;
		friend expr_impl;
	};
}

class expr {
	internal::expr_impl_base *impl;

	static void del(internal::expr_impl_base *impl) noexcept;

	constexpr void incref() const noexcept {
		if(impl)
			impl->refcount++;
	}

	constexpr void decref() const noexcept {
		if(impl) {
			if(impl->refcount == 1)
				del(impl);
			else
				impl->refcount--;
		}
	}

public:
	constexpr explicit expr(internal::expr_impl_base *impl) noexcept;
	constexpr explicit expr(std::nullptr_t) noexcept : impl(nullptr) {}

	constexpr expr(const expr &src) noexcept : impl(src.impl) {
		incref();
	}
	constexpr expr(expr &&src) noexcept : impl(src.impl) {
		src.impl = nullptr;
	}
	constexpr expr &operator =(const expr &src) noexcept {
		src.incref();
		decref();
		impl = src.impl;
		return *this;
	}
	constexpr expr &operator =(expr &&src) noexcept {
		decref();
		impl = src.impl;
		src.impl = nullptr;
		return *this;
	}

	constexpr bool is_null() const noexcept {
		return !impl;
	}
	constexpr const expr &to_expr() const & noexcept {
		return *this;
	}
	constexpr expr &&to_expr() && noexcept {
		return std::move(*this);
	}
	constexpr internal::expr_impl *operator ->() const noexcept;

	constexpr ~expr() noexcept {
		decref();
	}

	friend constexpr void swap(expr &a, expr &b) noexcept {
		std::swap(a.impl, b.impl);
	}
	friend bool operator ==(const expr &, const expr &) noexcept;
	friend constexpr std::size_t hash(const expr &e) noexcept {
		return e.impl->hash;
	}
};

template<typename T, typename Expected = expr>
concept ExprNoexcept = requires(T t, void f(Expected) noexcept) {
	{ f((T &&) t) } noexcept;
};

class exprs {
	const std::size_t num_items;
	std::unique_ptr<expr[]> items;

public:
	constexpr explicit exprs() : num_items(0) {}
	explicit exprs(ExprNoexcept auto&&... items) : num_items(sizeof...(items)), items(new (expr[sizeof...(items)]){decltype(items)(items)...}) {}
	explicit exprs(const exprs &) = delete;
	explicit exprs(exprs &&) noexcept = default;
	void operator =(const exprs &) = delete;
	void operator =(exprs &&) = delete;

	constexpr std::size_t size() const noexcept {
		return num_items;
	}
	const expr *begin() const noexcept {
		return items.get();
	}
	const expr *end() const noexcept {
		return items.get() + num_items;
	}

	friend bool operator ==(const exprs &, const exprs &) noexcept;
	friend std::size_t hash(const exprs &) noexcept;
};

template<typename K>
struct hashobj {
	constexpr std::size_t operator()(const K &key) noexcept {
		return hash(key);
	}
};

}

template<>
struct std::hash<doarr::expr> : doarr::hashobj<doarr::expr> {};
template<>
struct std::hash<doarr::exprs> : doarr::hashobj<doarr::exprs> {};

#endif
