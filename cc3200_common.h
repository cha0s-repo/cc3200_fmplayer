// cc3200_common.h

#ifndef _CC3200_COMMON_H
#define _CC3200_COMMON_H

// Simplelink includes
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"

#include "hw_memmap.h"
//Free_rtos/ti-rtos includes
#include "osi.h"

//Common interface includes
#include "gpio_if.h"
#ifndef NOTERM
#include "uart_if.h"
#endif
#include "common.h"
#include "pinmux.h"

#endif
