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

#define __PACK_PREFIX__ __attribute__((packed))

/*****************************************************************************************
    Defines
 ****************************************************************************************/
#define MXL_EAGLE_HOST_MSG_HEADER_SIZE  8
#define MXL_EAGLE_FW_MAX_SIZE_IN_KB     76
#define MXL_EAGLE_QAM_FFE_TAPS_LENGTH   16
#define MXL_EAGLE_QAM_SPUR_TAPS_LENGTH  32
#define MXL_EAGLE_QAM_DFE_TAPS_LENGTH   72
#define MXL_EAGLE_ATSC_FFE_TAPS_LENGTH  4096
#define MXL_EAGLE_ATSC_DFE_TAPS_LENGTH  384
#define MXL_EAGLE_VERSION_SIZE          5     //A.B.C.D-RCx
#define MXL_EAGLE_FW_LOAD_TIME          50

#define MXL_EAGLE_FW_MAX_SIZE_IN_KB       76
#define MXL_EAGLE_FW_HEADER_SIZE          16
#define MXL_EAGLE_FW_SEGMENT_HEADER_SIZE  8
#define MXL_EAGLE_MAX_I2C_PACKET_SIZE     58  //calculated as: ((MIN(max i2c driver buffer, 255) - 6) & ~3) + 6.  currently set according to USB-ISS driver
#define MXL_EAGLE_I2C_MHEADER_SIZE        6
#define MXL_EAGLE_I2C_PHEADER_SIZE        2

/*! Enum of Eagle family devices */
enum MXL_EAGLE_DEVICE_E
{
  MXL_EAGLE_DEVICE_691 = 1,                             //!< Device Mxl691
  MXL_EAGLE_DEVICE_248 = 2,                             //!< Device Mxl248
  MXL_EAGLE_DEVICE_692 = 3,                             //!< Device Mxl692
  MXL_EAGLE_DEVICE_MAX,                                 //!< No such device
};

#define VER_A   1
#define VER_B   1
#define VER_C   1
#define VER_D   3
#define VER_E  	6

/*! Enum of Host to Eagle I2C protocol opcodes */
enum MXL_EAGLE_OPCODE_E
{
  //DEVICE
  MXL_EAGLE_OPCODE_DEVICE_DEMODULATOR_TYPE_SET,
  MXL_EAGLE_OPCODE_DEVICE_MPEG_OUT_PARAMS_SET,
  MXL_EAGLE_OPCODE_DEVICE_POWERMODE_SET,
  MXL_EAGLE_OPCODE_DEVICE_GPIO_DIRECTION_SET,
  MXL_EAGLE_OPCODE_DEVICE_GPO_LEVEL_SET,
  MXL_EAGLE_OPCODE_DEVICE_INTR_MASK_SET,
  MXL_EAGLE_OPCODE_DEVICE_IO_MUX_SET,
  MXL_EAGLE_OPCODE_DEVICE_VERSION_GET,
  MXL_EAGLE_OPCODE_DEVICE_STATUS_GET,
  MXL_EAGLE_OPCODE_DEVICE_GPI_LEVEL_GET,

  //TUNER
  MXL_EAGLE_OPCODE_TUNER_CHANNEL_TUNE_SET,
  MXL_EAGLE_OPCODE_TUNER_LOCK_STATUS_GET,
  MXL_EAGLE_OPCODE_TUNER_AGC_STATUS_GET,

  //ATSC
  MXL_EAGLE_OPCODE_ATSC_INIT_SET,
  MXL_EAGLE_OPCODE_ATSC_ACQUIRE_CARRIER_SET,
  MXL_EAGLE_OPCODE_ATSC_STATUS_GET,
  MXL_EAGLE_OPCODE_ATSC_ERROR_COUNTERS_GET,
  MXL_EAGLE_OPCODE_ATSC_EQUALIZER_FILTER_DFE_TAPS_GET,
  MXL_EAGLE_OPCODE_ATSC_EQUALIZER_FILTER_FFE_TAPS_GET,

