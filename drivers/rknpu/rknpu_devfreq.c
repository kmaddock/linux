// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author: Finley Xiao <finley.xiao@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/devfreq_cooling.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/devfreq-governor.h>
#include "rknpu_drv.h"
#include "rknpu_devfreq.h"

#define POWER_DOWN_FREQ 200000000

static int npu_devfreq_target(struct device *dev, unsigned long *freq,
			      u32 flags);

static struct monitor_dev_profile npu_mdevp = {
	.type = MONITOR_TYPE_DEV,
	.low_temp_adjust = rockchip_monitor_dev_low_temp_adjust,
	.high_temp_adjust = rockchip_monitor_dev_high_temp_adjust,
	.check_rate_volt = rockchip_monitor_check_rate_volt,
};

static int npu_devfreq_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	return 0;
}

static int npu_devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);

	*freq = rknpu_dev->current_freq;

	return 0;
}

static struct devfreq_dev_profile npu_devfreq_profile = {
	.polling_ms = 50,
	.target = npu_devfreq_target,
	.get_dev_status = npu_devfreq_get_dev_status,
	.get_cur_freq = npu_devfreq_get_cur_freq,
};

static int devfreq_rknpu_ondemand_func(struct devfreq *df, unsigned long *freq)
{
	struct rknpu_device *rknpu_dev = df->data;

	if (rknpu_dev && rknpu_dev->ondemand_freq)
		*freq = rknpu_dev->ondemand_freq;
	else
		*freq = df->previous_freq;

	return 0;
}

static int devfreq_rknpu_ondemand_handler(struct devfreq *devfreq,
					  unsigned int event, void *data)
{
	return 0;
}

static struct devfreq_governor devfreq_rknpu_ondemand = {
	.name = "rknpu_ondemand",
	.get_target_freq = devfreq_rknpu_ondemand_func,
	.event_handler = devfreq_rknpu_ondemand_handler,
};

static int rk3576_npu_set_read_margin(struct device *dev,
				      struct rockchip_opp_info *opp_info,
				      u32 rm)
{
	if (!opp_info->grf || !opp_info->volt_rm_tbl)
		return 0;

	if (rm == opp_info->current_rm || rm == UINT_MAX)
		return 0;

	LOG_DEV_DEBUG(dev, "set rm to %d\n", rm);

	regmap_write(opp_info->grf, 0x08, 0x001c0000 | (rm << 2));
	regmap_write(opp_info->grf, 0x0c, 0x003c0000 | (rm << 2));
	regmap_write(opp_info->grf, 0x10, 0x001c0000 | (rm << 2));

	return 0;
}

static int rk3588_npu_get_soc_info(struct device *dev, struct device_node *np,
				   int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "specification_serial_number") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(
			np, "specification_serial_number", &value);
		if (ret) {
			LOG_DEV_ERROR(
				dev,
				"Failed to get specification_serial_number\n");
			return ret;
		}
		/* RK3588M */
		if (value == 0xd)
			*bin = 1;
		/* RK3588J */
		else if (value == 0xa)
			*bin = 2;
	}
	if (of_property_match_string(np, "nvmem-cell-names", "customer_demand") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "customer_demand", &value);
		if (ret) {
			dev_err(dev, "Failed to get customer_demand\n");
			return ret;
		}
		if (value == 0x3)
			*bin = 4;
	}
	if (*bin < 0)
		*bin = 0;
	LOG_DEV_INFO(dev, "bin=%d\n", *bin);

	return ret;
}

static int rv1126b_npu_get_soc_info(struct device *dev, struct device_node *np,
				    int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "specification_serial_number") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(
			np, "specification_serial_number", &value);
		if (ret) {
			LOG_DEV_ERROR(
				dev,
				"Failed to get specification_serial_number\n");
			return ret;
		}
		/* RV1126BM */
		if (value == 0xd)
			*bin = 1;
		/* RV1126BJ */
		else if (value == 0xa)
			*bin = 2;
		/* RV1126BP */
		else if (value == 0x10)
			*bin = 3;
	}
	if (*bin < 0)
		*bin = 0;
	LOG_DEV_INFO(dev, "bin=%d\n", *bin);

	return ret;
}

static int rk3588_npu_set_soc_info(struct device *dev, struct device_node *np,
				   struct rockchip_opp_info *opp_info)
{
	int bin = opp_info->bin;

	if (opp_info->volt_sel < 0)
		return 0;
	if (bin < 0)
		bin = 0;

	if (!of_property_read_bool(np, "rockchip,supported-hw"))
		return 0;

	/* SoC Version */
	opp_info->supported_hw[0] = BIT(bin);
	/* Speed Grade */
	opp_info->supported_hw[1] = BIT(opp_info->volt_sel);

	return 0;
}

