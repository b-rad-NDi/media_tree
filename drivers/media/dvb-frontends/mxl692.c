// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the MaxLinear MxL69x family of tuners/demods
 *
 * Copyright (C) 2020 Brad Love <brad@nextdimension.cc>
 *
 * based on code:
 * Copyright (c) 2016 MaxLinear, Inc. All rights reserved
 * which was released under GPL V2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mutex.h>
#include <linux/i2c-mux.h>
#include <linux/string.h>
#include <linux/firmware.h>

#include "mxl692.h"
#include "mxl692_defs.h"

static int gIsBigEndian = -1;

static const struct dvb_frontend_ops mxl692_ops;

struct mxl692_dev {
	struct dvb_frontend fe;
	struct i2c_client *i2c_client;
	struct mutex i2c_lock;
	enum MXL_EAGLE_DEMOD_TYPE_E demodType;
	u32 current_frequency;
	int device_type;
	int seqNum;
	int init_done;
};

static int mxl692_i2c_write(struct mxl692_dev *dev, u8 *pBuffer, u16 bufferLen)
{
	int ret = 0;
	struct i2c_msg msg = {
		.addr = dev->i2c_client->addr,
		.flags = 0,
		.buf = pBuffer,
		.len = bufferLen
	};

	ret = i2c_transfer(dev->i2c_client->adapter, &msg, 1);
	if (ret != 1)
		dev_info(&dev->i2c_client->dev, "%s: i2c write error!\n", __func__);

	return ret;
}


static int mxl692_i2c_read(struct mxl692_dev *dev, u8 *pBuffer, u16 bufferLen)
{
	int ret = 0;
	struct i2c_msg msg = {
		.addr = dev->i2c_client->addr,
		.flags = I2C_M_RD,
		.buf = pBuffer,
		.len = bufferLen
	};

	ret = i2c_transfer(dev->i2c_client->adapter, &msg, 1);
	if (ret != 1)
		dev_info(&dev->i2c_client->dev, "%s: i2c read error!\n", __func__);

	return ret;
}

static void detect_endianess(void)
{
	u32 temp = 1;
	u8 *pTemp = (u8*)&temp;

	gIsBigEndian = (*pTemp == 0) ? 1 : 0;

	if (gIsBigEndian)
		pr_err("%s() BIG   endian\n", __func__);
	else
		pr_err("%s() SMALL endian\n", __func__);

}

static int convert_endian(u32 size, u8 *d)
{
	u32 i;

	for (i = 0; i < (size & ~3); i += 4) {
		d[i + 0] ^= d[i + 3];
		d[i + 3] ^= d[i + 0];
		d[i + 0] ^= d[i + 3];

		d[i + 1] ^= d[i + 2];
		d[i + 2] ^= d[i + 1];
		d[i + 1] ^= d[i + 2];
	}

	switch (size & 3) {
	case 0:
	case 1:
		/* do nothing */
		break;
	case 2:
		d[i + 0] ^= d[i + 1];
		d[i + 1] ^= d[i + 0];
		d[i + 0] ^= d[i + 1];
		break;

	case 3:
		d[i + 0] ^= d[i + 2];
		d[i + 2] ^= d[i + 0];
		d[i + 0] ^= d[i + 2];
		break;
	}
	return size;
}

static int convert_endian_n(int n, u32 size, u8 *d)
{
	int i, count = 0;
	for (i = 0; i < n; i+= size)
		count += convert_endian(size, d + i);
	return count;
}

