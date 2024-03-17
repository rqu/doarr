CC = cc
CXX = c++
LD = ld
AR = ar
OBJCOPY = objcopy

# configurable flags
FLAGS = -Wall -Wextra -pedantic -Wno-parentheses -Wmissing-declarations -O2
CFLAGS = $(FLAGS) -Wmissing-prototypes -Wstrict-prototypes
CXXFLAGS = $(FLAGS)

# necessary flags - do not override these
RT_FLAGS = -DINTERNAL_VISIBILITY='__attribute__((visibility("internal")))'
RT_CFLAGS = $(RT_FLAGS) $(CFLAGS)
RT_CXXFLAGS = -std=c++20 -Wno-sequence-point -Iinclude $(RT_FLAGS) $(CXXFLAGS)
DCC_CFLAGS = $(CFLAGS)

RT_C_HEADERS = runtime/*.h
RT_CXX_HEADERS = runtime/*.hpp include/doarr/*.hpp
RT_HEADERS = $(RT_C_HEADERS) $(RT_CXX_HEADERS)
PUBLIC_HEADERS = include/doarr/*

DCC = build/dcc



all: build/libdoarr.a $(DCC)

build/_: Makefile
	mkdir -p build
	touch $@



build/call.o: runtime/call.cpp $(RT_HEADERS) build/_
	$(CXX) -c $(RT_CXXFLAGS) $< -o $@

build/expr_all.o: runtime/expr_all.cpp $(RT_HEADERS) build/_
	$(CXX) -c $(RT_CXXFLAGS) -fno-rtti $< -o $@

build/io.o: runtime/io.c $(RT_C_HEADERS) build/_
	$(CC) -c $(RT_CFLAGS) $< -o $@



RT_OBJS = build/call.o build/expr_all.o build/io.o

build/doarr.o: $(RT_OBJS) build/_
	$(LD) -r $(RT_OBJS) -o $@
	$(OBJCOPY) --localize-hidden $@

build/libdoarr.a: build/doarr.o build/_
	$(AR) rcs $@ $<



$(DCC): dcc/* build/runtime_struct_defs.str.h build/_
	$(CC) $(DCC_CFLAGS) dcc/*.c -o $@

build/runtime_struct_defs.str.h: dcc/runtime_struct_defs.h $(RT_C_HEADERS) build/_
	$(CC) -E -P $< | sed 's/.*/"\0\\n"/' > $@



test: check_headers build/test
	build/test

TEST_CXXINPUT = test/host.cpp build/test_guest_noarrless.o build/test_guest_mininoarr.o
TEST_CXXFLAGS = -std=c++20 -Iinclude -Og -Wall -Wextra -pedantic

build/test: $(TEST_CXXINPUT) $(PUBLIC_HEADERS) build/libdoarr.a build/_
	$(CXX) $(TEST_CXXINPUT) build/libdoarr.a -o $@ $(TEST_CXXFLAGS)

build/test_guest_noarrless.o: test/guest_noarrless.cpp $(DCC) $(PUBLIC_HEADERS) build/_
	$(DCC) -c $< -o $@ $(TEST_CXXFLAGS)

build/test_guest_mininoarr.o: test/guest_mininoarr.cpp $(DCC) $(PUBLIC_HEADERS) build/_
	$(DCC) -c $< -o $@ $(TEST_CXXFLAGS)



check_headers: $(RT_HEADERS) dcc/*.h test/compatibility.cpp
	sh test/check_headers.sh "`echo $(CC) $(CFLAGS)`" "`echo $(CXX) $(CXXFLAGS)`" "`echo $(RT_FLAGS)`"

clean:
	rm -rvf build