  //QAM
  MXL_EAGLE_OPCODE_QAM_PARAMS_SET,
  MXL_EAGLE_OPCODE_QAM_RESTART_SET,
  MXL_EAGLE_OPCODE_QAM_STATUS_GET,
  MXL_EAGLE_OPCODE_QAM_ERROR_COUNTERS_GET,
  MXL_EAGLE_OPCODE_QAM_CONSTELLATION_VALUE_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_FFE_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_SPUR_START_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_SPUR_END_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_TAPS_NUMBER_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_START_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_MIDDLE_GET,
  MXL_EAGLE_OPCODE_QAM_EQUALIZER_FILTER_DFE_END_GET,

  //OOB
  MXL_EAGLE_OPCODE_OOB_PARAMS_SET,
  MXL_EAGLE_OPCODE_OOB_RESTART_SET,
  MXL_EAGLE_OPCODE_OOB_ERROR_COUNTERS_GET,
  MXL_EAGLE_OPCODE_OOB_STATUS_GET,

  //SMA
  MXL_EAGLE_OPCODE_SMA_INIT_SET,
  MXL_EAGLE_OPCODE_SMA_PARAMS_SET,
  MXL_EAGLE_OPCODE_SMA_TRANSMIT_SET,
  MXL_EAGLE_OPCODE_SMA_RECEIVE_GET,

  //DEBUG
  MXL_EAGLE_OPCODE_INTERNAL,

  MXL_EAGLE_OPCODE_MAX = 70,
};

/*! Enum of Callabck function types */
enum MXL_EAGLE_CB_TYPE_E
{
  MXL_EAGLE_CB_FW_DOWNLOAD = 0,                         //!< Callback called during FW download
};

/*! Enum of power supply types */
enum MXL_EAGLE_POWER_SUPPLY_SOURCE_E
{
  MXL_EAGLE_POWER_SUPPLY_SOURCE_SINGLE,                 //!< Single supply of 3.3V
  MXL_EAGLE_POWER_SUPPLY_SOURCE_DUAL,                   //!< Dual supply of 1.8V & 3.3V
};

/*! Enum of I/O pad drive modes */
enum MXL_EAGLE_IO_MUX_DRIVE_MODE_E
{
  MXL_EAGLE_IO_MUX_DRIVE_MODE_1X,                       //!< I/O Mux drive 1X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_2X,                       //!< I/O Mux drive 2X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_3X,                       //!< I/O Mux drive 3X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_4X,                       //!< I/O Mux drive 4X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_5X,                       //!< I/O Mux drive 5X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_6X,                       //!< I/O Mux drive 6X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_7X,                       //!< I/O Mux drive 7X
  MXL_EAGLE_IO_MUX_DRIVE_MODE_8X,                       //!< I/O Mux drive 8X
};

/*! Enum of demodulator types. Used for selection of demodulator type in relevant devices, e.g. ATSC vs. QAM in Mxl691 */
enum MXL_EAGLE_DEMOD_TYPE_E
{
  MXL_EAGLE_DEMOD_TYPE_QAM,                             //!< Demodulator type QAM (Mxl248 or Mxl692)
  MXL_EAGLE_DEMOD_TYPE_OOB,                             //!< Demodulator type OOB (Mxl248 only)
  MXL_EAGLE_DEMOD_TYPE_ATSC                             //!< Demodulator type ATSC (Mxl691 or Mxl692)
};

/*! Enum of power modes. Used for initial activation, or for activating sleep mode */
enum MXL_EAGLE_POWER_MODE_E
{
  MXL_EAGLE_POWER_MODE_SLEEP,                           //!< Sleep mode: running on XTAL clock, all possible blocks clocked off
  MXL_EAGLE_POWER_MODE_ACTIVE                           //!< Active mode: running on PLL clock, relevant blocks clocked on
};

