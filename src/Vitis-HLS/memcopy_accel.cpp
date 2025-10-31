#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

#define BURST_LEN 32  // number of 32-bit words per burst

void memcopy_accel(
    uint32_t* src,   // AXI4 Master port for source DDR
    uint32_t* dst,   // AXI4 Master port for destination DDR
    uint32_t len     // Number of bytes to copy
) {
#pragma HLS INTERFACE m_axi     port=src offset=slave bundle=AXI_SRC depth=1024
#pragma HLS INTERFACE m_axi     port=dst offset=slave bundle=AXI_DST depth=1024
#pragma HLS INTERFACE s_axilite port=src       bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite port=dst       bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite port=len       bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite port=return    bundle=CTRL_BUS

    uint32_t num_words = len >> 2;  // divide by 4
    uint32_t buffer[BURST_LEN];
#pragma HLS ARRAY_PARTITION variable=buffer complete dim=1

copy_loop:
    for (uint32_t i = 0; i < num_words; i += BURST_LEN) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=1024
#pragma HLS PIPELINE II=1

    	//uint32_t chunk = (i + BURST_LEN <= num_words) ? BURST_LEN : (num_words - i);
    	//remove the conditional (branch) replacing the line above
    	//***Branchless Equivalent (HLS-safe)
        uint32_t diff = num_words - i;
        uint32_t mask = (diff >= BURST_LEN); // 1 if full burst, 0 otherwise
        mask = -mask; // 0xFFFFFFFF if true, 0x0 if false
        uint32_t chunk = (mask & BURST_LEN) | (~mask & diff);
       //****Branchless Equivalent (HLS-safe) end


    read_loop:
        for (uint32_t j = 0; j < BURST_LEN; j++) {
#pragma HLS UNROLL
            bool valid = (j < chunk);
            uint32_t vmask = -((uint32_t)valid);
            buffer[j] = src[i + j] & vmask;  // only valid data copied
        }

    write_loop:
        for (uint32_t j = 0; j < BURST_LEN; j++) {
#pragma HLS UNROLL
            bool valid = (j < chunk);
            uint32_t vmask = -((uint32_t)valid);
            dst[i + j] = buffer[j] & vmask;  // only valid data written
        }
    }
}
