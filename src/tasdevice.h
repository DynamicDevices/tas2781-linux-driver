/*
 * TAS2871 Linux Driver
 *
 * Copyright (C) 2022 Texas Instruments Incorporated 
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TASDEVICE_H__
#define __TASDEVICE_H__
#include "tasdevice-regbin.h"
#include "tasdevice-dsp.h"
#include <linux/miscdevice.h>
#include <linux/regmap.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/of.h>
#ifdef CONFIG_COMPAT
	#include <linux/compat.h>
#endif

#define SMARTAMP_MODULE_NAME	("tas2781")
#define MAX_LENGTH				(128)

#define TASDEVICE_RETRY_COUNT	(3)
#define TASDEVICE_ERROR_FAILED	(-2)

#define TASDEVICE_RATES	(SNDRV_PCM_RATE_44100 |\
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |\
	SNDRV_PCM_RATE_88200)
#define TASDEVICE_MAX_CHANNELS (8)

#define TASDEVICE_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

/*PAGE Control Register (available in page0 of each book) */
#define TASDEVICE_PAGE_SELECT	(0x00)
#define TASDEVICE_BOOKCTL_PAGE	(0x00)
#define TASDEVICE_BOOKCTL_REG	(127)
#define TASDEVICE_BOOK_ID(reg)		(reg / (256 * 128))
#define TASDEVICE_PAGE_ID(reg)		((reg % (256 * 128)) / 128)
#define TASDEVICE_PAGE_REG(reg)		((reg % (256 * 128)) % 128)
#define TASDEVICE_REG(book, page, reg)	(((book * 256 * 128) + \
					(page * 128)) + reg)

	/*Software Reset */
#define TASDEVICE_REG_SWRESET  TASDEVICE_REG(0x0, 0X0, 0x02)
#define TASDEVICE_REG_SWRESET_RESET  (0x1 << 0)

#define SMS_HTONS(a, b)  ((((a)&0x00FF)<<8) | \
				((b)&0x00FF))
#define SMS_HTONL(a, b, c, d) ((((a)&0x000000FF)<<24) |\
					(((b)&0x000000FF)<<16) | \
					(((c)&0x000000FF)<<8) | \
					((d)&0x000000FF))

	/*Power Control */
#define TASDEVICE_REG_POWERCONTROL  TASDEVICE_REG(0x0, 0x0, 0x02)
#define TASDEVICE_REG_POWERCONTROL_PWRON  (0)
		/*Interrupt Mask Reg0 */
#define TASDEVICE_InterruptMaskReg0  TASDEVICE_REG(0x0, 0x0, 0x3B)
#define TASDEVICE_InterruptMaskReg0_TDMClockErrorINTMASK_Mask  (0x1 << 2)
#define TASDEVICE_InterruptMaskReg0_TDMClockErrorINTMASK_ENABLE  (0x0 << 2)
#define TASDEVICE_InterruptMaskReg0_TDMClockErrorINTMASK_Disable  (0x1 << 2)
#define TASDEVICE_InterruptMaskReg0_OCEINTMASK_Mask  (0x1 << 1)
#define TASDEVICE_InterruptMaskReg0_OCEINTMASK_ENABLE  (0x0 << 1)
#define TASDEVICE_InterruptMaskReg0_OCEINTMASK_Disable  (0x1 << 1)
#define TASDEVICE_InterruptMaskReg0_Disable (0xff)

		/*Interrupt Mask Reg1 */
#define TASDEVICE_InterruptMaskReg1  TASDEVICE_REG(0x0, 0x0, 0x3C)
#define TASDEVICE_InterruptMaskReg1_Disable (0xff)

		/*Interrupt Mask Reg2 */
#define TASDEVICE_InterruptMaskReg2  TASDEVICE_REG(0x0, 0x0, 0x3D)
#define TASDEVICE_InterruptMaskReg2_Disable (0xff)

	/*Latched-Interrupt Reg0 */
#define TASDEVICE_LATCHEDINTERRUPTREG0	TASDEVICE_REG(0x0, 0x0, 0x49)
#define TASDEVICE_LATCHEDINTERRUPTREG0_TDMCLOCKERRORSTICKY_MASK	(0x1 << 2)
#define TASDEVICE_LATCHEDINTERRUPTREG0_TDMCLOCKERRORSTICKY_NOINTERRUPT	\
	(0x0 << 2)
#define TASDEVICE_LATCHEDINTERRUPTREG0_TDMCLOCKERRORSTICKY_INTERRUPT	\
	(0x1 << 2)
#define TASDEVICE_LATCHEDINTERRUPTREG0_OCEFLAGSTICKY_MASK  (0x1 << 1)
#define TASDEVICE_LATCHEDINTERRUPTREG0_OCEFLAGSTICKY_NOINTERRUPT  (0x0 << 1)
#define TASDEVICE_LATCHEDINTERRUPTREG0_OCEFLAGSTICKY_INTERRUPT  (0x1 << 1)

	/*Interrupt Configuration */