/*! Enum of GPIOs, used in device GPIO APIs */
enum MXL_EAGLE_GPIO_NUMBER_E
{
  MXL_EAGLE_GPIO_NUMBER_0,                              //!< GPIO0
  MXL_EAGLE_GPIO_NUMBER_1,                              //!< GPIO1
  MXL_EAGLE_GPIO_NUMBER_2,                              //!< GPIO2
  MXL_EAGLE_GPIO_NUMBER_3,                              //!< GPIO3
  MXL_EAGLE_GPIO_NUMBER_4,                              //!< GPIO4
  MXL_EAGLE_GPIO_NUMBER_5,                              //!< GPIO5
  MXL_EAGLE_GPIO_NUMBER_6                               //!< GPIO6
};

/*! Enum of GPIO directions, used in GPIO direction configuration API */
enum MXL_EAGLE_GPIO_DIRECTION_E
{
  MXL_EAGLE_GPIO_DIRECTION_INPUT,                       //!< GPIO direction is input (GPI)
  MXL_EAGLE_GPIO_DIRECTION_OUTPUT                       //!< GPIO direction is output (GPO)
};

/*! Enum of GPIO level, used in device GPIO APIs */
enum MXL_EAGLE_GPIO_LEVEL_E
{
  MXL_EAGLE_GPIO_LEVEL_LOW,                             //!< GPIO level is low ("0")
  MXL_EAGLE_GPIO_LEVEL_HIGH,                            //!< GPIO level is high ("1")
};

/*! Enum of I/O Mux function, used in device I/O mux configuration API */
enum MXL_EAGLE_IOMUX_FUNCTION_E
{
  MXL_EAGLE_IOMUX_FUNC_FEC_LOCK,                        //!< Select FEC_LOCK, overriding other pin functions (JTAG_CLK, MDAT4, GPIO3)
  MXL_EAGLE_IOMUX_FUNC_MERR,                            //!< Select MERR, overriding other pin functions (JTAG_TDO, MDAT3, UART_RX, GPIO4)
};


/*! Enum of MPEG Data format, used in MPEG and OOB output configuration */
enum MXL_EAGLE_MPEG_DATA_FORMAT_E
{
  MXL_EAGLE_DATA_SERIAL_LSB_1ST = 0,
  MXL_EAGLE_DATA_SERIAL_MSB_1ST,

  MXL_EAGLE_DATA_SYNC_WIDTH_BIT = 0,
  MXL_EAGLE_DATA_SYNC_WIDTH_BYTE
};

/*! Enum of MPEG Clock format, used in MPEG and OOB output configuration */
enum MXL_EAGLE_MPEG_CLOCK_FORMAT_E
{
  MXL_EAGLE_CLOCK_ACTIVE_HIGH = 0,
  MXL_EAGLE_CLOCK_ACTIVE_LOW,

  MXL_EAGLE_CLOCK_POSITIVE  = 0,
  MXL_EAGLE_CLOCK_NEGATIVE,

  MXL_EAGLE_CLOCK_IN_PHASE = 0,
  MXL_EAGLE_CLOCK_INVERTED,
};

/*! Enum of MPEG Clock speeds, used in MPEG output configuration */
enum MXL_EAGLE_MPEG_CLOCK_RATE_E
{
  MXL_EAGLE_MPEG_CLOCK_54MHz,
  MXL_EAGLE_MPEG_CLOCK_40_5MHz,
  MXL_EAGLE_MPEG_CLOCK_27MHz,
  MXL_EAGLE_MPEG_CLOCK_13_5MHz,
};

/*! Enum of Interrupt mask bit, used in host interrupt configuration */
enum MXL_EAGLE_INTR_MASK_BITS_E
{
  MXL_EAGLE_INTR_MASK_DEMOD = 0,                        //!< Demodulator locked or lost lock
  MXL_EAGLE_INTR_MASK_SMA_RX = 1,                       //!< Smart antenna message received
  MXL_EAGLE_INTR_MASK_WDOG = 31                         //!< Watchdog expired
};

