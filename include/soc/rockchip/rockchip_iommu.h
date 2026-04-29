/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __SOC_ROCKCHIP_IOMMU_H
#define __SOC_ROCKCHIP_IOMMU_H

#include <linux/device.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_ROCKCHIP_IOMMU)
bool rockchip_iommu_is_enabled(struct device *dev);
#else
static inline bool rockchip_iommu_is_enabled(struct device *dev)
{
	return false;
}
#endif

#endif /* __SOC_ROCKCHIP_IOMMU_H */
