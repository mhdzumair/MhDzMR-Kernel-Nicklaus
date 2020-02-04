/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_BOOT_REASON_H__
#define __MT_BOOT_REASON_H__

enum boot_reason_t {
	BR_POWER_KEY = 0,
	BR_USB,
	BR_RTC,
	BR_WDT,
	BR_WDT_BY_PASS_PWK,
	BR_TOOL_BY_PASS_PWK,
	BR_2SEC_REBOOT,
	BR_UNKNOWN
#ifdef CONFIG_LCT_BOOTINFO_SUPPORT// By shaohui - 2016-12-13-18-32
	,BR_KERNEL_PANIC,
	BR_WDT_SW,
	BR_WDT_HW
	,BR_CHR,
	BR_PWR_RST
#endif
};

extern enum boot_reason_t get_boot_reason(void);

#endif