static void mxl692_tx_swap(enum MXL_EAGLE_OPCODE_E opcode, u8 *pBuffer)
{
	if (gIsBigEndian)
		return;

	pBuffer += MXL_EAGLE_HOST_MSG_HEADER_SIZE; /* skip API header */

	switch (opcode)
	{
	case MXL_EAGLE_OPCODE_DEVICE_INTR_MASK_SET:
	case MXL_EAGLE_OPCODE_TUNER_CHANNEL_TUNE_SET:
	case MXL_EAGLE_OPCODE_SMA_TRANSMIT_SET:
		pBuffer += convert_endian(sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_PARAMS_SET:
		pBuffer += 5;
		pBuffer += convert_endian(2 * sizeof(u32), pBuffer);
		break;
	default:
		/* no swapping - all get opcodes */
		/* ATSC/OOB no swapping */
		break;
	}
}

static void mxl692_rx_swap(enum MXL_EAGLE_OPCODE_E opcode, u8 *pBuffer)
{
	if (gIsBigEndian)
		return;

	pBuffer += MXL_EAGLE_HOST_MSG_HEADER_SIZE; /* skip API header */

	switch (opcode)
	{
	case MXL_EAGLE_OPCODE_TUNER_AGC_STATUS_GET:
		pBuffer++;
		pBuffer += convert_endian(2 * sizeof(u16), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_ATSC_STATUS_GET:
		pBuffer += convert_endian_n(2, sizeof(u16), pBuffer);
		pBuffer += convert_endian(sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_ATSC_ERROR_COUNTERS_GET:
		pBuffer += convert_endian(3 * sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_ATSC_EQUALIZER_FILTER_FFE_TAPS_GET:
		pBuffer += convert_endian_n(24, sizeof(u16), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_STATUS_GET:
		pBuffer += 8;
		pBuffer += convert_endian_n(2, sizeof(u16), pBuffer);
		pBuffer += convert_endian(sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_ERROR_COUNTERS_GET:
		pBuffer += convert_endian(7 * sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_CONSTELLATION_VALUE_GET:
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_START_GET:
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_MIDDLE_GET:
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_END_GET:
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_SPUR_START_GET:
		pBuffer += convert_endian_n(24, sizeof(u16), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_SPUR_END_GET:
		pBuffer += convert_endian_n(8, sizeof(u16), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_FFE_GET:
		pBuffer += convert_endian_n(17, sizeof(u16), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_OOB_ERROR_COUNTERS_GET:
		pBuffer += convert_endian(3 * sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_OOB_STATUS_GET:
		pBuffer += convert_endian_n(2, sizeof(u16), pBuffer);
		pBuffer += convert_endian(sizeof(u32), pBuffer);
		break;
	case MXL_EAGLE_OPCODE_SMA_RECEIVE_GET:
		pBuffer += convert_endian(sizeof(u32), pBuffer);
		break;
	default:
		/* no swapping - all set opcodes */
		break;
	}
}

static u32 mxl692_checksum(u8 *pBuffer, u32 size)
{
	u32 ix, remainder = 0, currentChecksum = 0;

	for (ix=0; ix < size/4; ix++)
		currentChecksum += cpu_to_be32(*(u32*)(pBuffer +
						(ix*sizeof(u32))));
	remainder = size % 4;
	if (remainder > 0)
		currentChecksum += cpu_to_be32(*((u32*)&pBuffer[size-remainder]));

	currentChecksum ^= 0xDEADBEEF;

	return be32_to_cpu(currentChecksum);
}

static int mxl692_validate_fw_header(const u8 *buffer, u32 bufferLen)
{
	int status = 0;
	u32 ix, temp = 0;
	u32 *pLocalBuffer = NULL;

	if ((buffer[0] != 0x4D) || (buffer[1] != 0x31) ||
	    (buffer[2] != 0x10) || (buffer[3] != 0x02) ||
	    (buffer[4] != 0x40) || (buffer[5] != 0x00) ||
	    (buffer[6] != 0x00) || (buffer[7] != 0x80)) {
		status = -EINVAL;
		goto err_finish;
	}

	pLocalBuffer = (u32*)(buffer+8);
	temp = cpu_to_be32(*(u32*)pLocalBuffer);

	if ((bufferLen-16) != (temp >> 8)) {
		status = -EINVAL;
		goto err_finish;
	}

	temp = 0;
	for (ix = 16; ix < bufferLen; ix++)
		temp += buffer[ix];

	if ((u8)temp != buffer[11])
		status = -EINVAL;
err_finish:
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}

static int mxl692_write_fw_block(struct mxl692_dev *dev, const u8 *pBuffer,
				 u32 bufferLen, u32 *index)
{
	int status = 0;
	u32 ix = 0, totalLen = 0, addr = 0, chunkLen = 0, prevChunkLen = 0;
	u8 localBuf[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {}, *pLocalBuf = NULL;
	int payload_max = MXL_EAGLE_MAX_I2C_PACKET_SIZE - MXL_EAGLE_I2C_MHEADER_SIZE;
	ix = *index;

	if (pBuffer[ix] == 0x53) {
		totalLen = pBuffer[ix+1] << 16 | pBuffer[ix+2] << 8 | pBuffer[ix+3];
		totalLen = (totalLen + 3) & ~3;
		addr     = pBuffer[ix+4] << 24 | pBuffer[ix+5] << 16 |
		           pBuffer[ix+6] << 8 | pBuffer[ix+7];
		ix      += MXL_EAGLE_FW_SEGMENT_HEADER_SIZE;

		while ( (totalLen > 0) && (status == 0) ) {
			pLocalBuf = localBuf;
			chunkLen  = (totalLen < payload_max) ?
					totalLen : payload_max;

			*pLocalBuf++ = 0xFC;
			*pLocalBuf++ = chunkLen + sizeof(u32);

			*(u32*)pLocalBuf = cpu_to_le32(addr + prevChunkLen);
			pLocalBuf += sizeof(u32);

			memcpy(pLocalBuf, &pBuffer[ix], chunkLen);
			convert_endian(chunkLen, pLocalBuf);

			if (mxl692_i2c_write(dev, localBuf, (chunkLen + MXL_EAGLE_I2C_MHEADER_SIZE)) < 0) {
				status = -EREMOTEIO;
				break;
			}

			prevChunkLen += chunkLen;
			totalLen -= chunkLen;
			ix += chunkLen;
		}
		*index = ix;
	} else {
		status = -EINVAL;
	}

	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);

	return status;
}

static int mxl692_memwrite(struct mxl692_dev *dev, u32 addr,
			   u8 *pBuffer, u32 size)
{
	int status = 0, totalLen = 0;
	u8 localBuf[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {}, *pLocalBuf = NULL;

	totalLen = size;
	totalLen = (totalLen + 3) & ~3;  /* 4 byte alignment */

	if (totalLen > (MXL_EAGLE_MAX_I2C_PACKET_SIZE - MXL_EAGLE_I2C_MHEADER_SIZE))
		pr_err("%s() hrmph?\n", __func__);

	pLocalBuf = localBuf;

	*pLocalBuf++ = 0xFC;
	*pLocalBuf++ = totalLen + sizeof(u32);

	*(u32*)pLocalBuf = addr;
	pLocalBuf += sizeof(u32);

	memcpy(pLocalBuf, pBuffer, totalLen);
	if (gIsBigEndian)
		convert_endian(sizeof(u32) + totalLen, localBuf + 2);

	if (mxl692_i2c_write(dev, localBuf,
	    (totalLen + MXL_EAGLE_I2C_MHEADER_SIZE)) < 0) {
		status = -EREMOTEIO;
		goto err_finish;
	}

	return status;
err_finish:
	pr_err("%s() FAIL\n", __func__);
	return status;
}

static int mxl692_memread(struct mxl692_dev *dev, u32 addr,
			  u8 *pBuffer, u32 size)
{
	int status = 0;
	u8 localBuf[MXL_EAGLE_I2C_MHEADER_SIZE] = {}, *pLocalBuf = NULL;

	pLocalBuf = localBuf;

	*pLocalBuf++ = 0xFB;
	*pLocalBuf++ = sizeof(u32);
	*(u32*)pLocalBuf = addr;

	if (gIsBigEndian)
		convert_endian(sizeof(u32), pLocalBuf);

	if (mxl692_i2c_write(dev, localBuf, MXL_EAGLE_I2C_MHEADER_SIZE) > 0) {
		size = (size + 3) & ~3;  /* 4 byte alignment */
		status = mxl692_i2c_read(dev, pBuffer, (u16)size) < 0 ?
		                         -EREMOTEIO : 0;

		if (status == 0 && gIsBigEndian)
			convert_endian(size, pBuffer);
	} else
		status = -EREMOTEIO;

	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);

	return status;
}

static int mxl692_opwrite(struct mxl692_dev *dev, u8 *pBuffer,
			  u32 size)
{
	int status = 0, totalLen = 0;
	u8 localBuf[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {}, *pLocalBuf = NULL;

	totalLen = size;
	totalLen = (totalLen + 3) & ~3;  /* 4 byte alignment */

	if (totalLen > (MXL_EAGLE_MAX_I2C_PACKET_SIZE - MXL_EAGLE_I2C_PHEADER_SIZE))
		pr_err("%s() hrmph?\n", __func__);

	pLocalBuf = localBuf;

	*pLocalBuf++ = 0xFE;
	*pLocalBuf++ = (u8)totalLen;

	memcpy(pLocalBuf, pBuffer, totalLen);
	convert_endian(totalLen, pLocalBuf);

	if (mxl692_i2c_write(dev, localBuf, (totalLen + MXL_EAGLE_I2C_PHEADER_SIZE)) < 0) {
		status = -EREMOTEIO;
		goto err_finish;
	}
err_finish:
	if (status != 0)
		pr_err("%s() FAIL\n", __func__);
	return status;
}

static int mxl692_opread(struct mxl692_dev *dev, u8 *pBuffer,
			 u32 size)
{
	int status = 0;
	u32 ix = 0;
	u8 localBuf[MXL_EAGLE_I2C_PHEADER_SIZE] = {};

	localBuf[0] = 0xFD;
	localBuf[1] = 0;

	if (mxl692_i2c_write(dev, localBuf, MXL_EAGLE_I2C_PHEADER_SIZE) > 0) {
		size = (size + 3) & ~3;  /* 4 byte alignment */

		//read in 4 byte chunks
		for (ix=0; ix<size; ix+=4) {
			if (mxl692_i2c_read(dev, pBuffer+ix, 4) < 0) {
				pr_err("%s() Line %d   ix=%d   size=%d\n", __func__, __LINE__, ix, size);
				status = -EREMOTEIO;
				goto err_finish;
			}
		}
		convert_endian(size, pBuffer);
	} else
		status = -EREMOTEIO;
err_finish:
	if (status != 0)
		pr_err("%s() FAIL\n", __func__);
	return status;
}

static int mxl692_i2c_writeread(struct mxl692_dev *dev,
				u8 opcode,
				u8 *pTxPayload,
				u8 txPayloadSize,
				u8 *pRxPayload,
				u8 rxExpectedPayloadSize)
{
	int status = 0, timeout = 40;
	u8 txBuffer[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {};
	u8 rxBuffer[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {};
	u32 responseChecksum = 0, calculatedResponseChecksum = 0;
	struct MXL_EAGLE_HOST_MSG_HEADER_T *txMsgHeader;
	struct MXL_EAGLE_HOST_MSG_HEADER_T *pRxMsgHeader;

	mutex_lock(&dev->i2c_lock);

	if ((txPayloadSize + MXL_EAGLE_HOST_MSG_HEADER_SIZE) >
	    (MXL_EAGLE_MAX_I2C_PACKET_SIZE - MXL_EAGLE_I2C_PHEADER_SIZE)) {
		status = -EINVAL;
		goto err_finish;
	}

	txMsgHeader = (struct MXL_EAGLE_HOST_MSG_HEADER_T *)txBuffer;
	txMsgHeader->checksum = 0;
	txMsgHeader->opcode = opcode;
	txMsgHeader->payloadSize = txPayloadSize;
	txMsgHeader->seqNum = dev->seqNum++;

	if (dev->seqNum == 0)
		dev->seqNum = 1;

	if ((pTxPayload != NULL) && (txPayloadSize > 0))
		memcpy(&txBuffer[MXL_EAGLE_HOST_MSG_HEADER_SIZE], pTxPayload, txPayloadSize);

	mxl692_tx_swap(opcode, txBuffer);

	txMsgHeader->checksum = 0;
	txMsgHeader->checksum = mxl692_checksum(txBuffer, MXL_EAGLE_HOST_MSG_HEADER_SIZE + txPayloadSize);

	/* send Tx message */
	status = mxl692_opwrite(dev, txBuffer, txPayloadSize + MXL_EAGLE_HOST_MSG_HEADER_SIZE);
	if (status != 0) {
		status = -EREMOTEIO;
		goto err_finish;
	}

	/* receive Rx message (polling) */
	pRxMsgHeader = (struct MXL_EAGLE_HOST_MSG_HEADER_T*)rxBuffer;

	do {
		status = mxl692_opread(dev, rxBuffer, rxExpectedPayloadSize + MXL_EAGLE_HOST_MSG_HEADER_SIZE);
		usleep_range(1000,2000);
		timeout--;
	} while ((timeout > 0) && (status == 0) &&
	         (pRxMsgHeader->seqNum == 0) &&
	         (pRxMsgHeader->checksum == 0));

	if ( (timeout == 0) || (status != 0) ) {
		pr_err("%s() FAIL Line %d   timeout=%d   status=%d\n", __func__, __LINE__, timeout, status);
		status = -ETIMEDOUT;
		goto err_finish;
	}

	if (pRxMsgHeader->status != 0) {
		status = (int)pRxMsgHeader->status;
		goto err_finish;
	}

	if ((pRxMsgHeader->seqNum != txMsgHeader->seqNum) ||
	    (pRxMsgHeader->opcode != txMsgHeader->opcode) ||
	    (pRxMsgHeader->payloadSize != rxExpectedPayloadSize)) {
		pr_debug("%s() Something failed seq=%s  opcode=%s  pSize=%s\n",
		       __func__,
		       pRxMsgHeader->seqNum != txMsgHeader->seqNum ? "X" : "0",
		       pRxMsgHeader->opcode != txMsgHeader->opcode ? "X" : "0",
		       pRxMsgHeader->payloadSize != rxExpectedPayloadSize ? "X" : "0");
		if (pRxMsgHeader->payloadSize != rxExpectedPayloadSize)
			pr_err("%s() pRxMsgHeader->payloadSize=%d   rxExpectedPayloadSize=%d\n",
			       __func__, pRxMsgHeader->payloadSize, rxExpectedPayloadSize);
		status = -EREMOTEIO;
		goto err_finish;
	}

	responseChecksum = pRxMsgHeader->checksum;
	pRxMsgHeader->checksum = 0;
	calculatedResponseChecksum = mxl692_checksum(rxBuffer,
				MXL_EAGLE_HOST_MSG_HEADER_SIZE + pRxMsgHeader->payloadSize);

	if (responseChecksum != calculatedResponseChecksum) {
		status = -EREMOTEIO;
		goto err_finish;
	}

	mxl692_rx_swap(pRxMsgHeader->opcode, rxBuffer);

	if (pRxMsgHeader->payloadSize > 0) {
		if (pRxPayload == NULL) {
			status = -EREMOTEIO;
			goto err_finish;
		}
		memcpy(pRxPayload, rxBuffer + MXL_EAGLE_HOST_MSG_HEADER_SIZE, pRxMsgHeader->payloadSize);
	}
err_finish:
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);

	mutex_unlock(&dev->i2c_lock);
	return status;
}

static int mxl692_fwdownload(struct mxl692_dev *dev,
			     const u8 *pFwBuffer, u32 bufferLen)
{
	int status = 0;
	u32 ix, regValue = 0x1;
	u8 rxBuffer[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {};
	struct MXL_EAGLE_DEV_STATUS_T *deviceStatus;

	if (bufferLen < MXL_EAGLE_FW_HEADER_SIZE ||
	    bufferLen > MXL_EAGLE_FW_MAX_SIZE_IN_KB * 1000)
		return -EINVAL;

	mutex_lock(&dev->i2c_lock);

	pr_err("%s()\n", __func__);

	status = mxl692_validate_fw_header(pFwBuffer, bufferLen);
	if (status != 0)
		goto err_finish;

	ix = 16;
	status = mxl692_write_fw_block(dev, pFwBuffer, bufferLen, &ix); /* DRAM */
	if (status != 0)
		goto err_finish;

	status = mxl692_write_fw_block(dev, pFwBuffer, bufferLen, &ix); /* IRAM */
	if (status != 0)
		goto err_finish;

	/* release CPU from reset */
	status = mxl692_memwrite(dev, 0x70000018, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	mutex_unlock(&dev->i2c_lock);

	if (status == 0) {
		/* verify FW is alive */
		usleep_range(MXL_EAGLE_FW_LOAD_TIME * 1000, (MXL_EAGLE_FW_LOAD_TIME + 5) * 1000);
		deviceStatus = (struct MXL_EAGLE_DEV_STATUS_T*)&rxBuffer;
		status = mxl692_i2c_writeread(dev,
                                              MXL_EAGLE_OPCODE_DEVICE_STATUS_GET,
                                              NULL,
                                              0,
                                              (u8*)deviceStatus,
                                              sizeof(struct MXL_EAGLE_DEV_STATUS_T));
	}

	return status;
err_finish:
	mutex_unlock(&dev->i2c_lock);
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}

static int mxl692_get_versions(struct mxl692_dev *dev)
{
	int status   = 0;
	struct MXL_EAGLE_DEV_VER_T devVersionStruct = {};
	char *chipId[] = {"N/A", "691", "248", "692"};

	status = mxl692_i2c_writeread(dev, MXL_EAGLE_OPCODE_DEVICE_VERSION_GET,
				      NULL,
				      0,
				      (u8*)&devVersionStruct,
				      sizeof(struct MXL_EAGLE_DEV_VER_T));
	if (status != 0)
		return status;

	pr_err("MxL692_DEMOD Chip ID: %s \n", chipId[devVersionStruct.chipId]);

	pr_err("MxL692_DEMOD FW Version: %d.%d.%d.%d_RC%d \n",
	       devVersionStruct.firmwareVer[0],
	       devVersionStruct.firmwareVer[1],
	       devVersionStruct.firmwareVer[2],
	       devVersionStruct.firmwareVer[3],
	       devVersionStruct.firmwareVer[4]);

	return status;
}

static int mxl692_reset(struct mxl692_dev *dev)
{
	int status = 0;
	u32 deviceType = MXL_EAGLE_DEVICE_MAX, regValue = 0x2;

	pr_err("%s()\n", __func__);

	/* legacy i2c override */
	status = mxl692_memwrite(dev, 0x80000100, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	/* verify sku */
	status = mxl692_memread(dev, 0x70000188, (u8*)&deviceType, sizeof(u32));
	if (status != 0)
		goto err_finish;

	if (deviceType != dev->device_type)
		goto err_finish;

err_finish:
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}

static int mxl692_config_regulators(struct mxl692_dev *dev,
				    enum MXL_EAGLE_POWER_SUPPLY_SOURCE_E powerSupply)
{
	int status = 0;
	u32 regValue;

	pr_err("%s()\n", __func__);

	/* configure main regulator according to the power supply source */
	status = mxl692_memread(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue &= 0x00FFFFFF;
	regValue |= (powerSupply == MXL_EAGLE_POWER_SUPPLY_SOURCE_SINGLE) ?
					0x14000000 : 0x10000000;

	status = mxl692_memwrite(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	/* configure digital regulator to high current mode */
	status = mxl692_memread(dev, 0x90000018, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue |= 0x800;

	status = mxl692_memwrite(dev, 0x90000018, (u8*)&regValue, sizeof(u32));

err_finish:
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}

static int mxl692_config_xtal(struct mxl692_dev *dev,
			      struct MXL_EAGLE_DEV_XTAL_T *pDevXtalStruct)
{
	int status = 0;
	u32 regValue, regValue1;

	pr_err("%s()\n", __func__);

	status = mxl692_memread(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	/* set XTAL capacitance */
	regValue &= 0xFFFFFFE0;
	regValue |= pDevXtalStruct->xtalCap;

	/* set CLK OUT */
	regValue = pDevXtalStruct->clkOutEnable ?
				(regValue | 0x0100) : (regValue & 0xFFFFFEFF);

	status = mxl692_memwrite(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	/* set CLK OUT divider */
	regValue = pDevXtalStruct->clkOutDivEnable ?
				(regValue | 0x0200) : (regValue & 0xFFFFFDFF);

	status = mxl692_memwrite(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	/* set XTAL sharing */
	regValue = pDevXtalStruct->xtalSharingEnable ?
				(regValue | 0x010400) : (regValue & 0xFFFEFBFF);

	status = mxl692_memwrite(dev, 0x90000000, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;


	/* enable/disable XTAL calibration, based on master/slave device */
	status = mxl692_memread(dev, 0x90000030, (u8*)&regValue1, sizeof(u32));
	if (status != 0)
		goto err_finish;

	if (pDevXtalStruct->xtalCalibrationEnable)
	{
		/* enable XTAL calibration and set XTAL amplitude to a higher value */
		regValue1 &= 0xFFFFFFFD;
		regValue1 |= 0x30;

		status = mxl692_memwrite(dev, 0x90000030, (u8*)&regValue1, sizeof(u32));
		if (status != 0)
			goto err_finish;
	} else {
		/* disable XTAL calibration */
		regValue1 |= 0x2;

		status = mxl692_memwrite(dev, 0x90000030, (u8*)&regValue1, sizeof(u32));
		if (status != 0)
			goto err_finish;

		/* set XTAL bias value */
		status = mxl692_memread(dev, 0x9000002c, (u8*)&regValue, sizeof(u32));
		if (status != 0)
			goto err_finish;

		regValue &= 0xC0FFFFFF;
		regValue |= 0xA000000;

		status = mxl692_memwrite(dev, 0x9000002c, (u8*)&regValue, sizeof(u32));
		if (status != 0)
			goto err_finish;
	}

	/* start XTAL calibration */
	status = mxl692_memread(dev, 0x70000010, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue |= 0x8;

	status = mxl692_memwrite(dev, 0x70000010, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	status = mxl692_memread(dev, 0x70000018, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue |= 0x10;

	status = mxl692_memwrite(dev, 0x70000018, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	status = mxl692_memread(dev, 0x9001014c, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue &= 0xFFFFEFFF;

	status = mxl692_memwrite(dev, 0x9001014c, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	regValue |= 0x1000;

	status = mxl692_memwrite(dev, 0x9001014c, (u8*)&regValue, sizeof(u32));
	if (status != 0)
		goto err_finish;

	usleep_range(45000,55000);

err_finish:
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}


static int mxl692_powermode(struct mxl692_dev *dev,
			    enum MXL_EAGLE_POWER_MODE_E powerMode)
{
	int status = 0;

	pr_err("%s() %s\n", __func__, powerMode == MXL_EAGLE_POWER_MODE_SLEEP ?
	       "sleep" : "active");

	status = mxl692_i2c_writeread(dev,
				      MXL_EAGLE_OPCODE_DEVICE_POWERMODE_SET,
				      (u8*)&powerMode,
				      sizeof(u8),
				      NULL,
				      0);
	if (status != 0)
		pr_err("%s() FAIL!\n", __func__);
	return status;
}

static int mxl692_init(struct dvb_frontend *fe)
{
	struct mxl692_dev *dev = fe->demodulator_priv;
	struct i2c_client *client = dev->i2c_client;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	int status = 0;
	const struct firmware *firmware;
	struct MXL_EAGLE_DEV_XTAL_T devXtalConfig = {};

	if(dev->init_done)
		goto warm;

	dev->seqNum = 1;

	status = mxl692_reset(dev);
	if(status != 0)
		goto err;

	usleep_range(100 * 1000, 110 * 1000); /* was 1000! */

	status = mxl692_config_regulators(dev, MXL_EAGLE_POWER_SUPPLY_SOURCE_DUAL);
	if(status != 0)
		goto err;

	devXtalConfig.xtalCap = 26;
	devXtalConfig.clkOutDivEnable = 0;
	devXtalConfig.clkOutEnable = 0;
	devXtalConfig.xtalCalibrationEnable = 0;
	devXtalConfig.xtalSharingEnable = 1;
	status = mxl692_config_xtal(dev, &devXtalConfig);
	if(status != 0)
		goto err;

	status = request_firmware(&firmware, MXL692_FIRMWARE, &client->dev);
	if (status) {
		pr_err("%s() firmware missing? %s\n", __func__, MXL692_FIRMWARE);
		goto err;
	}

	status = mxl692_fwdownload(dev, firmware->data, firmware->size);
	if(status != 0) {
		goto err_release_firmware;
	}

	release_firmware(firmware);

	usleep_range(500 * 1000, 510 * 1000); /* was 1000! */
	status = mxl692_get_versions(dev);
	if(status != 0)
		goto err;
warm:
        /* Init stats here to indicate which stats are supported */
        c->cnr.len = 1;
        c->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
        c->post_bit_error.len = 1;
        c->post_bit_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
        c->post_bit_count.len = 1;
        c->post_bit_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
        c->block_error.len = 1;
        c->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	dev->init_done = 1;
	return 0;
err_release_firmware:
	release_firmware(firmware);
err:
	return status;
}

static int mxl692_sleep(struct dvb_frontend *fe)
{
	struct mxl692_dev *dev = fe->demodulator_priv;

	mxl692_powermode(dev, MXL_EAGLE_POWER_MODE_SLEEP);
	return 0;
}

static int mxl692_set_frontend(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct mxl692_dev *dev = fe->demodulator_priv;

	int status = 0;
	enum MXL_EAGLE_DEMOD_TYPE_E demodType;
	enum MXL_EAGLE_POWER_MODE_E powerMode = MXL_EAGLE_POWER_MODE_ACTIVE;
	struct MXL_EAGLE_MPEGOUT_PARAMS_T mpegOutParams = {};
	enum MXL_EAGLE_QAM_DEMOD_ANNEX_TYPE_E qamAnnexType = MXL_EAGLE_QAM_DEMOD_ANNEX_A;
//	struct MXL_EAGLE_QAM_DEMOD_PARAMS_T qamParamsStruct = {};
	struct MXL_EAGLE_TUNER_CHANNEL_PARAMS_T tunerChannelParams = {};

	switch (p->modulation) {
	case VSB_8:
		demodType = MXL_EAGLE_DEMOD_TYPE_ATSC;
		break;
	case QAM_AUTO:
	case QAM_64:
	case QAM_128:
	case QAM_256:
		demodType = MXL_EAGLE_DEMOD_TYPE_QAM;
		break;
	default:
		return -EINVAL;
	}

	if(dev->current_frequency == p->frequency && dev->demodType == demodType)
		return 0;

	dev->current_frequency = -1;
	dev->demodType = -1;

	status = mxl692_i2c_writeread(dev,
				      MXL_EAGLE_OPCODE_DEVICE_DEMODULATOR_TYPE_SET,
				      (u8*)&demodType,
				      sizeof(u8),
				      NULL,
				      0);
	if(status != 0)
	    pr_err("DEVICE_DEMODULATOR_TYPE_SET...FAIL  Status:0x%x\n", status);

	usleep_range(200 * 1000, 210 * 1000); /* was 500! */

	//Config Device Power Mode
	status = mxl692_powermode(dev, powerMode);
	if(status != 0)
		goto err;

	usleep_range(200 * 1000, 210 * 1000); /* was 500! */

	mpegOutParams.mpegIsParallel = 0;
	mpegOutParams.lsbOrMsbFirst = MXL_EAGLE_DATA_SERIAL_MSB_1ST; // MXL_EAGLE_DATA_SERIAL_LSB_1ST
	mpegOutParams.mpegSyncPulseWidth = MXL_EAGLE_DATA_SYNC_WIDTH_BIT; // MXL_EAGLE_DATA_SYNC_WIDTH_BYTE
	mpegOutParams.mpegValidPol = MXL_EAGLE_CLOCK_POSITIVE; // MXL_EAGLE_CLOCK_NEGATIVE
	mpegOutParams.mpegSyncPol = MXL_EAGLE_CLOCK_POSITIVE;
	mpegOutParams.mpegClkPol = MXL_EAGLE_CLOCK_NEGATIVE;
	mpegOutParams.mpeg3WireModeEnable = 0;
	mpegOutParams.mpegClkFreq = MXL_EAGLE_MPEG_CLOCK_27MHz; // MXL_EAGLE_MPEG_CLOCK_54MHz, MXL_EAGLE_MPEG_CLOCK_40_5MHz, MXL_EAGLE_MPEG_CLOCK_27MHz
	//mpegOutParams.mpegPadDrv = 1;
	switch(demodType)
	{
	case MXL_EAGLE_DEMOD_TYPE_ATSC:
		status = mxl692_i2c_writeread(dev,
					      MXL_EAGLE_OPCODE_DEVICE_MPEG_OUT_PARAMS_SET,
					      (u8*)&mpegOutParams,
					      sizeof(struct MXL_EAGLE_MPEGOUT_PARAMS_T),
					      NULL,
					      0);
		if(status != 0)
			goto err;
		break;
	case MXL_EAGLE_DEMOD_TYPE_QAM:
		if(MXL_EAGLE_QAM_DEMOD_ANNEX_A == qamAnnexType)
			mpegOutParams.lsbOrMsbFirst = MXL_EAGLE_DATA_SERIAL_LSB_1ST;
		status = mxl692_i2c_writeread(dev,
					      MXL_EAGLE_OPCODE_DEVICE_MPEG_OUT_PARAMS_SET,
					      (u8*)&mpegOutParams,
					      sizeof(struct MXL_EAGLE_MPEGOUT_PARAMS_T),
					      NULL,
					      0);
		if(status != 0)
			goto err;
		break;
	default:
		break;
	}

	usleep_range(200 * 1000, 210 * 1000); /* was 500! */

	tunerChannelParams.freqInHz = p->frequency;
	tunerChannelParams.bandWidth = MXL_EAGLE_TUNER_BW_6MHz;
	tunerChannelParams.tuneMode = MXL_EAGLE_TUNER_CHANNEL_TUNE_MODE_VIEW;

	pr_err(" Tuning Freq: %d\n", tunerChannelParams.freqInHz);

	status = mxl692_i2c_writeread(dev,
				      MXL_EAGLE_OPCODE_TUNER_CHANNEL_TUNE_SET,
				      (u8*)&tunerChannelParams,
				      sizeof(struct MXL_EAGLE_TUNER_CHANNEL_PARAMS_T),
				      NULL,
				      0);
	if(status != 0)
		goto err;

	usleep_range(200 * 1000, 210 * 1000); /* was 500! */

	switch(demodType)
	{
	case MXL_EAGLE_DEMOD_TYPE_ATSC:
		status = mxl692_i2c_writeread(dev,
					      MXL_EAGLE_OPCODE_ATSC_INIT_SET,
					      NULL, 0, NULL, 0);
		if(status != 0)
			goto err;
		break;
	default:
		break;
	}

	dev->demodType = demodType;
	dev->current_frequency = p->frequency;
err:
	return 0;
}

static int mxl692_read_status(struct dvb_frontend *fe,
				 enum fe_status *status)
{
	struct mxl692_dev *dev = fe->demodulator_priv;
	u8 rxBuffer[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {};
	struct MXL_EAGLE_ATSC_DEMOD_STATUS_T *atscStatusStruct;
//	struct MXL_EAGLE_QAM_DEMOD_STATUS_T *qamStatusStruct;
	int mxl_status = 0;
	enum MXL_EAGLE_DEMOD_TYPE_E demodType = dev->demodType;
	int ret = 0;
	*status = 0;

	pr_debug("%s()\n", __func__);
	atscStatusStruct = (struct MXL_EAGLE_ATSC_DEMOD_STATUS_T*)&rxBuffer;

	switch(demodType)
	{
	case MXL_EAGLE_DEMOD_TYPE_ATSC:
		mxl_status = mxl692_i2c_writeread(dev,
						  MXL_EAGLE_OPCODE_ATSC_STATUS_GET,
						  NULL,
						  0,
						  (u8*)atscStatusStruct,
						  sizeof(struct MXL_EAGLE_ATSC_DEMOD_STATUS_T));
		if(!mxl_status && atscStatusStruct->isAtscLock) {
			*status |= FE_HAS_SIGNAL;
			*status |= FE_HAS_CARRIER;
			*status |= FE_HAS_VITERBI;
			*status |= FE_HAS_SYNC;
			*status |= FE_HAS_LOCK;
		}
		break;
	case MXL_EAGLE_DEMOD_TYPE_QAM:
	case MXL_EAGLE_DEMOD_TYPE_OOB:
	default:
		break;
	}

	return ret;
}

static int mxl692_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct mxl692_dev *dev = fe->demodulator_priv;
	u8 rxBuffer[MXL_EAGLE_MAX_I2C_PACKET_SIZE] = {};
	struct MXL_EAGLE_ATSC_DEMOD_STATUS_T *atscStatusStruct;
//	struct MXL_EAGLE_QAM_DEMOD_STATUS_T *qamStatusStruct = &rxBuffer;
	int mxl_status = 0;
	enum MXL_EAGLE_DEMOD_TYPE_E demodType = dev->demodType;

	pr_err("%s()\n", __func__);
	atscStatusStruct = (struct MXL_EAGLE_ATSC_DEMOD_STATUS_T*)&rxBuffer;

	switch(demodType)
	{
	case MXL_EAGLE_DEMOD_TYPE_ATSC:
		mxl_status = mxl692_i2c_writeread(dev,
						  MXL_EAGLE_OPCODE_ATSC_STATUS_GET,
						  NULL,
						  0,
						  (u8*)atscStatusStruct,
						  sizeof(struct MXL_EAGLE_ATSC_DEMOD_STATUS_T));
		if(!mxl_status)
			*snr = (u16)(atscStatusStruct->snrDbTenths / 10);
		break;
	case MXL_EAGLE_DEMOD_TYPE_QAM:
	case MXL_EAGLE_DEMOD_TYPE_OOB:
	default:
		break;
	}
	return 0;
}

static const struct dvb_frontend_ops mxl692_ops = {
	.delsys = { SYS_ATSC },
	.info = {
		.name = "MaxLinear mxl692 VSB Frontend",
		.frequency_min_hz      = 54000000,
		.frequency_max_hz      = 858000000,
		.frequency_stepsize_hz = 62500,
		.caps = FE_CAN_8VSB
	},

	.init = mxl692_init,
	.sleep = mxl692_sleep,
	.set_frontend = mxl692_set_frontend,
	.read_status = mxl692_read_status,
	.read_snr = mxl692_read_snr,
};

static int mxl692_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mxl692_config *config = client->dev.platform_data;
	struct mxl692_dev *dev;
	int ret = 0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		dev_err(&client->dev, "kzalloc() failed\n");
		goto err;
	}

	memcpy(&dev->fe.ops, &mxl692_ops, sizeof(struct dvb_frontend_ops));
	dev->fe.demodulator_priv = dev;
	dev->i2c_client = client;
	*config->fe = &dev->fe;
	mutex_init(&dev->i2c_lock);
	i2c_set_clientdata(client, dev);

	dev_info(&client->dev, "MaxLinear mxl692 successfully attached\n");
	detect_endianess();

	return 0;
err:
	dev_dbg(&client->dev, "mxl692_probe failed\n");
	return -ENODEV;
}

static int mxl692_remove(struct i2c_client *client)
{
	struct mxl692_dev *dev = i2c_get_clientdata(client);

	dev->fe.demodulator_priv = NULL;
	i2c_set_clientdata(client, NULL);
	kfree(dev);

	return 0;
}

static const struct i2c_device_id mxl692_id_table[] = {
	{"mxl692", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mxl692_id_table);

static struct i2c_driver mxl692_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mxl692",
	},
	.probe		= mxl692_probe,
	.remove		= mxl692_remove,
	.id_table	= mxl692_id_table,
};

module_i2c_driver(mxl692_driver);

MODULE_AUTHOR("Brad Love <brad@nextdimension.cc");
MODULE_DESCRIPTION("MaxLinear mxl692 demodulator/tuner driver");
MODULE_FIRMWARE(MXL692_FIRMWARE);
MODULE_LICENSE("GPL");
