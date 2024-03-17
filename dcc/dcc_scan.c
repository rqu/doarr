#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dcc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef unsigned char uchar;

static bool scan_exactly(FILE *in, const uchar *expected, int num_expected) {
	for(int i = 0; i < num_expected; i++) {
		uchar c = fgetc(in);
		if(c != *expected++) {
			ungetc(c, in);
			return false;
		}
	}
	return true;
}

static void scan_raw_string(FILE *in) {
	uchar delim[17]; // including quote, not including paren
	int delim_len = 0;
	while(delim_len < 17) {
		uchar c = fgetc(in);
		if(c == '(') {
			delim[delim_len++] = '"';
			break;
		}
		delim[delim_len++] = c;
	}
	for(;;) {
		switch(fgetc(in)) {
			case EOF:
				return;
			case ')':
				if(scan_exactly(in, delim, delim_len))
					return;
		}
	}
}

static void scan_string(FILE *in) {
	for(;;) {
		switch(fgetc(in)) {
			case EOF:
			case '"':
				return;
			case '\\':
				fgetc(in);
		}
	}
}

static int scan_first_printable(FILE *in) {
	for(;;) {
		int c = fgetc(in);
		if(c == EOF || c > ' ')
			return c;
	}
}

static bool char_is_ident_start(uchar c) {
	return c == '$' || c >= 'A' && c <= 'Z' || c == '_' || c >= 'a' && c <= 'z' || c >= 0x80;
}

static bool char_is_ident(uchar c) {
	return char_is_ident_start(c) || c >= '0' && c <= '9';
}

static void report_eof(FILE *in) {
	dcc_err(ferror(in) ? "error reading preprocessed source" : "unexpected end of file");
}

static char *scan_ident(FILE *in) {
	int c = scan_first_printable(in);
	if(c == EOF) {
		report_eof(in);
		return NULL;
	}
	if(!char_is_ident_start(c)) {
		dcc_err("'doarr::exported' not immediately followed by the function name");
		return NULL;
	}

	int size = 0;
	int capacity = 32;
	char *ptr = malloc(capacity);
	if(!ptr) return dcc_oom(), NULL;

	ptr[size++] = c;
	for(;;) {
		c = fgetc(in);
		if(c == EOF) {
			report_eof(in);
			return NULL;
		}
		if(char_is_ident(c)) {
			ptr[size++] = c;
		} else {
			ptr[size++] = '\0';
			ungetc(c, in);
			return ptr;
		}
		if(size == capacity) {
			capacity = capacity / 2 * 3;
			ptr = realloc(ptr, capacity);
			if(!ptr) return dcc_oom(), NULL;
		}
	}
}

char *dcc_scan_up_to_next_export(FILE *in) {
	static const uchar doarr[] = "doarr";
	static const uchar exported[] = "exported";
	for(int curr, prev = ' ';;) {
		switch(curr = fgetc(in)) {
			case EOF:
				if(ferror(in)) {
					dcc_err("error reading preprocessed source");
					return NULL;
				} else {
					return "";
				}
			case '"':
				scan_string(in);
				prev = '"';
				continue;
			case 'R':
				if(fgetc(in) == '"') {
					scan_raw_string(in);
					prev = '"';
				} else {
					prev = 'R';
				}
				continue;
			case 'd':
				if(char_is_ident(prev)) continue;
				if(!scan_exactly(in, doarr + 1, sizeof doarr - 2)) continue;
				if(scan_first_printable(in) != ':') continue;
				if(scan_first_printable(in) != ':') continue;
				if(scan_first_printable(in) != *exported) continue;
				if(!scan_exactly(in, exported + 1, sizeof exported - 2)) continue;
				char *ident = scan_ident(in);
				if(!ident)
					return NULL;
				if(strstr(ident, "__")) {
					dcc_errf("invalid identifier '%s'", ident);
					free(ident);
					return NULL;
				}
				if(scan_first_printable(in) != '(') {
					dcc_errf("'doarr::exported' used for non-function '%s'", ident);
					free(ident);
					return NULL;
				}
				return ident;
			default:
				prev = curr;
				continue;
		}
	}
}
