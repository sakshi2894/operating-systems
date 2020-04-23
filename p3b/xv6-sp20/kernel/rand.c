#include "rand.h"

struct xorshift32_state {
  unsigned int a;
};

struct xorshift32_state state = {1};

/* The state word must be initialized to non-zero */
unsigned int xorshift32()
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	unsigned x = state.a;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return state.a = x;
}

int xv6_rand (void) {
	unsigned int x = xorshift32() % XV6_RAND_MAX;
	return (int) x;
}

void xv6_srand (unsigned int seed) {
	state.a = seed;
}
