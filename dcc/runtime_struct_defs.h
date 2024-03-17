#include "../runtime/guest_file.h"

struct guest_fn {
	struct guest_file *file;
	const char *name;
};