#define TASDEVICE_INTERRUPTCONFIGURATION	TASDEVICE_REG(0x0, 0x0, 0x5C)
#define TASDEVICE_INTERRUPTCONFIGURATION_LTCHINTCLEAR_MASK (0x1 << 2)
#define TASDEVICE_INTERRUPTCONFIGURATION_LTCHINTCLEAR (0x1 << 2)

	/*Revision and PG ID */
#define TASDEVICE_REG_REV_PGID  TASDEVICE_REG(0x0, 0x0, 0x7D)

#define TAS2781_REG_INT_STATUS  TASDEVICE_REG(0x0, 0X0, 0x78)
#define TAS2781_REG_INT_STATUS_DAC_CLASSD_ISNS_VSNS_STATUS_MASK		(0xF)
#define TAS2781_REG_INT_STATUS_DAC_CLASSD_ISNS_VSNS_STATUS_POWERUP	(0xF)

	/*I2C Checksum */
#define TASDEVICE_I2CChecksum  TASDEVICE_REG(0x0, 0x0, 0x7E)

#define TAS2781_REG_MISC_DSP  TASDEVICE_REG(0x0, 0x1, 0x02)
#define TAS2781_REG_MISC_DSP_ROM_MODE_MASK			\
	(0xf << 1)
/*DSP mode*/
#define TAS2781_REG_MISC_DSP_ROM_MODE_RAM_MODE			(0x0 << 1)
/*bypass mode*/
#define TAS2781_REG_MISC_DSP_ROM_MODE_ROM_MODE			(0x1 << 1)

#define TAS2781_DEVICEID  TASDEVICE_REG(0x0, 0xFD, 0x7C)
#define TAS2781_REG_UNLCK		TASDEVICE_REG(0x0, 0xFD, 0x0D)

#define TASDEVICE_CMD_SING_W	(0x1)
#define TASDEVICE_CMD_BURST	(0x2)
#define TASDEVICE_CMD_DELAY	(0x3)

enum audio_device {
	GENERAL_AUDEV = 0,
	TAS2781	  = 1,
};

struct smartpa_params {
	int mProg;
	int config;
	int regscene;
};

struct smartpa_info {
	unsigned char spkvendorid;
	unsigned char ndev;
	unsigned char i2c_list[MaxChn];
	unsigned char bSPIEnable;
};

struct smartpa_gpio_info {
	unsigned char ndev;
	int mnResetGpio[MaxChn];
};


#define TILOAD_IOC_MAGIC			(0xE0)
#define TILOAD_IOMAGICNUM_GET		_IOR(TILOAD_IOC_MAGIC, 1, int)
#define TILOAD_IOMAGICNUM_SET		_IOW(TILOAD_IOC_MAGIC, 2, int)
#define TILOAD_BPR_READ			_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_BPR_WRITE		_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_IOCTL_SET_CHL		_IOW(TILOAD_IOC_MAGIC, 5, int)
#define TILOAD_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, int)

#define TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI	_IOWR(TILOAD_IOC_MAGIC, 15, \
	void *)
#define TILOAD_IOC_MAGIC_PA_INFO_GET	_IOR(TILOAD_IOC_MAGIC, 34, \
	struct smartpa_info)

#ifdef CONFIG_COMPAT
#define TILOAD_COMPAT_IOMAGICNUM_GET	_IOR(TILOAD_IOC_MAGIC, 1, compat_int_t)
#define TILOAD_COMPAT_IOMAGICNUM_SET	_IOW(TILOAD_IOC_MAGIC, 2, compat_int_t)
#define TILOAD_COMPAT_BPR_READ		_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_COMPAT_BPR_WRITE		_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_COMPAT_IOCTL_SET_CHL	_IOW(TILOAD_IOC_MAGIC, 5, compat_int_t)
#define TILOAD_COMPAT_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, \
	compat_int_t)
#define TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI_COMPAT	\
 	_IOWR(TILOAD_IOC_MAGIC, 15, compat_uptr_t)
#define TILOAD_IOC_COMPAT_MAGIC_PA_INFO_GET	\
	_IOR(TILOAD_IOC_MAGIC, 34, struct smartpa_info)
#endif

#define SMS_HTONS(a, b)  ((((a)&0x00FF)<<8) | ((b)&0x00FF))
#define SMS_HTONL(a, b, c, d) ((((a)&0x000000FF)<<24) | \
	(((b)&0x000000FF)<<16) | (((c)&0x000000FF)<<8) | \
	((d)&0x000000FF))

/*typedefs required for the included header files */
struct BPR {
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nRegister;
};

struct Tbookpage {
	unsigned char mnBook;
	unsigned char mnPage;
};

