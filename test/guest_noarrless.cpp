#include <doarr/export.hpp>

void DOARR_EXPORT empty() {
}

void DOARR_EXPORT add(int a, int b, void *c) {
	*(int *) c = a + b;
}

template<int A>
void DOARR_EXPORT addt(int b, void *c) {
	*(int *) c = A + b;
}
