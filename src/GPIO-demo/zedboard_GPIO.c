#include "xparameters.h"
#include "xil_printf.h"
#include "xgpio.h"
#include "xil_types.h"

#define LED_DEV_ID      XPAR_AXI_GPIO_LED_DEVICE_ID   // Update to match xparameters.h
#define SW_DEV_ID       XPAR_AXI_GPIO_SW_DEVICE_ID    // Update to match xparameters.h
#define LED_CHANNEL     1
#define SW_CHANNEL      1
#define LED_MASK        0xFF
#define SW_MASK         0xFF

int main() {
    XGpio_Config *cfg_ptr;
    XGpio led_gpio, sw_gpio;
    u32 sw_val;

    xil_printf("ZedBoard: Switch LED mirror (separate AXI GPIOs)\r\n");

    //Initialize LED GPIO
    cfg_ptr = XGpio_LookupConfig(LED_DEV_ID);
    if (cfg_ptr == NULL) {
        xil_printf("ERROR: LED XGpio_LookupConfig failed\r\n");
        return XST_FAILURE;
    }
    if (XGpio_CfgInitialize(&led_gpio, cfg_ptr, cfg_ptr->BaseAddress) != XST_SUCCESS) {
        xil_printf("ERROR: LED XGpio_CfgInitialize failed\r\n");
        return XST_FAILURE;
    }

    // Initialize Switch GPIO
    cfg_ptr = XGpio_LookupConfig(SW_DEV_ID);
    if (cfg_ptr == NULL) {
        xil_printf("ERROR: SW XGpio_LookupConfig failed\r\n");
        return XST_FAILURE;
    }
    if (XGpio_CfgInitialize(&sw_gpio, cfg_ptr, cfg_ptr->BaseAddress) != XST_SUCCESS) {
        xil_printf("ERROR: SW XGpio_CfgInitialize failed\r\n");
        return XST_FAILURE;
    }

    // Set directions
    XGpio_SetDataDirection(&sw_gpio,  SW_CHANNEL, SW_MASK); // input
    XGpio_SetDataDirection(&led_gpio, LED_CHANNEL, 0x00);   // output

    while (1) {
        sw_val = XGpio_DiscreteRead(&sw_gpio, SW_CHANNEL) & SW_MASK;
        XGpio_DiscreteWrite(&led_gpio, LED_CHANNEL, sw_val & LED_MASK);
    }
}
