#include <doarr/expr_ctors.hpp>
#include <doarr/any_.hpp>
#include "expr_util.hpp"

#include <algorithm>
#include <cstdlib>

#define FORWARD(E) decltype(E)(E)

using doarr::ExprNoexcept;
using doarr::expr;
using doarr::exprs;
using doarr::internal::any;
using doarr::internal::expr_impl;
using doarr::internal::expr_impl_base;
using namespace doarr::runtime;



///////////////////////
//                   //
//   expr_base.hpp   //
//                   //
///////////////////////



// fully define expr_impl_base

constexpr expr_impl_base::expr_impl_base(std::size_t hash) noexcept : refcount(1), hash(hash) {}



// fully define expr_impl

namespace {
	struct expr_impl_class {
		bool (*const are_equal)(const expr_impl &a, const expr_impl &b);
	};

	template<typename T, typename EQ>
	constexpr expr_impl_class make_expr_impl_class = {
		.are_equal = [](const expr_impl &a, const expr_impl &b) { return EQ()((const T &)a, (const T &)b); },
	};
}

struct doarr::internal::expr_impl : expr_impl_base {
	const expr_impl_class *const cls;
	const std::size_t num_params;

	explicit expr_impl() = delete;
	constexpr explicit expr_impl(const expr_impl_class *cls, std::size_t hash, std::size_t num_params) noexcept : expr_impl_base(hash), cls(cls), num_params(num_params) {}
	explicit expr_impl(const expr_impl &) = delete;
	explicit expr_impl(expr_impl &&) = delete;
	void operator =(const expr_impl &) = delete;
	void operator =(expr_impl &&) = delete;

	virtual any *extract_params(any *) = 0;
	virtual std::size_t write_to(std::FILE *, std::size_t) const = 0;
	virtual ~expr_impl() = default;

	constexpr std::size_t get_hash() noexcept { return hash; }
};



// fully define expr

constexpr expr::expr(expr_impl_base *impl) noexcept : impl(impl) {}

[[gnu::nonnull]]
static constexpr expr_impl *as_expr_impl(expr_impl_base *ptr) noexcept {
	return static_cast<expr_impl *>(ptr);
}

constexpr expr_impl *expr::operator ->() const noexcept {
	return as_expr_impl(impl);
}

void expr::del(expr_impl_base *impl) noexcept {
	delete as_expr_impl(impl);
}



// fully define expr(s) non-members

namespace doarr {

bool operator ==(const expr &a, const expr &b) noexcept {
	auto *ap = a.operator->(), *bp = b.operator->();
	if(ap == bp)
		return true;
	if(ap->get_hash() != bp->get_hash())
		return false;
	auto cls = ap->cls;
	if(cls != bp->cls)
		return false;
	return cls->are_equal(*ap, *bp);
}

bool operator ==(const exprs &a, const exprs &b) noexcept {
	return std::ranges::equal(a, b);
}

std::size_t hash(const exprs &es) noexcept {
	std::size_t r = 1;
	for(const expr &e : es)
		r *= 31, r += e->get_hash();
	return r;
}

}



///////////////////////
//                   //
//   expr_util.hpp   //
//                   //
///////////////////////



std::size_t exs::num_params(const exprs &es) noexcept {
	std::size_t sum = 0;
	for(const expr &e : es)
		sum += e->num_params;
	return sum;
}

doarr::internal::any *exs::extract_params(const exprs &es, any *out) noexcept {
	for(const expr &e : es)
		out = e->extract_params(out);
	return out;
}

std::size_t exs::write_to(const exprs &es, std::FILE *out, std::size_t param_idx) noexcept {
	bool sep = false;
	for(const expr &e : es) {
		if(sep)
			std::fputs(", ", out);
		param_idx = e->write_to(out, param_idx);
		sep = true;
	}
	return param_idx;
}



////////////////////////
//                    //
//   expr_ctors.hpp   //
//                    //
////////////////////////



struct doarr::infix_op {
	const char *str;
};
const doarr::infix_op doarr::infix_xor = {"^"};



