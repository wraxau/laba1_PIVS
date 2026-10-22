#include "my_rand.h"

unsigned my_rand(unsigned *seed) {
    long long value = *seed;
    value = value*279470273%4294967291U;
    *seed = value;
    return *seed;
}

double my_drand(unsigned *seed) {
    return my_rand(seed)/4294967291.0;
}
