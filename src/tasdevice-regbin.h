/*
* Copyright (C) 2022, Texas Instruments Incorporated
*
* All rights reserved not granted herein.
* Limited License.
*
* Texas Instruments Incorporated grants a world-wide, royalty-free,
* non-exclusive license under copyrights and patents it now or hereafter
* owns or controls to make, have made, use, import, offer to sell and sell
* ("Utilize"), this software subject to the terms herein. With respect to the
* foregoing patent license, such license is granted solely to the extent that
* any such patent is necessary to Utilize the software alone.
* The patent license shall not apply to any combinations which include this
* software, other than combinations with devices manufactured by or for
* TI (TAS2781). No hardware patent is licensed hereunder.
* Redistributions must preserve existing copyright notices and reproduce this
* license (including the above copyright notice and the disclaimer and
* (if applicable) source code license limitations below)
* in the documentation and/or other materials provided with the distribution
*
* Redistribution and use in binary form, without modification, are permitted
* provided that the following conditions are met:
*
* * No reverse engineering, decompilation, or disassembly of this software is
* permitted with respect to any software provided in binary form.
* * any redistribution and use are licensed by TI for use only with TI Devices.
* * Nothing shall obligate TI to provide you with source code for the software
* licensed and provided to you in object code.
*
* If software source code is provided to you, modification and redistribution
* of the source code are permitted
* provided that the following conditions are met:
*
* * any redistribution and use of the source code, including any resulting
* derivative works, are licensed by TI for use only with TI Devices.
* * any redistribution and use of any object code compiled from the source
* code and any resulting derivative works, are licensed by TI for use only
* with TI Devices.
*
* Neither the name of Texas Instruments Incorporated nor the names of its
* suppliers may be used to endorse or promote products derived from this
* software without specific prior written permission.
*
* DISCLAIMER.
*
* THIS SOFTWARE IS PROVIDED BY TI AND TI?S LICENSORS "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL TI AND TI?S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __TASDEVICE_REGBIN_H__
#define __TASDEVICE_REGBIN_H__

#define TASDEVICE_CONFIG_SUM  (64)
#define TASDEVICE_DEVICE_SUM  (8)

#define TASDEVICE_CMD_SING_W  (0x1)
#define TASDEVICE_CMD_BURST  (0x2)
#define TASDEVICE_CMD_DELAY  (0x3)
#define TASDEVICE_CMD_FIELD_W  (0x4)

enum tasdevice_dsp_fw_state {
	TASDEVICE_DSP_FW_NONE = 0,
	TASDEVICE_DSP_FW_PENDING,
	TASDEVICE_DSP_FW_FAIL,
	TASDEVICE_DSP_FW_ALL_OK,
};

enum tasdevice_bin_blk_type {
	TASDEVICE_BIN_BLK_COEFF = 1,
	TASDEVICE_BIN_BLK_POST_POWER_UP,
	TASDEVICE_BIN_BLK_PRE_SHUTDOWN,
	TASDEVICE_BIN_BLK_PRE_POWER_UP,
	TASDEVICE_BIN_BLK_POST_SHUTDOWN
};

struct tasdevice_regbin_hdr {
	unsigned int img_sz;
	unsigned int checksum;
	unsigned int binary_version_num;
	unsigned int drv_fw_version;
	unsigned int timestamp;
	unsigned char plat_type;
	unsigned char dev_family;
	unsigned char reserve;
	unsigned char ndev;
	unsigned char devs[TASDEVICE_DEVICE_SUM];
	unsigned int nconfig;
	unsigned int config_size[TASDEVICE_CONFIG_SUM];
};

struct tasdevice_block_data {
	unsigned char dev_idx;
	unsigned char block_type;
	unsigned short yram_checksum;
	unsigned int block_size;
	unsigned int nSublocks;
	unsigned char *regdata;
};

struct tasdevice_config_info {
	char mpName[64];
	unsigned int nblocks;
	unsigned int real_nblocks;
	unsigned char active_dev;
	struct tasdevice_block_data **blk_data;
};

struct tasdevice_regbin {
	struct tasdevice_regbin_hdr fw_hdr;
	int ncfgs;
	struct tasdevice_config_info **cfg_info;
	int profile_cfg_id;
};

void tasdevice_regbin_ready(const struct firmware *pFW,
	void *pContext);
void tasdevice_config_info_remove(void *pContext);
void tasdevice_powerup_regcfg_dev(void *pContext,
	unsigned char dev);
void tasdevice_select_cfg_blk(void *pContext, int conf_no,
	unsigned char block_type);
int tasdevice_process_block(void *pContext,
	unsigned char *data, unsigned char dev_idx, int sublocksize);
int tasdevice_process_block_show(void *pContext,
	unsigned char *data, unsigned char dev_idx, int sublocksize,
	char *buf, ssize_t *len);
#endif