/*! Enum of QAM Demodulator type, used in QAM configuration */
enum MXL_EAGLE_QAM_DEMOD_ANNEX_TYPE_E
{
  MXL_EAGLE_QAM_DEMOD_ANNEX_B,                          //!< J.83B
  MXL_EAGLE_QAM_DEMOD_ANNEX_A,                          //!< DVB-C
};

/*! Enum of QAM Demodulator modulation, used in QAM configuration and status */
enum MXL_EAGLE_QAM_DEMOD_QAM_TYPE_E
{
  MXL_EAGLE_QAM_DEMOD_QAM16,                            //!< QAM 16 modulation
  MXL_EAGLE_QAM_DEMOD_QAM64,                            //!< QAM 64 modulation
  MXL_EAGLE_QAM_DEMOD_QAM256,                           //!< QAM 256 modulation
  MXL_EAGLE_QAM_DEMOD_QAM1024,                          //!< QAM 1024 modulation
  MXL_EAGLE_QAM_DEMOD_QAM32,                            //!< QAM 32 modulation
  MXL_EAGLE_QAM_DEMOD_QAM128,                           //!< QAM 128 modulation
  MXL_EAGLE_QAM_DEMOD_QPSK,                             //!< QPSK modulation
  MXL_EAGLE_QAM_DEMOD_AUTO,                             //!< Automatic modulation
};

/*! Enum of Demodulator IQ setup, used in QAM, OOB configuration and status */
enum MXL_EAGLE_IQ_FLIP_E
{
  MXL_EAGLE_DEMOD_IQ_NORMAL,                            //!< Normal I/Q
  MXL_EAGLE_DEMOD_IQ_FLIPPED,                           //!< Flipped I/Q
  MXL_EAGLE_DEMOD_IQ_AUTO,                              //!< Automatic I/Q
};

/*! Enum of OOB Demodulator symbol rates, used in OOB configuration */
enum MXL_EAGLE_OOB_DEMOD_SYMB_RATE_E
{
  MXL_EAGLE_OOB_DEMOD_SYMB_RATE_0_772MHz,               //!< OOB ANSI/SCTE 55-2 0.772 MHz
  MXL_EAGLE_OOB_DEMOD_SYMB_RATE_1_024MHz,               //!< OOB ANSI/SCTE 55-1 1.024 MHz
  MXL_EAGLE_OOB_DEMOD_SYMB_RATE_1_544MHz,               //!< OOB ANSI/SCTE 55-2 1.544 MHz
};

/*! Enum of tuner channel tuning mode */
enum MXL_EAGLE_TUNER_CHANNEL_TUNE_MODE_E
{
  MXL_EAGLE_TUNER_CHANNEL_TUNE_MODE_VIEW,               //!< Normal "view" mode - optimal performance
  MXL_EAGLE_TUNER_CHANNEL_TUNE_MODE_SCAN,               //!< Fast "scan" mode - faster tune time
};

/*! Enum of tuner bandwidth */
enum MXL_EAGLE_TUNER_BW_E
{
  MXL_EAGLE_TUNER_BW_6MHz,
  MXL_EAGLE_TUNER_BW_7MHz,
  MXL_EAGLE_TUNER_BW_8MHz,
};

