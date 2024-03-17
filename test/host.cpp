#include <doarr/import.hpp>
#include <doarr/expr.hpp>

#include <exception>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

////////////////////////////////////////////////////////////////

static bool GLOBAL_failed = false;

struct test_failed {};

void run_test_impl(auto test, const char *name) {
	std::printf("%40s | ", name);
	std::fflush(stdout);
	auto begin = std::chrono::steady_clock::now();
	try {
		test();
	} catch(test_failed) {
		GLOBAL_failed = true;
		return;
	} catch(std::exception &e) {
		std::printf("Thrown %s: %s\n", typeid(e).name(), e.what());
		GLOBAL_failed = true;
		return;
	}
	auto end = std::chrono::steady_clock::now();
	long long unsigned nanos = std::chrono::nanoseconds(end - begin).count();
	if(nanos >= 1000000ull)
		std::printf("%3llu %03llu %03llu ns\n", nanos/1000000ull%1000, nanos/1000ull%1000, nanos%1000);
	else if(nanos >= 1000ull)
		std::printf("%3s %3llu %03llu ns\n", "",                       nanos/1000ull%1000, nanos%1000);
	else
		std::printf("%3s %3s %3llu ns\n", "", "",                                          nanos%1000);
}

void assert_impl(bool cond, const char *msg) {
	if(!cond) {
		std::puts(msg);
		throw test_failed{};
	}
}

void assert_eq_impl(auto a, auto b, const char *a_name, const char *b_name) {
	if(a != b) {
		std::string a_str = std::to_string(a);
		std::string b_str = std::to_string(b);
		std::printf("***\n\nAssertion '%s == %s' failed:\n- %s = %s\n- %s = %s\n",
			    a_name, b_name, a_name, a_str.c_str(), b_name, b_str.c_str());
		throw test_failed{};
	}
}

#define ASSERT(COND) assert_impl(COND, "Assertion '" #COND "' failed")
#define ASSERT_EQ(A, B) assert_eq_impl(A, B, #A, #B)
#define RUN_TEST(TEST) run_test_impl([]{TEST;}, #TEST)
#define TEST_SEP() std::puts(std::string(57, '-').c_str())

////////////////////////////////////////////////////////////////

extern "C" doarr::imported empty;

void test_empty() {
	empty();
}


extern "C" doarr::imported add;

void test_add_dynamic(int a, int b) {
	int c = 999999999;
	add(doarr::dyn(a), doarr::dyn(b), doarr::ptr(&c));
	ASSERT_EQ(c, a + b);
}

void test_add_static(int a, int b) {
	int c = 999999999;
	add(doarr::num(a), doarr::num(b), doarr::ptr(&c));
	ASSERT_EQ(c, a + b);
}

void test_add_sd(int a, int b) {
	int c = 999999999;
	add(doarr::num(a), doarr::dyn(b), doarr::ptr(&c));
	ASSERT_EQ(c, a + b);
}

void test_add_ds(int a, int b) {
	int c = 999999999;
	add(doarr::dyn(a), doarr::num(b), doarr::ptr(&c));
	ASSERT_EQ(c, a + b);
}


extern "C" doarr::imported addt;

void test_add_tmpl(int a, int b) {
	int c = 999999999;
	addt[doarr::num(a)](doarr::num(b), doarr::ptr(&c));
	ASSERT_EQ(c, a + b);
}


using doarr::noarr;


extern "C" doarr::imported nempty;

void test_noarr_scalar() {
	nempty(noarr.scalar["float"]());
}

void test_noarr_vector() {
	nempty(noarr.scalar["float"]() ^ noarr.vector['x']());
}

void test_noarr_matrix() {
	nempty(noarr.scalar["float"]() ^ noarr.vector['x']() ^ noarr.vector['y']());
}

void test_noarr_rmatrix() {
	nempty(noarr.scalar["float"]() ^ (noarr.vector['x']() ^ noarr.vector['y']()));
}

void test_noarr_szvector() {
	nempty(noarr.scalar["float"]() ^ noarr.sized_vector['x'](42));
}

////////////////////////////////////////////////////////////////

} // unnamed ns

int main() {
	std::puts("");
	RUN_TEST(test_empty());
	RUN_TEST(test_empty());
	std::puts("");
	RUN_TEST(test_add_dynamic(123, 456));
	RUN_TEST(test_add_dynamic(222, 444));
	RUN_TEST(test_add_static(123, 456));
	RUN_TEST(test_add_static(222, 444));
	RUN_TEST(test_add_static(222, 444));
	std::puts("");
	RUN_TEST(test_add_sd(100, 200));
	RUN_TEST(test_add_ds(300, 400));
	std::puts("");
	RUN_TEST(test_add_tmpl(1111, 2222));
	std::puts("");
	RUN_TEST(test_noarr_scalar());
	RUN_TEST(test_noarr_vector());
	RUN_TEST(test_noarr_matrix());
	RUN_TEST(test_noarr_rmatrix());
	std::puts("");
	RUN_TEST(test_noarr_szvector());
	std::puts("");
	return GLOBAL_failed;
}
