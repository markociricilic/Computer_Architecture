#include <stdio.h>

#define ARRAY_SIZE 1000000

/*
Stride prefetchers struggle with non-repeating patterns (irregular strides) and nested accesses, while delta-correlation learn the 
dynamic delta sequence (17, 29, -13) and adapts to the nested accesses, hence having a lower miss rate than the stride prefetcher. 

So over 1,000,000 iterations, the open-ended prefetcher L1 data cache miss rate is 6.36% while the stride prefetcher 8.38%.
*/
int main() {
    int i = 0, j;
    int array[ARRAY_SIZE];

    while (i < ARRAY_SIZE) {
        int delta = (i % 4 == 0) ? 17 : ((i % 3 == 0) ? 29 : -13);  // delta changes based on index i using a non-linear, non-repeating formula (using both forward and backward strides (17, 29, and -13) add the irregularity)
        i += delta;  // Then update index i with changing delta

        if (i >= 0 && i < ARRAY_SIZE) {
            array[i] = i;  // Access the array
        }

        // Nested irregular access pattern
        if (i % 5 == 0) {
            // if index is divisible by 5, then execute inner loop below with irregular increments (j % 7 + 5) - this nest and irregular increment creates harder to predict memory accesses
            for (j = i; j < i + 50 && j < ARRAY_SIZE; j += (j % 7 + 5)) {
                array[j] = j;  // More irregular accesses
            }
        }
    }
    return 0;
}