#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 1000000
#define ITERATIONS 4

int array[ARRAY_SIZE];

int main() {
    int i, j, array[ARRAY_SIZE];

    // Increment the index by 16 for each loop iteration, then access that element in the array. Since the block size is 64B, the address + block size can be
    // represented as address + (16 elements * 4 byte integers). Therefore, the address of element i + 16 should be what the next-line prefetcher fetches into
    // the cache when element i is accessed, assuming the prefetcher is working correctly. If it is working correctly, there should be 0 or close to 0 cache 
    // misses disregarding the first access to the array and other accesses not directly related to the 'array[i] = i' portion of the code.
    for (j = 0; j < ITERATIONS; j++) {
        for (i = 0; i < ARRAY_SIZE; i += 16) {
            array[i] = 5;
        }
    }

    // Increment the index by 32 for each loop iteration, then access that element in the array. This results in the program accessing the elements represented
    // by address + (32 elements * 4 byte integers). This is equivalent to accessing elements in 2 block intervals, which is out of the scope of the next line
    // prefetcher (1 block). This means that there should always be a cache miss for each iteration of the inner loop in the following code, assuming the
    // prefetcher is working correctly, since the prefetcher will prefetch the incorrect address into the cache. 
    for (j = 0; j < ITERATIONS; j++) {
        for (i = 0; i < ARRAY_SIZE; i += 32) {
            array[i] = 5;
        }
    }
    return 0;
}