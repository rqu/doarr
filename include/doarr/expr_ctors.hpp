#ifndef DOARR_EXPR_CTORS_HPP_
#define DOARR_EXPR_CTORS_HPP_

/*
 * Functions that construct expr instances, implemented by expr_ctors.cpp.
 * See expr.hpp for user-friendly wrappers.
 */

#include "expr_base.hpp"

namespace doarr {

struct infix_op;
extern const infix_op infix_xor;

expr dyn_expr(std::size_t value);
expr dyn_expr(double value);
expr dyn_expr(void *value);

expr call_expr(const expr &fn, exprs &&args);
expr call_expr(expr &&fn, exprs &&args);
expr inst_expr(const expr &tmpl, exprs &&args);
expr inst_expr(expr &&tmpl, exprs &&args);

expr infix_expr(const infix_op &op, const expr &left, const expr &right);
expr infix_expr(const infix_op &op, const expr &left, expr &&right);
expr infix_expr(const infix_op &op, expr &&left, const expr &right);
expr infix_expr(const infix_op &op, expr &&left, expr &&right);

expr char_expr(char value);
expr int_expr(std::size_t value);
expr qname_expr(const char *qname);

}

#endif