/*! Enum of tuner bandwidth */
enum MXL_EAGLE_JUNCTION_TEMPERATURE_E
{
  MXL_EAGLE_JUNCTION_TEMPERATURE_BELOW_0_CELSIUS          = 0,    //!        Temperature < 0C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_0_TO_14_CELSIUS  = 1,    //!  0C <= Temperature < 14C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_14_TO_28_CELSIUS = 3,    //! 14C <= Temperature < 28C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_28_TO_42_CELSIUS = 2,    //! 28C <= Temperature < 42C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_42_TO_57_CELSIUS = 6,    //! 42C <= Temperature < 57C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_57_TO_71_CELSIUS = 7,    //! 57C <= Temperature < 71C
  MXL_EAGLE_JUNCTION_TEMPERATURE_BETWEEN_71_TO_85_CELSIUS = 5,    //! 71C <= Temperature < 85C
  MXL_EAGLE_JUNCTION_TEMPERATURE_ABOVE_85_CELSIUS         = 4,    //! 85C <= Temperature
};

//STRUCTS
/*! Struct passed in optional callback used during FW download */
struct MXL_EAGLE_FW_DOWNLOAD_CB_PAYLOAD_T
{
  u32  totalLen;                                     //!< Total length of FW file being downloaded
  u32  downloadedLen;                                //!< Downloaded length so far. Percentage = downloadedLen / totalLen * 100
};


/*! Struct used of I2C protocol between host and Eagle, internal use only */
struct __PACK_PREFIX__ MXL_EAGLE_HOST_MSG_HEADER_T
{
  u8   opcode;
  u8   seqNum;
  u8   payloadSize;
  u8   status;
  u32  checksum;
};

/*! Device version information struct */
struct __PACK_PREFIX__ MXL_EAGLE_DEV_VER_T
{
  u8   chipId;                                       //!< See Enum MXL_EAGLE_DEVICE_E
  u8   firmwareVer[MXL_EAGLE_VERSION_SIZE];          //!< Firmware version, e.g. 1.0.0.0.(RC)1
  u8   mxlWareVer[MXL_EAGLE_VERSION_SIZE];           //!< MxLWare version, e.g. 1.0.0.0.(RC)1
};

/*! Xtal configuration struct */
struct __PACK_PREFIX__ MXL_EAGLE_DEV_XTAL_T
{
  u8   xtalCap;                                      //!< XTAL capacitance, accepted range is 1..31 pF.  Default capacitance value is 26 pF.
  u8   clkOutEnable;                                 //!< See MXL_BOOL_E
  u8   clkOutDivEnable;                              //!< Clock out frequency is divided by 6 relative to the XTAL frequency value. see MXL_BOOL_E
  u8   xtalSharingEnable;                            //!< When Crystal sharing mode is enabled crystal capacitance value should be set to 25 pF. See MXL_BOOL_E
  u8   xtalCalibrationEnable;                        //!< Crystal calibration. Default value should be enable. See MXL_BOOL_E
                                                        //!< For Master Eagle devices set this parameter to enable
                                                        //!< For Slave Eagle devices, set this parameter to disable
};

/*! GPIO direction struct, internally used in GPIO configuration API */
struct __PACK_PREFIX__ MXL_EAGLE_DEV_GPIO_DIRECTION_T
{
  u8   gpioNumber;
  u8   gpioDirection;
};

/*! GPO level struct, internally used in GPIO configuration API */
struct __PACK_PREFIX__ MXL_EAGLE_DEV_GPO_LEVEL_T
{
  u8   gpioNumber;
  u8   gpoLevel;
};

/*! Device Status struct */
struct MXL_EAGLE_DEV_STATUS_T
{
  u8   temperature;                                  //!< See MXL_EAGLE_JUNCTION_TEMPERATURE_E
  u8   demodulatorType;                              //!< See MXL_EAGLE_DEMOD_TYPE_E
  u8   powerMode;                                    //!< See MXL_EAGLE_POWER_MODE_E
  u8   cpuUtilizationPercent;                        //!< Average CPU utilization [percent]
};