static int rk3588_npu_set_read_margin(struct device *dev,
				      struct rockchip_opp_info *opp_info,
				      u32 rm)
{
	u32 offset = 0, val = 0;
	int i, ret = 0;

	if (!opp_info->grf || !opp_info->volt_rm_tbl)
		return 0;

	if (rm == opp_info->current_rm || rm == UINT_MAX)
		return 0;

	LOG_DEV_DEBUG(dev, "set rm to %d\n", rm);

	for (i = 0; i < 3; i++) {
		ret = regmap_read(opp_info->grf, offset, &val);
		if (ret < 0) {
			LOG_DEV_ERROR(dev, "failed to get rm from 0x%x\n",
				      offset);
			return ret;
		}
		val &= ~0x1c;
		regmap_write(opp_info->grf, offset, val | (rm << 2));
		offset += 4;
	}
	opp_info->current_rm = rm;

	return 0;
}

static int npu_opp_config_regulators(struct device *dev,
				     struct dev_pm_opp *old_opp,
				     struct dev_pm_opp *new_opp,
				     struct regulator **regulators,
				     unsigned int count)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);

	return rockchip_opp_config_regulators(dev, old_opp, new_opp, regulators,
					      count, &rknpu_dev->opp_info);
}

static int npu_opp_config_clks(struct device *dev, struct opp_table *opp_table,
			       struct dev_pm_opp *opp, void *data,
			       bool scaling_down)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);

	return rockchip_opp_config_clks(dev, opp_table, opp, data, scaling_down,
					&rknpu_dev->opp_info);
}

static const struct rockchip_opp_data rk3576_npu_opp_data = {
	.set_read_margin = rk3576_npu_set_read_margin,
	.set_soc_info = rockchip_opp_set_low_length,
	.config_regulators = npu_opp_config_regulators,
	.config_clks = npu_opp_config_clks,
};

static const struct rockchip_opp_data rk3588_npu_opp_data = {
	.get_soc_info = rk3588_npu_get_soc_info,
	.set_soc_info = rk3588_npu_set_soc_info,
	.set_read_margin = rk3588_npu_set_read_margin,
	.config_regulators = npu_opp_config_regulators,
	.config_clks = npu_opp_config_clks,
};

static const struct rockchip_opp_data rv1126b_npu_opp_data = {
	.is_use_pvtpll = true,
	.get_soc_info = rv1126b_npu_get_soc_info,
	.set_soc_info = rk3588_npu_set_soc_info,
	.config_regulators = npu_opp_config_regulators,
	.config_clks = npu_opp_config_clks,
};

static const struct of_device_id rockchip_npu_of_match[] = {
	{
		.compatible = "rockchip,rk3576",
		.data = (void *)&rk3576_npu_opp_data,
	},
	{
		.compatible = "rockchip,rk3576s",
		.data = (void *)&rk3576_npu_opp_data,
	},
	{
		.compatible = "rockchip,rk3588",
		.data = (void *)&rk3588_npu_opp_data,
	},
	{
		.compatible = "rockchip,rv1126b",
		.data = (void *)&rv1126b_npu_opp_data,
	},
	{},
};

void rknpu_devfreq_lock(struct rknpu_device *rknpu_dev)
{
	if (rknpu_dev->devfreq)
		rockchip_opp_dvfs_lock(&rknpu_dev->opp_info);
}
EXPORT_SYMBOL(rknpu_devfreq_lock);

void rknpu_devfreq_unlock(struct rknpu_device *rknpu_dev)
{
	if (rknpu_dev->devfreq)
		rockchip_opp_dvfs_unlock(&rknpu_dev->opp_info);
}
EXPORT_SYMBOL(rknpu_devfreq_unlock);

static int npu_devfreq_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu_dev->opp_info;
	struct dev_pm_opp *opp;
	unsigned long opp_volt;
	int ret = 0;

	if (!opp_info->is_rate_volt_checked)
		return -EINVAL;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);
	opp_volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	if (*freq == rknpu_dev->current_freq)
		return 0;

	rockchip_opp_dvfs_lock(opp_info);
	if (pm_runtime_active(dev))
		opp_info->is_runtime_active = true;
	else
		opp_info->is_runtime_active = false;
	ret = dev_pm_opp_set_rate(dev, *freq);
	if (!ret) {
		rknpu_dev->current_freq = *freq;
		if (rknpu_dev->devfreq)
			rknpu_dev->devfreq->last_status.current_frequency =
				*freq;
		rknpu_dev->current_volt = opp_volt;
		LOG_DEV_DEBUG(dev, "set rknpu freq: %lu, volt: %lu\n",
			      rknpu_dev->current_freq, rknpu_dev->current_volt);
	}
	rockchip_opp_dvfs_unlock(opp_info);

	return ret;
}

static const struct rockchip_opp_data rockchip_npu_opp_data = {
	.config_clks = npu_opp_config_clks,
};

