#include <iostream>
#include <cstdlib>
#include <cstring>
#include "memcopy_accel_tb_data.h"

void memcopy_accel(uint32_t* src, uint32_t* dst, uint32_t len);

int main() {
    const int SIZE = 64;
    uint32_t src[SIZE];
    uint32_t dst[SIZE];

    // Initialize source data
    for (int i = 0; i < SIZE; i++)
        src[i] = i + 1;

    // Run accelerator
    memcopy_accel(src, dst, SIZE * 4);

    // Verify
    int errors = 0;
    for (int i = 0; i < SIZE; i++) {
        if (src[i] != dst[i]) {
            std::cout << "Mismatch at " << i << ": " << src[i]
                      << " != " << dst[i] << std::endl;
            errors++;
        }
    }

    if (errors == 0)
        std::cout << "Test passed!" << std::endl;
    else
        std::cout << "Test failed with " << errors << " mismatches." << std::endl;

    return errors;
}
