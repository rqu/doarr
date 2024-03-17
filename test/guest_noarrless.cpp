#include <doarr/export.hpp>

doarr::exported empty() {
}

doarr::exported add(int a, int b, void *c) {
	*(int *) c = a + b;
}

template<int A>
doarr::exported addt(int b, void *c) {
	*(int *) c = A + b;
}