/*! Device interrupt configuration struct */
struct __PACK_PREFIX__ MXL_EAGLE_DEV_INTR_CFG_T
{
  u32  intrMask;                                     //!< Interrupt mask is a bit mask, bits described in MXL_EAGLE_INTR_MASK_BITS_E
  u8   edgeTrigger;                                  //!< See MXL_BOOL_E
  u8   positiveTrigger;                              //!< See MXL_BOOL_E
  u8   globalEnableInterrupt;                        //!< See MXL_BOOL_E
};

/*! MPEG pad drive parameters, used on MPEG output configuration */
struct MXL_EAGLE_MPEG_PAD_DRIVE_T
{
  u8   padDrvMpegSyn;                                //!< See MXL_EAGLE_IO_MUX_DRIVE_MODE_E
  u8   padDrvMpegDat;                                //!< See MXL_EAGLE_IO_MUX_DRIVE_MODE_E
  u8   padDrvMpegVal;                                //!< See MXL_EAGLE_IO_MUX_DRIVE_MODE_E
  u8   padDrvMpegClk;                                //!< See MXL_EAGLE_IO_MUX_DRIVE_MODE_E
};

/*! MPEGOUT parameter struct, used in MPEG output configuration */
struct MXL_EAGLE_MPEGOUT_PARAMS_T
{
  u8   mpegIsParallel;                               //!< If enabled, selects serial mode vs. parallel
  u8   lsbOrMsbFirst;                                //!< In Serial mode, transmit MSB first or LSB. See MXL_EAGLE_MPEG_DATA_FORMAT_E
  u8   mpegSyncPulseWidth;                           //!< In serial mode, it can be configured with either 1 bit or 1 byte. See MXL_EAGLE_MPEG_DATA_FORMAT_E
  u8   mpegValidPol;                                 //!< VALID polarity, active high or low. See MXL_EAGLE_MPEG_CLOCK_FORMAT_E
  u8   mpegSyncPol;                                  //!< SYNC byte(0x47) indicator, Active high or low. See MXL_EAGLE_MPEG_CLOCK_FORMAT_E
  u8   mpegClkPol;                                   //!< Clock polarity, Active high or low. See MXL_EAGLE_MPEG_CLOCK_FORMAT_E
  u8   mpeg3WireModeEnable;                          //!< In Serial mode, 0: disable 3 wire mode 1: enable 3 wire mode.
  u8   mpegClkFreq;                                  //!< see MXL_EAGLE_MPEG_CLOCK_RATE_E
  struct MXL_EAGLE_MPEG_PAD_DRIVE_T  mpegPadDrv;               //!< Configure MPEG output pad drive strength
};

/*! QAM Demodulator parameters struct, used in QAM params configuration */
struct __PACK_PREFIX__ MXL_EAGLE_QAM_DEMOD_PARAMS_T
{
  u8   annexType;                                    //!< See MXL_EAGLE_QAM_DEMOD_ANNEX_TYPE_E
  u8   qamType;                                      //!< See MXL_EAGLE_QAM_DEMOD_QAM_TYPE_E
  u8   iqFlip;                                       //!< See MXL_EAGLE_IQ_FLIP_E
  u8   searchRangeIdx;                               //!< Equalizer frequency search range. Accepted range is 0..15. The search range depends on the current symbol rate, see documentation
  u8   spurCancellerEnable;                          //!< Spur cancellation enable/disable
  u32  symbolRateHz;                                 //!< For any QAM type in Annex-A mode, For QAM64 in Annex-B mode. Range = [2.0MHz  2MHz]
  u32  symbolRate256QamHz;                           //!< Symbol rate for QAM256 in Annex-B mode. In Annex-A, this should be the same as symbRateInHz. Range as above.
};

