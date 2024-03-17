HFLAGS = -Wall -Wextra -pedantic -Wno-parentheses -Wmissing-declarations
FLAGS = $(HFLAGS) -O2 -DINTERNAL_VISIBILITY='__attribute__((visibility("internal")))'
CFLAGS = $(FLAGS) -Wmissing-prototypes -Wstrict-prototypes
CXXFLAGS = $(FLAGS) -std=c++20 -Wno-sequence-point -Iinclude

OBJS = build/call.o build/expr_all.o build/io.o

C_HEADERS = runtime/*.h
CXX_HEADERS = runtime/*.hpp include/doarr/*.hpp
ALL_HEADERS = $(C_HEADERS) $(CXX_HEADERS)
PUBLIC_HEADERS = include/doarr/*

DCC = build/dcc

all: build/libdoarr.a $(DCC)

build/_: Makefile
	mkdir -p build
	touch $@



build/call.o: runtime/call.cpp $(ALL_HEADERS) build/_
	g++ -c $(CXXFLAGS) $< -o $@

build/expr_all.o: runtime/expr_all.cpp $(ALL_HEADERS) build/_
	g++ -c $(CXXFLAGS) -fno-rtti $< -o $@

build/io.o: runtime/io.c $(C_HEADERS) build/_
	gcc -c $(CFLAGS) $< -o $@



build/doarr.o: $(OBJS) build/_
	ld -r $(OBJS) -o $@
	objcopy --localize-hidden $@

build/libdoarr.a: build/doarr.o build/_
	ar rcs $@ $<



$(DCC): dcc/* build/runtime_struct_defs.str.h build/_
	$(CC) $(CFLAGS) dcc/*.c -o $@

build/runtime_struct_defs.str.h: dcc/runtime_struct_defs.h $(C_HEADERS) build/_
	$(CC) -E -P $< | sed 's/.*/"\0\\n"/' > $@



test: check_headers build/test
	build/test

TEST_CXXINPUT = test/host.cpp build/test_guest_noarrless.o build/test_guest_mininoarr.o
TEST_CXXFLAGS = -std=c++20 -Iinclude -Og -Wall -Wextra

build/test: $(TEST_CXXINPUT) $(PUBLIC_HEADERS) build/libdoarr.a build/_
	g++ $(TEST_CXXINPUT) build/libdoarr.a -o $@ $(TEST_CXXFLAGS)

build/test_guest_noarrless.o: test/guest_noarrless.cpp $(DCC) $(PUBLIC_HEADERS) build/_
	$(DCC) -c $< -o $@ $(TEST_CXXFLAGS)

build/test_guest_mininoarr.o: test/guest_mininoarr.cpp $(DCC) $(PUBLIC_HEADERS) build/_
	$(DCC) -c $< -o $@ $(TEST_CXXFLAGS)



check_headers: $(ALL_HEADERS)
	sh test/check_headers.sh $(HFLAGS)
