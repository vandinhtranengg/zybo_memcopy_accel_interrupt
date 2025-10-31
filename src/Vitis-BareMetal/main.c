#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xparameters.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "memcopy_accel.h"


/* Example base: prefer using XPAR_<IP>_BASEADDR if your BSP/Xparameters defines it.
   Otherwise the driver default MEMCOPY_ACCEL_BASEADDR will be used. */
#ifndef XPAR_MEMCOPY_ACCEL_0_BASEADDR
#define MEMCOPY_BASE MEMCOPY_ACCEL_BASEADDR
#else
#define MEMCOPY_BASE XPAR_MEMCOPY_ACCEL_0_BASEADDR
#endif

#define INTC_DEVICE_ID         XPAR_SCUGIC_SINGLE_DEVICE_ID
#define MEMCOPY_ACCEL_INTR_ID  XPAR_FABRIC_MEMCOPY_ACCEL_0_INTERRUPT_INTR

// CPU memcpy
void cpu_memcopy(uint32_t* src, uint32_t* dst, uint32_t len_bytes) {
    uint32_t num_words = len_bytes / 4;
    for (uint32_t i = 0; i < num_words; i++) {
        dst[i] = src[i];
    }
}

/* Global variables */
static XScuGic Intc;
static volatile int memcopy_done = 0;

/* ----------------------------------------------------
 * Interrupt service routine for memcopy_accel
 * ---------------------------------------------------- */

static inline u32 reg(u32 off){ return Xil_In32(MEMCOPY_BASE + off); }
static inline void wr(u32 off, u32 v){ Xil_Out32(MEMCOPY_BASE + off, v); }

static void dump_regs(const char* tag){
    xil_printf("[REGS %s] CTRL=%08x GIE=%08x IER=%08x ISR=%08x \r\n",
        tag,
        reg(MEMCOPY_ACCEL_CTRL_OFFSET),
        reg(MEMCOPY_ACCEL_GIE_OFFSET),
        reg(MEMCOPY_ACCEL_IER_OFFSET),
        reg(MEMCOPY_ACCEL_ISR_OFFSET));
}

void memcopy_isr(void *CallbackRef)
{
	//dump_regs("ISR entry");
    memcopy_accel_interrupt_clear();
    //dump_regs("ISR clear");

    // Read-back to flush AXI write and avoid IRQ retrigger (required on Zynq)
    (void)Xil_In32(MEMCOPY_BASE + MEMCOPY_ACCEL_ISR_OFFSET);  // read-back barrier - Barrier read to ensure ISR write takes effect before returning

    memcopy_done = 1;


    /*explain more why need add this a dummy read "(void)Xil_In32(MEMCOPY_BASE + MEMCOPY_ACCEL_ISR_OFFSET);"
     * Why This Fix Works
	AXI-Lite writes are posted transactions — they can take several cycles to propagate from the ARM core (PS) to the PL IP.
	When the ISR returned too quickly, the interrupt signal (ap_done) was still asserted for a few cycles, so the GIC saw the line still high and re-entered the ISR instantly (interrupt storm).
	By performing a dummy read right after writing 1 to clear the ISR, you flush the AXI write buffer, forcing the hardware to complete the clear before leaving the ISR.
	This ensures that the interrupt line is low when the ISR exits, stopping re-triggers.
	So that read acts as a memory barrier or handshake acknowledgment to the hardware.
     */

}



/* ================== Interrupt Setup ================== */
int setup_interrupt_system(void)
{
    XScuGic_Config *IntcConfig;
    int Status;

    IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
    if (IntcConfig == NULL)
        return XST_FAILURE;

    Status = XScuGic_CfgInitialize(&Intc, IntcConfig, IntcConfig->CpuBaseAddress);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
                                 &Intc);

    Status = XScuGic_Connect(&Intc,
                             MEMCOPY_ACCEL_INTR_ID,
                             (Xil_ExceptionHandler)memcopy_isr,
                             NULL);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    XScuGic_Enable(&Intc, MEMCOPY_ACCEL_INTR_ID);
    Xil_ExceptionEnable();
    //dump_regs("before_en");
    memcopy_accel_interrupt_enable();
    //dump_regs("after_en");
    xil_printf("Interrupt system setup complete.\r\n");
    return XST_SUCCESS;
}