namespace {

static constexpr std::size_t hash_str(const char *str, std::size_t len) noexcept {
	std::size_t r = 1;
	for(std::size_t i = 0; i < len; i++)
		r *= 31, r += (unsigned char) str[i];
	return r;
}

struct dyn_expr_impl final : expr_impl {
	const char tag;
	const any value;

	static constexpr expr_impl_class cls = make_expr_impl_class<dyn_expr_impl, decltype([](const dyn_expr_impl &a, const dyn_expr_impl &b) {
		return a.tag == b.tag;
	})>;

	explicit dyn_expr_impl(char tag, any value) :
		expr_impl(&cls, hash_all((std::size_t) &cls, tag), 1),
		tag(tag),
		value(value) {}

	any *extract_params(any *out) override {
		*out++ = value;
		return out;
	}

	std::size_t write_to(std::FILE *out, std::size_t param_idx) const override {
		std::fprintf(out, "DOARR_EXPORT[%zu].%c", param_idx++, tag);
		return param_idx;
	}
};

struct call_expr_impl final : expr_impl {
	const expr fn;
	const exprs args;
	const char lbr, rbr;

	static constexpr expr_impl_class cls = make_expr_impl_class<call_expr_impl, decltype([](const call_expr_impl &a, const call_expr_impl &b) {
		return a.fn == b.fn && a.args == b.args && a.lbr == b.lbr && a.rbr == b.rbr;
	})>;


	explicit call_expr_impl(ExprNoexcept auto &&fn, exprs &&args, char lbr, char rbr) :
		expr_impl(&cls, hash_all((std::size_t) &cls, fn, args, lbr), fn->num_params + exs::num_params(args)),
		fn(FORWARD(fn)),
		args(FORWARD(args)),
		lbr(lbr),
		rbr(rbr) {}

	any *extract_params(any *out) override {
		out = fn->extract_params(out);
		out = exs::extract_params(args, out);
		return out;
	}

	std::size_t write_to(std::FILE *out, std::size_t param_idx) const override {
		param_idx = fn->write_to(out, param_idx);
		std::fputc(lbr, out);
		param_idx = exs::write_to(args, out, param_idx);
		std::fputc(rbr, out);
		return param_idx;
	}
};

struct infix_expr_impl final : expr_impl {
	const doarr::infix_op op;
	const expr left, right;

	static constexpr expr_impl_class cls = make_expr_impl_class<infix_expr_impl, decltype([](const infix_expr_impl &a, const infix_expr_impl &b) {
		return a.op.str == b.op.str && a.left == b.left && a.right == b.right;
	})>;

	explicit infix_expr_impl(doarr::infix_op op, ExprNoexcept auto &&left, ExprNoexcept auto &&right) :
		expr_impl(&cls, hash_all((std::size_t) &cls, (std::size_t) op.str, left, right), left->num_params + right->num_params),
		op(op),
		left(FORWARD(left)),
		right(FORWARD(right)) {}

	any *extract_params(any *out) override {
		out = left->extract_params(out);
		out = right->extract_params(out);
		return out;
	}

	std::size_t write_to(std::FILE *out, std::size_t param_idx) const override {
		std::fputc('(', out);
		param_idx = left->write_to(out, param_idx);
		std::fputs(op.str, out);
		param_idx = right->write_to(out, param_idx);
		std::fputc(')', out);
		return param_idx;
	}
};

struct raw_expr_impl final : expr_impl {
	const std::unique_ptr<char[]> code;

	static constexpr expr_impl_class cls = make_expr_impl_class<raw_expr_impl, decltype([](const raw_expr_impl &a, const raw_expr_impl &b) {
		const char *ap = a.code.get();
		const char *bp = b.code.get();
		for(;;) {
			if(*ap != *bp)
				return false;
			if(!*ap)
				return true;
			ap++, bp++;
		}
	})>;