struct Ttasdevice {
	unsigned int mnDevAddr;
	unsigned int mnErrCode;
	short mnCurrentProgram;
	short mnCurrentConfiguration;
	short mnCurrentRegConf;
	int mnIRQGPIO;
	int mnResetGpio;
	int mn_irq;
	int PowerStatus;
	bool bDSPBypass;
	bool bIrq_enabled;
	bool bLoading;
	bool bLoaderr;
	bool mbCalibrationLoaded;
	struct Tbookpage mnBkPg;
	struct TFirmware *mpCalFirmware;
};

struct Trwinfo {
	int mnDBGCmd;
	int mnCurrentReg;
	enum channel mnCurrentChannel;
	char mPage;
	char mBook;
};

struct Tsyscmd {
	bool bCmdErr;
	unsigned char mnBook;
	unsigned char mnPage;
	unsigned char mnReg;
	unsigned char mnValue;
	unsigned short bufLen;
	enum channel mnCurrentChannel;
};

enum syscmds {
	RegSettingCmd = 0,
	RegDumpCmd	= 1,
	RegCfgListCmd = 2,
	CalBinListCmd = 3,
	MaxCmd
};

struct tas_control {
	struct snd_kcontrol_new *tasdevice_profile_controls;
	int nr_controls;
};

struct tasdevice_irqinfo {
	int mn_irq_gpio;
	int mn_irq;
	struct delayed_work irq_work;
	bool mb_irq_enable;
};

struct tasdevice_priv {
	struct device *dev;
	void *client;//struct i2c_client
	struct regmap *regmap;
	struct miscdevice misc_dev;
	struct mutex dev_lock;
	struct mutex file_lock;
	struct Ttasdevice tasdevice[MaxChn];
	struct Trwinfo rwinfo;
	struct Tsyscmd nSysCmd[MaxCmd];
	struct TFirmware *mpFirmware;
	struct tasdevice_regbin mtRegbin;
	struct smartpa_gpio_info mtRstGPIOs;
	struct tasdevice_irqinfo mIrqInfo;
	struct tas_control tas_ctrl;
	int mnCurrentProgram;
	int mnCurrentConfiguration;
	unsigned int chip_id;
	int (*read)(struct tasdevice_priv *tas_dev, enum channel chn,
		unsigned int reg, unsigned int *pValue);
	int (*write)(struct tasdevice_priv *tas_dev, enum channel chn,
		unsigned int reg, unsigned int Value);
	int (*bulk_read)(struct tasdevice_priv *tas_dev, enum channel chn,
		unsigned int reg, unsigned char *pData, unsigned int len);
	int (*bulk_write)(struct tasdevice_priv *tas_dev, enum channel chn,
		unsigned int reg, unsigned char *pData, unsigned int len);
	int (*update_bits)(struct tasdevice_priv *tas_dev, enum channel chn,
		unsigned int reg, unsigned int mask, unsigned int value);
	int (*set_calibration)(void *pTAS2563, enum channel chl,
		int calibration);
	int (*fw_parse_variable_header)(struct tasdevice_priv *tas_dev,
		const struct firmware *pFW, int offset);
	int (*fw_parse_program_data)(struct TFirmware *pFirmware,
		const struct firmware *pFW, int offset);
	int (*fw_parse_configuration_data)(struct TFirmware *pFirmware,
		const struct firmware *pFW, int offset);
	int (*tasdevice_load_block)(struct tasdevice_priv *pTAS2563,
		struct TBlock *pBlock);
	int (*fw_parse_calibration_data)(struct TFirmware *pFirmware,
		const struct firmware *pFW, int offset);
	void (*irq_work_func)(struct tasdevice_priv *pcm_dev);
	int fw_state;
	unsigned int magic_num;
	int mnSPIEnable;
	int mnI2CSPIGpio;
	unsigned char ndev;
	unsigned char dev_name[32];
	unsigned char regbin_binaryname[64];
	unsigned char dsp_binaryname[64];
	unsigned char cal_binaryname[MaxChn][64];
	bool mb_runtime_suspend;
	void *codec;
	int sysclk;
	int pstream;
	int cstream;
	struct mutex codec_lock;
	struct delayed_work powercontrol_work;
	bool mbCalibrationLoaded;
};

extern const char *blocktype[5];
#ifdef CONFIG_TASDEV_CODEC_SPI
extern const struct spi_device_id tasdevice_id[];
#else
extern const struct i2c_device_id tasdevice_id[];
#endif
extern const struct dev_pm_ops tasdevice_pm_ops;
extern const struct of_device_id tasdevice_of_match[];

int tasdevice_create_controls(
	struct tasdevice_priv *tas_dev);
int tasdevice_parse_dt(struct tasdevice_priv *tas_dev);
int tasdevice_probe_next(struct tasdevice_priv *tas_dev);
void tasdevice_remove(struct tasdevice_priv *tas_dev);
void tasdevice_enable_irq(
	struct tasdevice_priv *tas_dev, bool enable);
void tas2781_irq_work_func(struct tasdevice_priv *tas_dev);

#endif /*__PCMDEVICE_H__ */