int main()
{
	XTime tStart, tEnd;
	//uint32_t time_cpu, time_accel;

    xil_printf("\r\n--- Memcopy_accel bare-metal demo (Zybo Z7-10) ---\r\n");

    /* Initialize driver with actual base address (optional) */
    memcopy_accel_init(MEMCOPY_BASE);

    dump_regs("Before Setup interrupt controller");

    /* Setup interrupt controller*/
    if (setup_interrupt_system() != XST_SUCCESS) {
        xil_printf("ERROR: Interrupt setup failed!\r\n");
        return -1;
    }
    dump_regs("After Setup interrupt controller");


    /* Buffer sizes and allocation
       Use reasonably sized test (must be multiple of 4 in this HLS design) */
    const uint32_t NUM_WORDS = 512; /* 256 * 4 = 1024 bytes */
    const uint32_t BYTE_LEN = NUM_WORDS * 4;

    /* Allocate buffers in DDR (malloc from heap) */
    uint32_t *src_buf = (uint32_t *)malloc(BYTE_LEN);
    uint32_t *dst_buf = (uint32_t *)malloc(BYTE_LEN);
    //uint32_t *dst_buf_cpu = (uint32_t *)malloc(BYTE_LEN);

    /*
    if (!src_buf || !dst_buf || !dst_buf_cpu) {
        xil_printf("Error: malloc failed\r\n");
        return -1;
    }
	*/

    /* Fill source with pattern and clear destination */
    for (uint32_t i = 0; i < NUM_WORDS; ++i) {
        src_buf[i] = 0xA5A50000u | i;
        dst_buf[i] = 0x0;
        //dst_buf_cpu[i] = 0x0;
    }

    xil_printf("\nSource buffer at 0x%08x, Dest buffer at 0x%08x, len=%u bytes\r\n",
               (unsigned int)src_buf, (unsigned int)dst_buf, BYTE_LEN);

    /* Cache maintenance:
       - Flush source region so PL (via HP port) sees latest data
       - Flush destination region as well to avoid dirty data being written back incorrectly.
         For correct read-after-write, after accelerator finishes we will invalidate dest region. */
    Xil_DCacheFlushRange((unsigned int)src_buf, BYTE_LEN);
    Xil_DCacheFlushRange((unsigned int)dst_buf, BYTE_LEN);
   // Xil_DCacheFlushRange((unsigned int)dst_buf_cpu, BYTE_LEN);

    /* Start accelerator (pass physical addresses).
       In bare-metal on Zynq PL masters access the same address space, so virtual==physical for PS. */

    xil_printf("Starting accelerator...\r\n");
    XTime_GetTime(&tStart);

    memcopy_done = 0;
    //memcopy_accel_copy_polling((uint32_t)src_buf, (uint32_t)dst_buf, BYTE_LEN);
    memcopy_accel_start((uint32_t)src_buf, (uint32_t)dst_buf, BYTE_LEN);

    /* Wait for interrupt (ISR will set memcopy_done) */
    while (!memcopy_done) {
        /* optionally do other work here */
    	xil_printf("do sthing here!! \r\n");
    	xil_printf("do sthing here!! \r\n");
    	xil_printf("do sthing here!! \r\n");
    	xil_printf("do sthing here!! \r\n");
    	xil_printf("do sthing here!! \r\n");

    }


    XTime_GetTime(&tEnd);
    uint32_t time_accel = (uint32_t)((tEnd - tStart) / (COUNTS_PER_SECOND / 1000000));

    xil_printf("Accelerator finished!\r\n");

    xil_printf("Copy done!! \r\n");
    /* Once done, invalidate destination cache region so CPU reads updated data from DDR */
    Xil_DCacheInvalidateRange((unsigned int)dst_buf, BYTE_LEN);

    /* Verify */
    int errors = 0;
    for (uint32_t i = 0; i < NUM_WORDS; ++i) {
        if (dst_buf[i] != src_buf[i]) {
            xil_printf("Mismatch at idx %u: src=0x%08x dst=0x%08x\r\n",
                       i, src_buf[i], dst_buf[i]);
            errors++;
            if (errors > 10) break;
        }

    }

    if (errors == 0) {
        xil_printf("memcopy_accel test PASSED. %u words copied.\r\n", NUM_WORDS);
    } else {
        xil_printf("memcopy_accel test FAILED with %d errors.\r\n", errors);
    }

    /*
    XTime_GetTime(&tStart);
    cpu_memcopy(src_buf, dst_buf_cpu, BYTE_LEN);
    XTime_GetTime(&tEnd);
    time_cpu = (uint32_t)((tEnd - tStart) / (COUNTS_PER_SECOND / 1000000));

	*/
    xil_printf("\r\n------------------------------------------\r\n");
    //xil_printf("CPU memcpy done in %d us\r\n", time_cpu);
    xil_printf("Accelerator memcpy done in %d us\r\n", time_accel);



    /* Clean up */
    free(src_buf);
    free(dst_buf);
    //free(dst_buf_cpu);



    xil_printf("Demo complete.\r\n");


    /* Optionally loop forever */
    while (1) {
        /* idle */
    }

    return 0;
}
