#include "memcopy_accel.h"
#include "xil_io.h"
#include "xil_printf.h"

static uint32_t base_addr = MEMCOPY_ACCEL_BASEADDR;

void memcopy_accel_init(uint32_t baseaddr)
{
    base_addr = baseaddr;
}

void memcopy_accel_start(uint32_t src_addr, uint32_t dst_addr, uint32_t len)
{
    /* Write parameters */
    Xil_Out32(base_addr + MEMCOPY_ACCEL_SRC_OFFSET, src_addr);
    Xil_Out32(base_addr + MEMCOPY_ACCEL_DST_OFFSET, dst_addr);
    Xil_Out32(base_addr + MEMCOPY_ACCEL_LEN_OFFSET, len);
    /* Start IP (write ap_start = 1) */
    Xil_Out32(base_addr + MEMCOPY_ACCEL_CTRL_OFFSET, MEMCOPY_AP_START_MASK);
}

bool memcopy_accel_is_done(void)
{
    uint32_t ctrl = Xil_In32(base_addr + MEMCOPY_ACCEL_CTRL_OFFSET);
    return (ctrl & MEMCOPY_AP_DONE_MASK) != 0;
}

void memcopy_accel_wait_done(void)
{
    /* Poll ap_done */
    while (!memcopy_accel_is_done()) { /* busy wait */ }
    /* Clear ap_done by reading control register (HLS will clear on handshake or write 0) */
    /* Often ap_done auto-clears on read; safe to clear ap_start (write 0) */
    Xil_Out32(base_addr + MEMCOPY_ACCEL_CTRL_OFFSET, 0x0);
}

int memcopy_accel_copy_polling(uint32_t src_addr, uint32_t dst_addr, uint32_t len)
{
    memcopy_accel_start(src_addr, dst_addr, len);
    memcopy_accel_wait_done();
    return 0;
}

/* -------------------------------------------------------
 * Enable interrupt (Global + DONE)
 * ------------------------------------------------------- */
void memcopy_accel_interrupt_enable(void)
{
    Xil_Out32(base_addr + MEMCOPY_ACCEL_GIE_OFFSET, MEMCOPY_GIE_ENABLE_MASK);
    Xil_Out32(base_addr + MEMCOPY_ACCEL_IER_OFFSET, MEMCOPY_IER_CHAN0_INT_EN_MASK);
}

/* -------------------------------------------------------
 * Clear interrupt after handling
 * ------------------------------------------------------- */
void memcopy_accel_interrupt_clear(void)
{
    Xil_Out32(base_addr + MEMCOPY_ACCEL_ISR_OFFSET, MEMCOPY_ISR_CHAN0_INT_CLEAR_MASK);
}
