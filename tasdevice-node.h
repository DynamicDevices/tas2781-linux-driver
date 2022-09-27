/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEVICE_NODE_H__
#define __TASDEVICE_NODE_H__

ssize_t dspfwinfo_list_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t active_address_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t reg_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
ssize_t regdump_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t regdump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
ssize_t regbininfo_list_show(struct device *dev,
				struct device_attribute *attr, char *buf);
ssize_t regcfg_list_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
ssize_t regcfg_list_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t fwload_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
ssize_t dspfw_config_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t dev_addr_show(struct device *dev,
	struct device_attribute *attr, char *buf);

#endif