int rknpu_devfreq_init(struct rknpu_device *rknpu_dev)
{
	struct rockchip_opp_info *info = &rknpu_dev->opp_info;
	struct device *dev = rknpu_dev->dev;
	struct devfreq_dev_profile *dp;
	struct dev_pm_opp *opp;
	unsigned int dyn_power_coeff = 0;
	int ret = 0;

	info->data = &rockchip_npu_opp_data;
	rockchip_get_opp_data(rockchip_npu_of_match, info);
	ret = rockchip_init_opp_table(dev, info, "clk_npu", "rknpu");
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init_opp_table\n");
		return -EINVAL;
	}

	rknpu_dev->current_freq = clk_get_rate(rknpu_dev->clks[0].clk);
	opp = devfreq_recommended_opp(dev, &rknpu_dev->current_freq, 0);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto err_uinit_table;
	}
	dev_pm_opp_put(opp);

	dp = &npu_devfreq_profile;
	dp->initial_freq = rknpu_dev->current_freq;
	of_property_read_u32(dev->of_node, "dynamic-power-coefficient",
			     &dyn_power_coeff);
	if (dyn_power_coeff)
		dp->is_cooling_device = true;

	ret = devfreq_add_governor(&devfreq_rknpu_ondemand);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to add rknpu_ondemand governor\n");
		goto err_uinit_table;
	}

	rknpu_dev->devfreq = devm_devfreq_add_device(dev, dp, "rknpu_ondemand",
						     (void *)rknpu_dev);
	if (IS_ERR(rknpu_dev->devfreq)) {
		LOG_DEV_ERROR(dev, "failed to add devfreq\n");
		ret = PTR_ERR(rknpu_dev->devfreq);
		rknpu_dev->devfreq = NULL;
		goto err_remove_governor;
	}

	npu_mdevp.data = rknpu_dev->devfreq;
	npu_mdevp.opp_info = &rknpu_dev->opp_info;
	rknpu_dev->mdev_info =
		rockchip_system_monitor_register(dev, &npu_mdevp);
	if (IS_ERR(rknpu_dev->mdev_info)) {
		dev_dbg(dev, "without system monitor\n");
		rknpu_dev->mdev_info = NULL;
	}

	rknpu_dev->current_freq = clk_get_rate(rknpu_dev->clks[0].clk);
	rknpu_dev->ondemand_freq = rknpu_dev->current_freq;
	rknpu_dev->current_volt = regulator_get_voltage(rknpu_dev->vdd);

	rknpu_dev->devfreq->previous_freq = rknpu_dev->current_freq;
	if (rknpu_dev->devfreq->suspend_freq)
		rknpu_dev->devfreq->resume_freq = rknpu_dev->current_freq;
	rknpu_dev->devfreq->last_status.current_frequency =
		rknpu_dev->current_freq;
	rknpu_dev->devfreq->last_status.total_time = 1;
	rknpu_dev->devfreq->last_status.busy_time = 1;

	return 0;

err_remove_governor:
	devfreq_remove_governor(&devfreq_rknpu_ondemand);
err_uinit_table:
	rockchip_uninit_opp_table(dev, info);

	return ret;
}
EXPORT_SYMBOL(rknpu_devfreq_init);

int rknpu_devfreq_runtime_suspend(struct device *dev)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu_dev->opp_info;

	if (rockchip_opp_is_use_pvtpll(opp_info)) {
		if (clk_set_rate(opp_info->clk, POWER_DOWN_FREQ))
			LOG_DEV_ERROR(dev, "failed to restore clk rate\n");
	}
	opp_info->current_rm = UINT_MAX;

	return 0;
}
EXPORT_SYMBOL(rknpu_devfreq_runtime_suspend);

int rknpu_devfreq_runtime_resume(struct device *dev)
{
	struct rknpu_device *rknpu_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu_dev->opp_info;
	int ret = 0;

	if (!rknpu_dev->current_freq || !rknpu_dev->current_volt)
		return 0;

	ret = clk_bulk_prepare_enable(opp_info->nclocks, opp_info->clocks);
	if (ret) {
		LOG_DEV_INFO(dev, "failed to enable opp clks\n");
		return ret;
	}

	if (opp_info->data && opp_info->data->set_read_margin)
		opp_info->data->set_read_margin(dev, opp_info,
						opp_info->target_rm);
	if (rockchip_opp_is_use_pvtpll(opp_info)) {
		if (clk_set_rate(opp_info->clk, rknpu_dev->current_freq))
			LOG_DEV_ERROR(dev, "failed to set power down rate\n");
	}

	clk_bulk_disable_unprepare(opp_info->nclocks, opp_info->clocks);

	return ret;
}
EXPORT_SYMBOL(rknpu_devfreq_runtime_resume);

void rknpu_devfreq_remove(struct rknpu_device *rknpu_dev)
{
	if (rknpu_dev->mdev_info) {
		rockchip_system_monitor_unregister(rknpu_dev->mdev_info);
		rknpu_dev->mdev_info = NULL;
	}
	if (rknpu_dev->devfreq)
		devfreq_remove_governor(&devfreq_rknpu_ondemand);
	rockchip_uninit_opp_table(rknpu_dev->dev, &rknpu_dev->opp_info);
}
EXPORT_SYMBOL(rknpu_devfreq_remove);
