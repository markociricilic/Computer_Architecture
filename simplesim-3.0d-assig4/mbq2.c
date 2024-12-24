#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 1000000
#define ITERATIONS 4

int array[ARRAY_SIZE];

int main() {
    int i, j, array[ARRAY_SIZE];

    // For one of the instructions, increment the index by 32 for each loop iteration, then access that element in the array. This is equivalent to a 2 block
    // interval (32 * 4 = 128B), which the next line prefetcher failed to prefetch correctly. Additionally, there is a second instruction (loop) which performs
    // the same loop logic but with an interval of 64, equivalent to 4 blocks. Ideally, if the stride prefetcher works, it will recognize that the first instruction
    // has a constant stride interval of 128B and the second has a constant stride interval of 256B, causing a cache miss rate of near 0%.
    for (j = 0; j < ITERATIONS; j++) {
        for (i = 0; i < ARRAY_SIZE; i += 32) {
            array[i] = 5;
        }
    }

    for (j = 0; j < ITERATIONS; j++) {
        for (i = 0; i < ARRAY_SIZE; i += 64) {
            array[i] = 5;
        }
    }

    // Increment the index by 32 or 64 for each loop iteration, then access that element in the array. The increment changes every other iteration, from 32 to 64
    // to 32 and so on. This ensures that for every iteration, the predicted stride for the next address in memory is NOT the same as the last actual stride,
    // causing the prefetcher to constantly be in a state of initial and transient for this instruction. This means that there should always be a cache miss for 
    // each iteration of the inner loop in the following code, assuming the prefetcher is working correctly, since the prefetcher will prefetch the address at the
    // wrong block interval into the cache. 
    for (j = 0; j < ITERATIONS; j++) {
        int n = 1;
        for (i = 0; i < ARRAY_SIZE; i += 64 / n) {
            array[i] = 5;
            if (n == 1) {
                n = 2;
            }
            else {
                n = 1;
            }
        }
    }

    return 0;
}