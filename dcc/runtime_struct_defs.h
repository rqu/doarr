#include "../runtime/guest_file.h"

struct guest_fn {
	void *const file; // points to struct guest_file
	const char *const name;
};
