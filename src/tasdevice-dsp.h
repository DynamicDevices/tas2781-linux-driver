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

#ifndef __TASDEVICE_DSP_H__
#define __TASDEVICE_DSP_H__

#define MAIN_ALL_DEVICES			(0x0d)
#define MAIN_DEVICE_A				(0x01)
#define MAIN_DEVICE_B				(0x08)
#define MAIN_DEVICE_C				(0x10)
#define MAIN_DEVICE_D				(0x14)
#define COEFF_DEVICE_A				(0x03)
#define COEFF_DEVICE_B				(0x0a)
#define COEFF_DEVICE_C				(0x11)
#define COEFF_DEVICE_D				(0x15)
#define PRE_DEVICE_A				(0x04)
#define PRE_DEVICE_B				(0x0b)
#define PRE_DEVICE_C				(0x12)
#define PRE_DEVICE_D				(0x16)

#define PPC3_VERSION 0x4100
#define REGBIN_CONFIGID_BYPASS_ALL	(0)

enum channel {
	TopLeftChn = 0x00,
	TopRightChn = 0x01,
	BottomLeftChn = 0x02,
	BottomRightChn = 0x03,
	MaxChn
};

enum tasdevice_dsp_dev_idx {
	TASDEVICE_DSP_TAS_2555 = 0,
	TASDEVICE_DSP_TAS_2555_STEREO,
	TASDEVICE_DSP_TAS_2557_MONO,
	TASDEVICE_DSP_TAS_2557_DUAL_MONO,
	TASDEVICE_DSP_TAS_2559,
	TASDEVICE_DSP_TAS_2563,
	TASDEVICE_DSP_TAS_2563_DUAL_MONO = 7,
	TASDEVICE_DSP_TAS_2563_QUAD,
	TASDEVICE_DSP_TAS_2563_21,
	TASDEVICE_DSP_TAS_2781,
	TASDEVICE_DSP_TAS_2781_DUAL_MONO,
	TASDEVICE_DSP_TAS_2781_21,
	TASDEVICE_DSP_TAS_2781_QUAD,
	TASDEVICE_DSP_TAS_MAX_DEVICE
};

struct tasdevice_fw_fixed_hdr {
	unsigned int mnMagicNumber;
	unsigned int mnFWSize;
	unsigned int mnChecksum;
	unsigned int mnPPCVersion;
	unsigned int mnFWVersion;
	unsigned int mnDriverVersion;
	unsigned int mnTimeStamp;
	char mpDDCName[64];
};

struct tasdevice_dspfw_hdr {
	struct tasdevice_fw_fixed_hdr mnFixedHdr;
	unsigned int mnBinFileDocVer;
	char *mpDescription;
	unsigned short mnDeviceFamily;
	unsigned short mnDevice;
	unsigned char ndev;
};

struct TBlock {
	unsigned int mnType;
	unsigned char mbPChkSumPresent;
	unsigned char mnPChkSum;
	unsigned char mbYChkSumPresent;
	unsigned char mnYChkSum;
	unsigned int mnCommands;
	unsigned int blk_size;
	unsigned int nSublocks;
	unsigned char *mpData;
};

struct TData {
	char mpName[64];
	char *mpDescription;
	unsigned int mnBlocks;
	struct TBlock *mpBlocks;
};
struct TProgram {
	unsigned int prog_size;
	char mpName[64];
	char *mpDescription;
	unsigned char mnAppMode;
	unsigned char mnPDMI2SMode;
	unsigned char mnISnsPD;
	unsigned char mnVSnsPD;
	unsigned char mnPowerLDG;
	struct TData mData;
};

struct TConfiguration {
	unsigned int cfg_size;
	char mpName[64];
	char *mpDescription;
	unsigned char mnDevice_orientation;
	unsigned char mnDevices;
	unsigned int mProgram;
	unsigned int mnSamplingRate;
	unsigned short mnPLLSrc;
	unsigned int mnPLLSrcRate;
	unsigned int mnFsRate;
	struct TData mData;
};

struct TCalibration {
	char mpName[64];
	char *mpDescription;
	unsigned int mnProgram;
	unsigned int mnConfiguration;
	struct TData mData;
};

struct TFirmware {
	struct tasdevice_dspfw_hdr fw_hdr;
	unsigned int prog_start_offset;
	unsigned short mnPrograms;
	struct TProgram *mpPrograms;
	unsigned int cfg_start_offset;
	unsigned short mnConfigurations;
	struct TConfiguration *mpConfigurations;
	unsigned short mnCalibrations;
	struct TCalibration *mpCalibrations;
	bool bKernelFormat;
};

extern const char deviceNumber[TASDEVICE_DSP_TAS_MAX_DEVICE];

int tasdevice_dspfw_ready(const void *pVoid, void *pContext);
void tasdevice_dsp_remove(void *pContext);
void tasdevice_calbin_remove(void *pContext);
int tas2781_load_calibration(void *tas_dev, char *pFileName,
	enum channel i);
int tas2781_set_calibration(void *pContext, enum channel i,
	int nCalibration);
void tasdevice_select_tuningprm_cfg(void *pContext, int prm,
	int cfg_no, int regbin_conf_no);

#endif