/*! QAM Demodulator status */
struct MXL_EAGLE_QAM_DEMOD_STATUS_T
{
  u8   annexType;                                    //!< See MXL_EAGLE_QAM_DEMOD_ANNEX_TYPE_E
  u8   qamType;                                      //!< See MXL_EAGLE_QAM_DEMOD_QAM_TYPE_E
  u8   iqFlip;                                       //!< See MXL_EAGLE_IQ_FLIP_E
  u8   interleaverDepthI;                            //!< Interleaver I depth.
  u8   interleaverDepthJ;                            //!< Interleaver J depth.
  u8   isQamLocked;                                  //!< See MXL_BOOL_E
  u8   isFecLocked;                                  //!< See MXL_BOOL_E
  u8   isMpegLocked;                                 //!< See MXL_BOOL_E
  u16  snrDbTenth;                                   //!< Current SNR value. The returned value is in x10dB format (241 = 24.1dB)
  s16  timingOffset;                                 //!< Timing recovery offset in units of parts per million (ppm)
  s32  carrierOffsetHz;                              //!< Current frequency offset value in [Hz]
};

/*! QAM Demodulator error counters */
struct MXL_EAGLE_QAM_DEMOD_ERROR_COUNTERS_T
{
  u32  correctedCodeWords;                           //!< Corrected code words
  u32  uncorrectedCodeWords;                         //!< Uncorrected code words
  u32  totalCodeWordsReceived;                       //!< Total received code words
  u32  correctedBits;                                //!< Counter for corrected bits
  u32  errorMpegFrames;                              //!< Counter for error MPEG frames
  u32  mpegFramesReceived;                           //!< Counter for received MPEG frames
  u32  erasures;                                     //!< Counter for erasures
};

/*! QAM Demodulator constellation point */
struct MXL_EAGLE_QAM_DEMOD_CONSTELLATION_VAL_T
{
  s16  iValue[12];                                   //!< 12 I samples
  s16  qValue[12];                                   //!< 12 Q samples
};

/*! QAM Demodulator equalizer filter taps */
struct MXL_EAGLE_QAM_DEMOD_EQU_FILTER_T
{
  s16  ffeTaps[MXL_EAGLE_QAM_FFE_TAPS_LENGTH];       //!< FFE filter
  s16  spurTaps[MXL_EAGLE_QAM_SPUR_TAPS_LENGTH];     //!< Spur filter
  s16  dfeTaps[MXL_EAGLE_QAM_DFE_TAPS_LENGTH];       //!< DFE filter
  u8   ffeLeadingTapIndex;                           //!< Location of leading tap (in FFE)
  u8   dfeTapsNumber;                                //!< Number of taps in DFE
};

/*! OOB Demodulator parameters struct, used in OOB params configuration */
struct __PACK_PREFIX__ MXL_EAGLE_OOB_DEMOD_PARAMS_T
{
  u8   symbolRate;                                   //!< See MXL_EAGLE_OOB_DEMOD_SYMB_RATE_E
  u8   iqFlip;                                       //!< See MXL_EAGLE_IQ_FLIP_E
  u8   clockPolarity;                                //!< See MXL_EAGLE_MPEG_CLOCK_FORMAT_E
};

/*! OOB Demodulator error counters */
struct MXL_EAGLE_OOB_DEMOD_ERROR_COUNTERS_T
{
  u32  correctedPackets;                             //!< Corrected packets
  u32  uncorrectedPackets;                           //!< Uncorrected packets
  u32  totalPacketsReceived;                         //!< Total received packets
};

/*! OOB status */
struct __PACK_PREFIX__ MXL_EAGLE_OOB_DEMOD_STATUS_T
{
  u16  snrDbTenth;                                   //!< Current SNR value. The returned value is in x10dB format (241 = 24.1dB)
  s16  timingOffset;                                 //!< Timing recovery offset in units of parts per million (ppm)
  s32  carrierOffsetHz;                              //!< Current frequency offset value in [Hz]
  u8   isQamLocked;                                  //!< See MXL_BOOL_E
  u8   isFecLocked;                                  //!< See MXL_BOOL_E
  u8   isMpegLocked;                                 //!< See MXL_BOOL_E
  u8   isRetuneRequired;                             //!< See MXL_BOOL_E
  u8   iqFlip;                                       //!< See MXL_EAGLE_IQ_FLIP_E
};

