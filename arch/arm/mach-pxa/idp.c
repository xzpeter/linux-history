/*
 *  linux/arch/arm/mach-pxa/idp.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Copyright (c) 2001 Cliff Brake, Accelent Systems Inc.
 *
 *  2001-09-13: Cliff Brake <cbrake@accelent.com>
 *              Initial code
 */
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "generic.h"

#define PXA_IDP_REV02

#ifndef PXA_IDP_REV02
/* shadow registers for write only registers */
unsigned int idp_cpld_led_control_shadow = 0x1;
unsigned int idp_cpld_periph_pwr_shadow = 0xd;
unsigned int ipd_cpld_cir_shadow = 0;
unsigned int idp_cpld_kb_col_high_shadow = 0;
unsigned int idp_cpld_kb_col_low_shadow = 0;
unsigned int idp_cpld_pccard_en_shadow = 0xC3;
unsigned int idp_cpld_gpioh_dir_shadow = 0;
unsigned int idp_cpld_gpioh_value_shadow = 0;
unsigned int idp_cpld_gpiol_dir_shadow = 0;
unsigned int idp_cpld_gpiol_value_shadow = 0;

/*
 * enable all LCD signals -- they should still be on
 * write protect flash
 * enable all serial port transceivers
 */

unsigned int idp_control_port_shadow = ((0x7 << 21) | 		/* LCD power */
					(0x1 << 19) |		/* disable flash write enable */
					(0x7 << 9));		/* enable serial port transeivers */

#endif

static int __init idp_init(void)
{
	printk("idp_init()\n");
	return 0;
}

__initcall(idp_init);

static void __init idp_init_irq(void)
{
	pxa_init_irq();
}

static struct map_desc idp_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */


#ifndef PXA_IDP_REV02
  { IDP_CTRL_PORT_BASE,
    IDP_CTRL_PORT_PHYS,
    IDP_CTRL_PORT_SIZE,
    DOMAIN_IO,
    0, 1, 0, 0 },
#endif

  { IDP_IDE_BASE,
    IDP_IDE_PHYS,
    IDP_IDE_SIZE,
    DOMAIN_IO,
    0, 1, 0, 0 },
  { IDP_ETH_BASE,
    IDP_ETH_PHYS,
    IDP_ETH_SIZE,
    DOMAIN_IO,
    0, 1, 0, 0 },
  { IDP_COREVOLT_BASE,
    IDP_COREVOLT_PHYS,
    IDP_COREVOLT_SIZE,
    DOMAIN_IO,
    0, 1, 0, 0 },
  { IDP_CPLD_BASE,
    IDP_CPLD_PHYS,
    IDP_CPLD_SIZE,
    DOMAIN_IO,
    0, 1, 0, 0 },

  LAST_DESC
};

static void __init idp_map_io(void)
{
	pxa_map_io();
	iotable_init(idp_io_desc);

	set_GPIO_IRQ_edge(IRQ_TO_GPIO_2_80(TOUCH_PANEL_IRQ), TOUCH_PANEL_IRQ_EDGE);
}

MACHINE_START(PXA_IDP, "Accelent Xscale IDP")
	MAINTAINER("Accelent Systems Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, 0xfc000000)
	MAPIO(idp_map_io)
	INITIRQ(idp_init_irq)
MACHINE_END