	explicit raw_expr_impl(const char *code, std::size_t len) : expr_impl(&cls, hash_all((std::size_t) &cls, hash_str(code, len)), 0), code(new char[len]) {
		for(std::size_t i = 0; i < len; i++)
			this->code[i] = code[i];
	}

	any *extract_params(any *out) override {
		return out;
	}

	std::size_t write_to(std::FILE *out, std::size_t param_idx) const override {
		std::fputs(code.get(), out);
		return param_idx;
	}
};

}



expr doarr::dyn_expr(std::size_t value) {
	return expr{new dyn_expr_impl('i', any{.i = value})};
}

expr doarr::dyn_expr(double value) {
	return expr{new dyn_expr_impl('f', any{.f = value})};
}

expr doarr::dyn_expr(void *value) {
	return expr{new dyn_expr_impl('p', any{.p = value})};
}



expr doarr::call_expr(const expr &fn, exprs &&args) {
	return expr{new call_expr_impl(FORWARD(fn), FORWARD(args), '(', ')')};
}

expr doarr::call_expr(expr &&fn, exprs &&args) {
	return expr{new call_expr_impl(FORWARD(fn), FORWARD(args), '(', ')')};
}

expr doarr::inst_expr(const expr &tmpl, exprs &&args) {
	return expr{new call_expr_impl(FORWARD(tmpl), FORWARD(args), '<', '>')};
}

expr doarr::inst_expr(expr &&tmpl, exprs &&args) {
	return expr{new call_expr_impl(FORWARD(tmpl), FORWARD(args), '<', '>')};
}



expr doarr::infix_expr(const infix_op &op, const expr &left, const expr &right) {
	return expr{new infix_expr_impl(op, FORWARD(left), FORWARD(right))};
}

expr doarr::infix_expr(const infix_op &op, const expr &left, expr &&right) {
	return expr{new infix_expr_impl(op, FORWARD(left), FORWARD(right))};
}

expr doarr::infix_expr(const infix_op &op, expr &&left, const expr &right) {
	return expr{new infix_expr_impl(op, FORWARD(left), FORWARD(right))};
}

expr doarr::infix_expr(const infix_op &op, expr &&left, expr &&right) {
	return expr{new infix_expr_impl(op, FORWARD(left), FORWARD(right))};
}



expr doarr::char_expr(char value) {
	// easy to be represented as char literal?
	if(value >= 32 && value < 127 && value != '\'' && value != '\\') {
		char lit[] = "'_'";
		lit[1] = value;
		return expr{new raw_expr_impl(lit, sizeof lit)};
	} else {
		return int_expr(value);
	}
}

expr doarr::int_expr(std::size_t value) {
	char str[3 * sizeof value + 1];
	char *p = str + sizeof str;
	*--p = '\0';
	do {
		*--p = '0' + value % 10;
		value /= 10;
	} while(value);
	return expr{new raw_expr_impl(p, str + sizeof str - p)};
}

namespace {

bool valid_ident_start(char c) {
	return c >= 'A' && c <= 'Z' || c == '_' || c >= 'a' && c <= 'z';
}

bool valid_ident_cont(char c) {
	return valid_ident_start(c) || c >= '0' && c <= '9';
}

const char *valid_qname_end(const char *name) {
	const char *p = name;
	// skip over leading ::
	if(p[0] == ':') {
		if(p[1] == ':')
			p += 2;
		else
			return nullptr;
	}
	for(;;) {
		// skip over identifier
		if(!valid_ident_start(*p++))
			return nullptr;
		while(valid_ident_cont(*p))
			p++;
		// either end, or skip :: and continue looping
		switch(*p++) {
			case '\0':
				return p;
			case ':':
				if(*p++ != ':')
					return nullptr;
				continue;
			default:
				return nullptr;
		}
	}
}

}

expr doarr::qname_expr(const char *name) {
	const char *end = valid_qname_end(name);
	if(!end) {
		std::fprintf(stderr, "Invalid qualified name: %s\n", name);
		std::abort();
	}
	return expr{new raw_expr_impl(name, end - name)};
}