/*! ATSC Demodulator status */
struct __PACK_PREFIX__ MXL_EAGLE_ATSC_DEMOD_STATUS_T
{
  s16  snrDbTenths;                                  //!< Current SNR value. The returned value is in x10dB format (241 = 24.1dB)
  s16  timingOffset;                                 //!< Timing recovery offset in units of parts per million (ppm)
  s32  carrierOffsetHz;                              //!< Current frequency offset value in [Hz].
  u8   isFrameLock;                                  //!< See MXL_BOOL_E
  u8   isAtscLock;                                   //!< See MXL_BOOL_E
  u8   isFecLock;                                    //!< See MXL_BOOL_E
};

/*! ATSC Demodulator error counters */
struct MXL_EAGLE_ATSC_DEMOD_ERROR_COUNTERS_T
{
  u32  errorPackets;                                 //!< Number of error packets
  u32  totalPackets;                                 //!< Total number of packets
  u32  errorBytes;                                   //!< Number of error bytes
};

/*! ATSC Demodulator equalizers filter taps */
struct __PACK_PREFIX__ MXL_EAGLE_ATSC_DEMOD_EQU_FILTER_T
{
  s16  ffeTaps[MXL_EAGLE_ATSC_FFE_TAPS_LENGTH];      //!< Frequency domain feed-forward complex filter
  s8   dfeTaps[MXL_EAGLE_ATSC_DFE_TAPS_LENGTH];      //!< Decision feed-back filter
};

/*! Tuner AGC Status */
struct __PACK_PREFIX__ MXL_EAGLE_TUNER_AGC_STATUS_T
{
  u8   isLocked;                                     //!< AGC lock indication, see MXL_BOOL_E
  u16  rawAgcGain;                                   //!< AGC gain [dB] = rawAgcGain / 2^6
  s16  rxPowerDbHundredths;                          //!< Current Rx power. The returned value is in x100dB format (241 = 2.41dB)
};

/*! Tuner channel tune parameters */
struct __PACK_PREFIX__ MXL_EAGLE_TUNER_CHANNEL_PARAMS_T
{
  u32  freqInHz;                                     //!< Channel Center Frequency in HZ, Range = [44MHz 006MHz]
  u8   tuneMode;                                     //!< see MXL_EAGLE_TUNER_CHANNEL_TUNE_MODE_E (view vs. scan)
  u8   bandWidth;                                    //!< see MXL_EAGLE_TUNER_BW_E (6MHz to 8MHz)
};

/*! Tuner channel lock indications */
struct __PACK_PREFIX__ MXL_EAGLE_TUNER_LOCK_STATUS_T
{
  u8   isRfPllLocked;                                //!< Status of Tuner RF synthesizer Lock
  u8   isRefPllLocked;                               //!< Status of Tuner Ref synthesizer Lock
};

/*! Smart antenna parameters struct, used in Smart antenna params configuration */
struct __PACK_PREFIX__ MXL_EAGLE_SMA_PARAMS_T
{
  u8   fullDuplexEnable;                             //!< See MXL_BOOL_E, half duplex disables RX while transmitting, full duplex intended mainly for debug
  u8   rxDisable;                                    //!< See MXL_BOOL_E, e.g ANSI-CTA-909B Mode A operation doesn't require RX, saves power
  u8   idleLogicHigh;                                //!< See MXL_BOOL_E, set polarity of both TX and RX signals to be idle logic high
};

/*! Smart antenna message format */
struct __PACK_PREFIX__ MXL_EAGLE_SMA_MESSAGE_T
{
  u32  payloadBits;                                  //!< Payload bits. Payload bits are sent and received LSB first
  u8   totalNumBits;                                 //!< Number of valid bits in the payload, in the range [8..31]
};

