#include <doarr/call_.hpp>
#include "expr_util.hpp"
extern "C" {
#include "io.h"
}

#include <stdexcept>
#include <unordered_map>

using doarr::exprs;
using doarr::internal::guest_fn;
using namespace doarr::runtime;

namespace {

struct cache_key {
	std::size_t hash;
	const guest_fn *fn;
	bool have_tmpl_args;
	exprs tmpl_args;
	exprs call_args;

	explicit cache_key(const guest_fn *fn, bool have_tmpl_args, exprs &&tmpl_args, exprs &&call_args) :
		hash(hash_all((std::size_t) fn, have_tmpl_args, tmpl_args, call_args)),
		fn(fn),
		have_tmpl_args(have_tmpl_args),
		tmpl_args(std::move(tmpl_args)),
		call_args(std::move(call_args)) {}

	friend bool operator ==(const cache_key &a, const cache_key &b) {
		return a.hash == b.hash
			&& a.fn == b.fn
			&& a.have_tmpl_args == b.have_tmpl_args
			&& a.tmpl_args == b.tmpl_args
			&& a.call_args == b.call_args
			;
	}
};

struct cache_value {
	void *handle;
	void *fn;
};

using cache_key_hash = decltype([](const cache_key &k) constexpr noexcept { return k.hash; });

std::unordered_map<cache_key, cache_value, cache_key_hash> GLOBAL_cache;

struct doarr_io_ctx *GLOBAL_io_ctx() {
	static struct lazy_init : doarr_io_ctx {
		lazy_init() {
			if(doarr_io_init(this))
				throw std::runtime_error("Initialization failed");
		}
	} instance;
	return &instance;
}

template<int N>
void w1(std::FILE *out, const char (&s)[N]) {
	std::fwrite(s, 1, N-1, out);
}

void w1(std::FILE *out, const char *s) {
	std::fputs(s, out);
}

void w1(std::FILE *out, const exprs &es) {
	exs::write_to(es, out, 0);
}

void w(std::FILE *out, const auto &... as) {
	(..., w1(out, as));
}

struct guest_file *fn_file(const guest_fn *fn) {
	return (struct guest_file *) fn->file;
}

void compile(const guest_fn *fn, bool have_tmpl_args, const cache_key &k, cache_value &out_v) {
	struct doarr_io_ctx *ctx = GLOBAL_io_ctx();

	const char *hdr = doarr_extract_precompiled_or_null(ctx, fn_file(fn));
	if(!hdr)
		throw std::runtime_error("Could not extract precompiled header");

	struct tmp_full_path cxx_file_name;
	doarr_tmp_path(ctx, ".cxx", &cxx_file_name);

	std::FILE *out = std::fopen(cxx_file_name.chars, "w");
	w(out, "#include \"", hdr, "\"\n");
	w(out, "#undef DOARR_EXPORT\n");
	w(out, "extern \"C\" __attribute__ ((visibility (\"default\"))) void DOARR_EXPORT(const doarr::internal::any *DOARR_EXPORT) {\n");
	w(out, "(void)DOARR_EXPORT;\n");
	w(out, fn->name);
	if(have_tmpl_args)
		w(out, "<", k.tmpl_args, ">");
	w(out, "(", k.call_args, ");\n");
	w(out, "}\n");
	std::fclose(out);

	switch(doarr_compile_and_load(ctx, &cxx_file_name, fn_file(fn), &out_v.handle, &out_v.fn)) {
		case 0:
			break; // OK
		case 1:
			throw doarr::compilation_error();
		case 2:
			throw std::runtime_error("Could not load the compiled code");
		default:
			abort(); // should not happen
	}
}

}

void doarr::internal::call(const guest_fn *fn, bool have_tmpl_args, exprs &&tmpl_args, exprs &&call_args) {
	if(exs::num_params(tmpl_args))
		throw std::logic_error("Template argument depends on a dynamic value");
	auto params_uniq = std::make_unique_for_overwrite<any[]>(exs::num_params(call_args));
	any *params = params_uniq.get();
	exs::extract_params(call_args, params);

	auto [iter, miss] = GLOBAL_cache.try_emplace(cache_key(fn, have_tmpl_args, std::move(tmpl_args), std::move(call_args)));
	auto &[k, v] = *iter;
	if(miss) {
		try {
			compile(fn, have_tmpl_args, k, v);
		} catch(...) {
			GLOBAL_cache.erase(iter);
			throw;
		}
	}
	((void(*)(const any *)) v.fn)(params);
}
