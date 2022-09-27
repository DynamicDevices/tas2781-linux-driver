/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEV_CODEC_H__
#define __TASDEV_CODEC_H__
int tasdevice_register_codec(struct tasdevice_priv *pTAS2781);
void tasdevice_deregister_codec(struct tasdevice_priv *pTAS2781);
int tasdevice_create_controls(struct tasdevice_priv *p_tasdevice);
int tasdevice_dsp_create_control(struct tasdevice_priv *p_tasdevice);
void powercontrol_routine(struct work_struct *work);
#endif
