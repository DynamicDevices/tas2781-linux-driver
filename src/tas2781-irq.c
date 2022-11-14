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
			
#include <linux/firmware.h>
#include <sound/soc.h>

#include "tasdevice.h"
#include "tas2781-reg.h"
#include "tasdevice-rw.h"

void tas2781_irq_work_func(struct tasdevice_priv *tas_dev)
{
	int rc = 0;
	unsigned int reg_val = 0, array_size = 0, i = 0, ndev = 0;
	unsigned int int_reg_array[] = {
		TAS2781_REG_INT_LTCH0,
		TAS2781_REG_INT_LTCH1,
		TAS2781_REG_INT_LTCH1_0,
		TAS2781_REG_INT_LTCH2,
		TAS2781_REG_INT_LTCH3,
		TAS2781_REG_INT_LTCH4};

	tasdevice_enable_irq(tas_dev, false);

	array_size = ARRAY_SIZE(int_reg_array);

	for (ndev = 0; ndev < tas_dev->ndev; ndev++) {
		for (i = 0; i < array_size; i++) {
			rc = tasdevice_dev_read(tas_dev,
				ndev, int_reg_array[i], &reg_val);
			if (!rc) {
				dev_info(tas_dev->dev,
					"INT STATUS REG 0x%04x=0x%02x\n",
					int_reg_array[i], reg_val);
			} else {
				dev_err(tas_dev->dev,
					"Read Reg 0x%04x error(rc=%d)\n",
					int_reg_array[i], rc);
			}
		}
	}

}
