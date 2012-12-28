/**
 *******************************************************************************
 * @file    kernel_init.c
 * @author  Olli Vanhoja
 * @brief   System init for Zero Kernel.
 *******************************************************************************
 */

#include "kernel.h"
#include "sched.h"
#include "app_main.h"
#include "hal_mcu.h"

static char main_Stack[configAPP_MAIN_SSIZE];

int main(void)
{
    if (interrupt_init_module()) {
		while (1);
	}
    sched_init();
    {
        /* Create app_main thread */
        osThreadDef_t main_thread = { (os_pthread)(&app_main), configAPP_MAIN_PRI, main_Stack, sizeof(main_Stack)/sizeof(char) };
        osThreadCreate(&main_thread, NULL);
    }
    sched_start();
    while (1);
}
