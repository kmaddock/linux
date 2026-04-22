/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 */
#ifndef __SOC_ROCKCHIP_SIP_H
#define __SOC_ROCKCHIP_SIP_H

#define ROCKCHIP_SIP_SUSPEND_MODE		0x82000003
#define ROCKCHIP_SLEEP_PD_CONFIG		0xff

#define ROCKCHIP_SIP_DRAM_FREQ			0x82000008
#define ROCKCHIP_SIP_CONFIG_DRAM_INIT		0x00
#define ROCKCHIP_SIP_CONFIG_DRAM_SET_RATE	0x01
#define ROCKCHIP_SIP_CONFIG_DRAM_ROUND_RATE	0x02
#define ROCKCHIP_SIP_CONFIG_DRAM_SET_AT_SR	0x03
#define ROCKCHIP_SIP_CONFIG_DRAM_GET_BW		0x04
#define ROCKCHIP_SIP_CONFIG_DRAM_GET_RATE	0x05
#define ROCKCHIP_SIP_CONFIG_DRAM_CLR_IRQ	0x06
#define ROCKCHIP_SIP_CONFIG_DRAM_SET_PARAM	0x07
#define ROCKCHIP_SIP_CONFIG_DRAM_SET_ODT_PD	0x08

/*
 * PVTPLL firmware commands - not implemented in upstream ATF.
 * Provide stub definitions so the driver compiles; the SMC calls
 * will return non-zero (failure) causing the driver to fall back
 * to the non-SMC PVTPLL path.
 */
#include <linux/arm-smccc.h>

#define PVTPLL_GET_INFO		0
#define PVTPLL_ADJUST_TABLE	1
#define PVTPLL_VOLT_SEL		2
#define PVTPLL_LOW_TEMP		3

static inline struct arm_smccc_res
sip_smc_get_pvtpll_info(u32 cmd, u32 clk_id)
{
	struct arm_smccc_res res = { .a0 = (unsigned long)-ENODEV };
	return res;
}

static inline struct arm_smccc_res
sip_smc_pvtpll_config(u32 cmd, u32 clk_id,
		      u32 a, u32 b, u32 c, u32 d, u32 e)
{
	struct arm_smccc_res res = { .a0 = (unsigned long)-ENODEV };
	return res;
}

/*
 * DRAM refresh interval update SIP command - not in upstream ATF.
 * Provide a stub so the driver compiles; returns failure.
 */
#define ROCKCHIP_SIP_CONFIG_DRAM_TREFI_UPD	0x09

static inline struct arm_smccc_res
sip_smc_dram(u32 arg0, u32 arg1, u32 cmd)
{
	struct arm_smccc_res res = { .a0 = (unsigned long)-ENODEV };
	return res;
}

#endif
