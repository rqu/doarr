HFLAGS = -Wall -Wextra -pedantic -Wno-parentheses -Wmissing-declarations
FLAGS = $(HFLAGS) -O2 -DINTERNAL_VISIBILITY='__attribute__((visibility("internal")))'
CFLAGS = $(FLAGS) -Wmissing-prototypes -Wstrict-prototypes
CXXFLAGS = $(FLAGS) -std=c++20 -Wno-sequence-point -Iinclude

OBJS = build/call.o build/expr_all.o build/io.o

C_HEADERS = src/*.h
CXX_HEADERS = src/*.hpp include/doarr/*.hpp
ALL_HEADERS = $(C_HEADERS) $(CXX_HEADERS)
PUBLIC_HEADERS = include/doarr/*

all: build/libdoarr.a build/preproc

build/_: Makefile
	mkdir -p build
	touch $@



build/call.o: src/call.cpp $(ALL_HEADERS) build/_
	g++ -c $(CXXFLAGS) $< -o $@

build/expr_all.o: src/expr_all.cpp $(ALL_HEADERS) build/_
	g++ -c $(CXXFLAGS) -fno-rtti $< -o $@

build/io.o: src/io.c $(C_HEADERS) build/_
	gcc -c $(CFLAGS) $< -o $@



build/doarr.o: $(OBJS) build/_
	ld -r $(OBJS) -o $@
	objcopy --localize-hidden $@

build/libdoarr.a: build/doarr.o build/_
	ar rcs $@ $<



build/preproc: preproc.template $(ALL_HEADERS) build/_
	awk '{ if(/^#include o/) system("gcc -E -P -include src/guest_file.h -include include/doarr/guest_fn_wrapper_.hpp -x c /dev/null"); else print; }' $< > $@
	chmod +x $@



test: check_headers build/test
	build/test

TEST_CXXINPUT = test/host.cpp build/test_guest_noarrless.pch.cpp build/test_guest_mininoarr.pch.cpp
TEST_CXXFLAGS = -std=c++20 -Iinclude -Og -Wall -Wextra

build/test: $(TEST_CXXINPUT) $(PUBLIC_HEADERS) build/libdoarr.a build/_
	g++ $(TEST_CXXINPUT) build/libdoarr.a -o $@ $(TEST_CXXFLAGS)

build/test_guest_noarrless.pch.cpp: test/guest_noarrless.cpp build/preproc $(PUBLIC_HEADERS) build/_
	build/preproc $< -o $@ $(TEST_CXXFLAGS)

build/test_guest_mininoarr.pch.cpp: test/guest_mininoarr.cpp build/preproc $(PUBLIC_HEADERS) build/_
	build/preproc $< -o $@ $(TEST_CXXFLAGS)



check_headers: $(ALL_HEADERS)
	sh test/check_headers.sh $(HFLAGS)
