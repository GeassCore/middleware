#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#ifdef ARCH_CV182X
#include <linux/cvi_vip_snsr.h>
#include "cvi_comm_video.h"
#else
#include <linux/vi_snsr.h>
#include <linux/cvi_comm_video.h>
#endif
#include "cvi_sns_ctrl.h"
#include "os04e10_cmos_ex.h"

static void os04e10_wdr_2048X2048_30_2to1_init(VI_PIPE ViPipe);
static void os04e10_wdr_2048X2048_30_2to1_2L_init(VI_PIPE ViPipe);
static void os04e10_wdr_2048X2048_30_2to1_2L_SLAVE_init(VI_PIPE ViPipe);
static void os04e10_linear_2048X2048_p30_12BIT_init(VI_PIPE ViPipe);
static void os04e10_linear_2048X2048_p30_2L_init(VI_PIPE ViPipe);
static void os04e10_linear_2048X2048_p30_2L_SLAVE_init(VI_PIPE ViPipe);

const CVI_U32 os04e10_addr_byte = 2;
const CVI_U32 os04e10_data_byte = 1;
static int g_fd[VI_MAX_PIPE_NUM] = {[0 ... (VI_MAX_PIPE_NUM - 1)] = -1};
ISP_SNS_MIRRORFLIP_TYPE_E g_aeOs04e10_MirrorFip_Initial[VI_MAX_PIPE_NUM] = {
	ISP_SNS_MIRROR, ISP_SNS_MIRROR, ISP_SNS_MIRROR, ISP_SNS_MIRROR};

int os04e10_i2c_init(VI_PIPE ViPipe)
{
	char acDevFile[16] = {0};
	CVI_U8 u8DevNum;

	if (g_fd[ViPipe] >= 0)
		return CVI_SUCCESS;
	int ret;

	u8DevNum = g_aunOs04e10_BusInfo[ViPipe].s8I2cDev;
	snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);

	g_fd[ViPipe] = open(acDevFile, O_RDWR, 0600);

	if (g_fd[ViPipe] < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "Open /dev/i2c-%u error!\n", u8DevNum);
		return CVI_FAILURE;
	}

	ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, g_aunOs04e10_AddrInfo[ViPipe].s8I2cAddr);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_SLAVE_FORCE error!\n");
		close(g_fd[ViPipe]);
		g_fd[ViPipe] = -1;
		return ret;
	}
	return CVI_SUCCESS;
}

int os04e10_i2c_exit(VI_PIPE ViPipe)
{
	if (g_fd[ViPipe] >= 0) {
		close(g_fd[ViPipe]);
		g_fd[ViPipe] = -1;
		return CVI_SUCCESS;
	}
	return CVI_FAILURE;
}

int os04e10_read_register(VI_PIPE ViPipe, int addr)
{
	int ret, data;
	CVI_U8 buf[8];
	CVI_U8 idx = 0;

	if (g_fd[ViPipe] < 0) {
		ret = os04e10_i2c_init(ViPipe);
		if (ret != CVI_SUCCESS) {
			return CVI_FAILURE;
		}
	}

	if (os04e10_addr_byte == 2)
		buf[idx++] = (addr >> 8) & 0xff;

	// add address byte 0
	buf[idx++] = addr & 0xff;

	ret = write(g_fd[ViPipe], buf, os04e10_addr_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_WRITE error!\n");
		return 0;
	}

	buf[0] = 0;
	buf[1] = 0;
	ret = read(g_fd[ViPipe], buf, os04e10_data_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_READ error!\n");
		return 0;
	}

	// pack read back data
	data = 0;
	if (os04e10_data_byte == 2) {
		data = buf[0] << 8;
		data += buf[1];
	} else {
		data = buf[0];
	}

	syslog(LOG_DEBUG, "i2c r 0x%x = 0x%x\n", addr, data);
	return data;
}

int os04e10_write_register(VI_PIPE ViPipe, int addr, int data)
{
	CVI_U8 idx = 0;
	int ret;
	CVI_U8 buf[8];

	if (g_fd[ViPipe] < 0)
		return CVI_SUCCESS;

	if (os04e10_addr_byte == 2) {
		buf[idx] = (addr >> 8) & 0xff;
		idx++;
		buf[idx] = addr & 0xff;
		idx++;
	}

	if (os04e10_data_byte == 1) {
		buf[idx] = data & 0xff;
		idx++;
	}

	ret = write(g_fd[ViPipe], buf, os04e10_addr_byte + os04e10_data_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_WRITE error!\n");
		return CVI_FAILURE;
	}
	syslog(LOG_DEBUG, "i2c w 0x%x 0x%x\n", addr, data);
	return CVI_SUCCESS;
}
/*
 *static void delay_ms(int ms)
 *{
 *	usleep(ms * 1000);
 *}
 */
void os04e10_standby(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0100, 0x00); /* STANDBY */
}

void os04e10_restart(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0100, 0x01); /* resume */
}

void os04e10_default_reg_init(VI_PIPE ViPipe)
{
	CVI_U32 i;
	CVI_U32 start = 1;
	CVI_U32 end = g_pastOs04e10[ViPipe]->astSyncInfo[0].snsCfg.u32RegNum - 3;

	for (i = start; i < end; i++) {
		os04e10_write_register(ViPipe,
				g_pastOs04e10[ViPipe]->astSyncInfo[0].snsCfg.astI2cData[i].u32RegAddr,
				g_pastOs04e10[ViPipe]->astSyncInfo[0].snsCfg.astI2cData[i].u32Data);
		CVI_TRACE_SNS(CVI_DBG_INFO, "i2c_addr:%#x, i2c_data:%#x\n",
			g_pastOs04e10[ViPipe]->astSyncInfo[0].snsCfg.astI2cData[i].u32RegAddr,
			g_pastOs04e10[ViPipe]->astSyncInfo[0].snsCfg.astI2cData[i].u32Data);
	}
}

ISP_SNS_MIRRORFLIP_TYPE_E ae04e10SnsMirrorFlipMap[ISP_SNS_BUTT][ISP_SNS_BUTT] = {
	{ISP_SNS_NORMAL, ISP_SNS_MIRROR, ISP_SNS_FLIP, ISP_SNS_MIRROR_FLIP},
	{ISP_SNS_MIRROR, ISP_SNS_NORMAL, ISP_SNS_MIRROR_FLIP, ISP_SNS_FLIP},
	{ISP_SNS_FLIP, ISP_SNS_MIRROR_FLIP, ISP_SNS_NORMAL, ISP_SNS_MIRROR},
	{ISP_SNS_MIRROR_FLIP, ISP_SNS_FLIP, ISP_SNS_MIRROR, ISP_SNS_NORMAL}
};

#define OS04E10_ORIEN_ADDR (0x3820)
void os04e10_mirror_flip(VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eSnsMirrorFlip)
{
	CVI_U8 val = 0;
	CVI_U32 i = 0;

	for (i = 0; i < ISP_SNS_BUTT; i++) {
		if (g_aeOs04e10_MirrorFip_Initial[ViPipe] == ae04e10SnsMirrorFlipMap[i][0]) {
			eSnsMirrorFlip = ae04e10SnsMirrorFlipMap[i][eSnsMirrorFlip];
			break;
		}
	}

	val = os04e10_read_register(ViPipe, OS04E10_ORIEN_ADDR);
	val &= ~(0x3 << 1);

	switch (eSnsMirrorFlip) {
	case ISP_SNS_NORMAL:
		break;
	case ISP_SNS_MIRROR:
		val |= 0x1<<1;
		break;
	case ISP_SNS_FLIP:
		val |= 0x1<<2;
		break;
	case ISP_SNS_MIRROR_FLIP:
		val |= 0x1<<1;
		val |= 0x1<<2;
		break;
	default:
		return;
	}

	os04e10_standby(ViPipe);
	os04e10_write_register(ViPipe, OS04E10_ORIEN_ADDR, val);
	usleep(1000*100);
	os04e10_restart(ViPipe);
}

#define OS04E10_CHIP_ID_ADDR_H		0x300A
#define OS04E10_CHIP_ID_ADDR_M		0x300B
#define OS04E10_CHIP_ID_ADDR_L		0x300C
#define OS04E10_CHIP_ID			0x530641

int os04e10_probe(VI_PIPE ViPipe)
{
	int nVal, nVal2, nVal3;

	usleep(500);
	if (os04e10_i2c_init(ViPipe) != CVI_SUCCESS)
		return CVI_FAILURE;

	nVal  = os04e10_read_register(ViPipe, OS04E10_CHIP_ID_ADDR_H);
	nVal2 = os04e10_read_register(ViPipe, OS04E10_CHIP_ID_ADDR_M);
	nVal3 = os04e10_read_register(ViPipe, OS04E10_CHIP_ID_ADDR_L);
	if (nVal < 0 || nVal2 < 0 || nVal3 < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "read sensor id error.\n");
		return nVal;
	}

	if ((((nVal & 0xFF) << 16) | ((nVal2 & 0xFF) << 8) | (nVal3 & 0xFF)) != OS04E10_CHIP_ID) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "Sensor ID Mismatch! Use the wrong sensor??\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

void os04e10_init(VI_PIPE ViPipe)
{
	WDR_MODE_E        enWDRMode;
	CVI_U8            u8ImgMode;

	enWDRMode   = g_pastOs04e10[ViPipe]->enWDRMode;
	u8ImgMode   = g_pastOs04e10[ViPipe]->u8ImgMode;

	os04e10_i2c_init(ViPipe);

	if (enWDRMode == WDR_MODE_2To1_LINE) {
		if (u8ImgMode == OS04E10_MODE_2048X2048_30_WDR) {
			os04e10_wdr_2048X2048_30_2to1_init(ViPipe);
		} else if (u8ImgMode == OS04E10_MODE_2048X2048_30_WDR_2L_MASTER) {
			os04e10_wdr_2048X2048_30_2to1_2L_init(ViPipe);
		} else if (u8ImgMode == OS04E10_MODE_2048X2048_30_WDR_2L_SLAVE) {
			os04e10_wdr_2048X2048_30_2to1_2L_SLAVE_init(ViPipe);
		}
	} else {
		if (u8ImgMode == OS04E10_MODE_2048X2048_30_12BIT) {
			os04e10_linear_2048X2048_p30_12BIT_init(ViPipe);
		} else if (u8ImgMode == OS04E10_MODE_2048X2048_30_10BIT_2L_MASTER) {
			os04e10_linear_2048X2048_p30_2L_init(ViPipe);
		} else if (u8ImgMode == OS04E10_MODE_2048X2048_30_10BIT_2L_SLAVE) {
			os04e10_linear_2048X2048_p30_2L_SLAVE_init(ViPipe);
		}
	}
	g_pastOs04e10[ViPipe]->bInit = CVI_TRUE;
}

// void os04e10_exit(VI_PIPE ViPipe)
// {
// 	os04e10_i2c_exit(ViPipe);
// }

static void os04e10_linear_2048X2048_p30_12BIT_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);
	os04e10_write_register(ViPipe, 0x0301, 0x44);
	os04e10_write_register(ViPipe, 0x0303, 0x02);
	os04e10_write_register(ViPipe, 0x0304, 0x00);
	os04e10_write_register(ViPipe, 0x0305, 0x48);
	os04e10_write_register(ViPipe, 0x0306, 0x00);
	os04e10_write_register(ViPipe, 0x0325, 0x3b);
	os04e10_write_register(ViPipe, 0x0327, 0x04);
	os04e10_write_register(ViPipe, 0x0328, 0x05);
	os04e10_write_register(ViPipe, 0x3002, 0x21);
	os04e10_write_register(ViPipe, 0x3016, 0x72);
	os04e10_write_register(ViPipe, 0x301b, 0xf0);
	os04e10_write_register(ViPipe, 0x301e, 0xb4);
	os04e10_write_register(ViPipe, 0x301f, 0xd0);
	os04e10_write_register(ViPipe, 0x3021, 0x03);
	os04e10_write_register(ViPipe, 0x3022, 0x01);
	os04e10_write_register(ViPipe, 0x3107, 0xa1);
	os04e10_write_register(ViPipe, 0x3108, 0x7d);
	os04e10_write_register(ViPipe, 0x3109, 0xfc);
	os04e10_write_register(ViPipe, 0x3500, 0x00);
	os04e10_write_register(ViPipe, 0x3501, 0x02);
	os04e10_write_register(ViPipe, 0x3502, 0x1c);
	os04e10_write_register(ViPipe, 0x3503, 0x88);
	os04e10_write_register(ViPipe, 0x3508, 0x01);
	os04e10_write_register(ViPipe, 0x3509, 0x00);
	os04e10_write_register(ViPipe, 0x350a, 0x04);
	os04e10_write_register(ViPipe, 0x350b, 0x00);
	os04e10_write_register(ViPipe, 0x350c, 0x04);
	os04e10_write_register(ViPipe, 0x350d, 0x00);
	os04e10_write_register(ViPipe, 0x350e, 0x04);
	os04e10_write_register(ViPipe, 0x350f, 0x00);
	os04e10_write_register(ViPipe, 0x3510, 0x00);
	os04e10_write_register(ViPipe, 0x3511, 0x00);
	os04e10_write_register(ViPipe, 0x3512, 0x20);
	os04e10_write_register(ViPipe, 0x3600, 0x4c);
	os04e10_write_register(ViPipe, 0x3601, 0x08);
	os04e10_write_register(ViPipe, 0x3610, 0x87);
	os04e10_write_register(ViPipe, 0x3611, 0x24);
	os04e10_write_register(ViPipe, 0x3614, 0x4c);
	os04e10_write_register(ViPipe, 0x3620, 0x0c);
	os04e10_write_register(ViPipe, 0x3621, 0x04);
	os04e10_write_register(ViPipe, 0x3632, 0x80);
	os04e10_write_register(ViPipe, 0x3633, 0x00);
	os04e10_write_register(ViPipe, 0x3660, 0x00);
	os04e10_write_register(ViPipe, 0x3662, 0x10);
	os04e10_write_register(ViPipe, 0x3664, 0x70);
	os04e10_write_register(ViPipe, 0x3665, 0x00);
	os04e10_write_register(ViPipe, 0x3666, 0x00);
	os04e10_write_register(ViPipe, 0x3667, 0x00);
	os04e10_write_register(ViPipe, 0x366a, 0x14);
	os04e10_write_register(ViPipe, 0x3670, 0x0b);
	os04e10_write_register(ViPipe, 0x3671, 0x0b);
	os04e10_write_register(ViPipe, 0x3672, 0x0b);
	os04e10_write_register(ViPipe, 0x3673, 0x0b);
	os04e10_write_register(ViPipe, 0x3674, 0x00);
	os04e10_write_register(ViPipe, 0x3678, 0x2b);
	os04e10_write_register(ViPipe, 0x3679, 0x43);
	os04e10_write_register(ViPipe, 0x3681, 0xff);
	os04e10_write_register(ViPipe, 0x3682, 0x86);
	os04e10_write_register(ViPipe, 0x3683, 0x44);
	os04e10_write_register(ViPipe, 0x3684, 0x24);
	os04e10_write_register(ViPipe, 0x3685, 0x00);
	os04e10_write_register(ViPipe, 0x368a, 0x00);
	os04e10_write_register(ViPipe, 0x368d, 0x2b);
	os04e10_write_register(ViPipe, 0x368e, 0x6b);
	os04e10_write_register(ViPipe, 0x3690, 0x00);
	os04e10_write_register(ViPipe, 0x3691, 0x0b);
	os04e10_write_register(ViPipe, 0x3692, 0x0b);
	os04e10_write_register(ViPipe, 0x3693, 0x0b);
	os04e10_write_register(ViPipe, 0x3694, 0x0b);
	os04e10_write_register(ViPipe, 0x3699, 0x03);
	os04e10_write_register(ViPipe, 0x369d, 0x68);
	os04e10_write_register(ViPipe, 0x369e, 0x34);
	os04e10_write_register(ViPipe, 0x369f, 0x1b);
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);
	os04e10_write_register(ViPipe, 0x36a1, 0x77);
	os04e10_write_register(ViPipe, 0x36a2, 0x00);
	os04e10_write_register(ViPipe, 0x36a3, 0x02);
	os04e10_write_register(ViPipe, 0x36a4, 0x02);
	os04e10_write_register(ViPipe, 0x36b0, 0x30);
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);
	os04e10_write_register(ViPipe, 0x36b2, 0x00);
	os04e10_write_register(ViPipe, 0x36b3, 0x00);
	os04e10_write_register(ViPipe, 0x36b4, 0x00);
	os04e10_write_register(ViPipe, 0x36b5, 0x00);
	os04e10_write_register(ViPipe, 0x36b6, 0x00);
	os04e10_write_register(ViPipe, 0x36b7, 0x00);
	os04e10_write_register(ViPipe, 0x36b8, 0x00);
	os04e10_write_register(ViPipe, 0x36b9, 0x00);
	os04e10_write_register(ViPipe, 0x36ba, 0x00);
	os04e10_write_register(ViPipe, 0x36bb, 0x00);
	os04e10_write_register(ViPipe, 0x36bc, 0x00);
	os04e10_write_register(ViPipe, 0x36bd, 0x00);
	os04e10_write_register(ViPipe, 0x36be, 0x00);
	os04e10_write_register(ViPipe, 0x36bf, 0x00);
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);
	os04e10_write_register(ViPipe, 0x36c1, 0x00);
	os04e10_write_register(ViPipe, 0x36c2, 0x00);
	os04e10_write_register(ViPipe, 0x36c3, 0x00);
	os04e10_write_register(ViPipe, 0x36c4, 0x00);
	os04e10_write_register(ViPipe, 0x36c5, 0x00);
	os04e10_write_register(ViPipe, 0x36c6, 0x00);
	os04e10_write_register(ViPipe, 0x36c7, 0x00);
	os04e10_write_register(ViPipe, 0x36c8, 0x00);
	os04e10_write_register(ViPipe, 0x36c9, 0x00);
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);
	os04e10_write_register(ViPipe, 0x36d2, 0x00);
	os04e10_write_register(ViPipe, 0x36d3, 0x08);
	os04e10_write_register(ViPipe, 0x36d4, 0x10);
	os04e10_write_register(ViPipe, 0x36d5, 0x10);
	os04e10_write_register(ViPipe, 0x36d6, 0x00);
	os04e10_write_register(ViPipe, 0x36d7, 0x08);
	os04e10_write_register(ViPipe, 0x36d8, 0x10);
	os04e10_write_register(ViPipe, 0x36d9, 0x10);
	os04e10_write_register(ViPipe, 0x3704, 0x05);
	os04e10_write_register(ViPipe, 0x3705, 0x00);
	os04e10_write_register(ViPipe, 0x3706, 0x2b);
	os04e10_write_register(ViPipe, 0x3709, 0x49);
	os04e10_write_register(ViPipe, 0x370a, 0x00);
	os04e10_write_register(ViPipe, 0x370b, 0x60);
	os04e10_write_register(ViPipe, 0x370e, 0x0c);
	os04e10_write_register(ViPipe, 0x370f, 0x1c);
	os04e10_write_register(ViPipe, 0x3710, 0x00);
	os04e10_write_register(ViPipe, 0x3713, 0x00);
	os04e10_write_register(ViPipe, 0x3714, 0x24);
	os04e10_write_register(ViPipe, 0x3716, 0x24);
	os04e10_write_register(ViPipe, 0x371a, 0x1e);
	os04e10_write_register(ViPipe, 0x3724, 0x0d);
	os04e10_write_register(ViPipe, 0x3725, 0xb2);
	os04e10_write_register(ViPipe, 0x372b, 0x54);
	os04e10_write_register(ViPipe, 0x3739, 0x10);
	os04e10_write_register(ViPipe, 0x373f, 0xb0);
	os04e10_write_register(ViPipe, 0x3740, 0x2b);
	os04e10_write_register(ViPipe, 0x3741, 0x2b);
	os04e10_write_register(ViPipe, 0x3742, 0x2b);
	os04e10_write_register(ViPipe, 0x3743, 0x2b);
	os04e10_write_register(ViPipe, 0x3744, 0x60);
	os04e10_write_register(ViPipe, 0x3745, 0x60);
	os04e10_write_register(ViPipe, 0x3746, 0x60);
	os04e10_write_register(ViPipe, 0x3747, 0x60);
	os04e10_write_register(ViPipe, 0x3748, 0x00);
	os04e10_write_register(ViPipe, 0x3749, 0x00);
	os04e10_write_register(ViPipe, 0x374a, 0x00);
	os04e10_write_register(ViPipe, 0x374b, 0x00);
	os04e10_write_register(ViPipe, 0x374c, 0x00);
	os04e10_write_register(ViPipe, 0x374d, 0x00);
	os04e10_write_register(ViPipe, 0x374e, 0x00);
	os04e10_write_register(ViPipe, 0x374f, 0x00);
	os04e10_write_register(ViPipe, 0x3756, 0x00);
	os04e10_write_register(ViPipe, 0x3757, 0x0e);
	os04e10_write_register(ViPipe, 0x3760, 0x11);
	os04e10_write_register(ViPipe, 0x3767, 0x08);
	os04e10_write_register(ViPipe, 0x3773, 0x01);
	os04e10_write_register(ViPipe, 0x3774, 0x02);
	os04e10_write_register(ViPipe, 0x3775, 0x12);
	os04e10_write_register(ViPipe, 0x3776, 0x02);
	os04e10_write_register(ViPipe, 0x377b, 0x40);
	os04e10_write_register(ViPipe, 0x377c, 0x00);
	os04e10_write_register(ViPipe, 0x377d, 0x0c);
	os04e10_write_register(ViPipe, 0x3782, 0x02);
	os04e10_write_register(ViPipe, 0x3787, 0x24);
	os04e10_write_register(ViPipe, 0x3795, 0x24);
	os04e10_write_register(ViPipe, 0x3796, 0x01);
	os04e10_write_register(ViPipe, 0x3798, 0x40);
	os04e10_write_register(ViPipe, 0x379c, 0x00);
	os04e10_write_register(ViPipe, 0x379d, 0x00);
	os04e10_write_register(ViPipe, 0x379e, 0x00);
	os04e10_write_register(ViPipe, 0x379f, 0x01);
	os04e10_write_register(ViPipe, 0x37a1, 0x10);
	os04e10_write_register(ViPipe, 0x37a6, 0x00);
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);
	os04e10_write_register(ViPipe, 0x37bb, 0x02);
	os04e10_write_register(ViPipe, 0x37be, 0x0a);
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);
	os04e10_write_register(ViPipe, 0x37c2, 0x04);
	os04e10_write_register(ViPipe, 0x37c4, 0x11);
	os04e10_write_register(ViPipe, 0x37c5, 0x80);
	os04e10_write_register(ViPipe, 0x37c6, 0x14);
	os04e10_write_register(ViPipe, 0x37c7, 0x08);
	os04e10_write_register(ViPipe, 0x37c8, 0x42);
	os04e10_write_register(ViPipe, 0x37cd, 0x17);
	os04e10_write_register(ViPipe, 0x37ce, 0x04);
	os04e10_write_register(ViPipe, 0x37d9, 0x08);
	os04e10_write_register(ViPipe, 0x37dc, 0x01);
	os04e10_write_register(ViPipe, 0x37e0, 0x30);
	os04e10_write_register(ViPipe, 0x37e1, 0x10);
	os04e10_write_register(ViPipe, 0x37e2, 0x14);
	os04e10_write_register(ViPipe, 0x37e4, 0x28);
	os04e10_write_register(ViPipe, 0x37ef, 0x00);
	os04e10_write_register(ViPipe, 0x37f4, 0x00);
	os04e10_write_register(ViPipe, 0x37f5, 0x00);
	os04e10_write_register(ViPipe, 0x37f6, 0x00);
	os04e10_write_register(ViPipe, 0x37f7, 0x00);
	os04e10_write_register(ViPipe, 0x3800, 0x00);
	os04e10_write_register(ViPipe, 0x3801, 0x00);
	os04e10_write_register(ViPipe, 0x3802, 0x00);
	os04e10_write_register(ViPipe, 0x3803, 0x00);
	os04e10_write_register(ViPipe, 0x3804, 0x08);
	os04e10_write_register(ViPipe, 0x3805, 0x0f);
	os04e10_write_register(ViPipe, 0x3806, 0x08);
	os04e10_write_register(ViPipe, 0x3807, 0x0f);
	os04e10_write_register(ViPipe, 0x3808, 0x08);
	os04e10_write_register(ViPipe, 0x3809, 0x00);
	os04e10_write_register(ViPipe, 0x380a, 0x08);
	os04e10_write_register(ViPipe, 0x380b, 0x00);
	os04e10_write_register(ViPipe, 0x380c, 0x06);
	os04e10_write_register(ViPipe, 0x380d, 0x51);
	os04e10_write_register(ViPipe, 0x380e, 0x08);
	os04e10_write_register(ViPipe, 0x380f, 0x74);
	os04e10_write_register(ViPipe, 0x3810, 0x00);
	os04e10_write_register(ViPipe, 0x3811, 0x08);
	os04e10_write_register(ViPipe, 0x3812, 0x00);
	os04e10_write_register(ViPipe, 0x3813, 0x08);
	os04e10_write_register(ViPipe, 0x3814, 0x01);
	os04e10_write_register(ViPipe, 0x3815, 0x01);
	os04e10_write_register(ViPipe, 0x3816, 0x01);
	os04e10_write_register(ViPipe, 0x3817, 0x01);
	os04e10_write_register(ViPipe, 0x3818, 0x00);
	os04e10_write_register(ViPipe, 0x3819, 0x00);
	os04e10_write_register(ViPipe, 0x381a, 0x00);
	os04e10_write_register(ViPipe, 0x381b, 0x01);
	os04e10_write_register(ViPipe, 0x3820, 0x88);
	os04e10_write_register(ViPipe, 0x3821, 0x00);
	os04e10_write_register(ViPipe, 0x3822, 0x14);
	os04e10_write_register(ViPipe, 0x3823, 0x08);
	os04e10_write_register(ViPipe, 0x3824, 0x00);
	os04e10_write_register(ViPipe, 0x3825, 0x20);
	os04e10_write_register(ViPipe, 0x3826, 0x00);
	os04e10_write_register(ViPipe, 0x3827, 0x08);
	os04e10_write_register(ViPipe, 0x3829, 0x03);
	os04e10_write_register(ViPipe, 0x382a, 0x00);
	os04e10_write_register(ViPipe, 0x382b, 0x00);
	os04e10_write_register(ViPipe, 0x3832, 0x08);
	os04e10_write_register(ViPipe, 0x3838, 0x00);
	os04e10_write_register(ViPipe, 0x3839, 0x00);
	os04e10_write_register(ViPipe, 0x383a, 0x00);
	os04e10_write_register(ViPipe, 0x383b, 0x00);
	os04e10_write_register(ViPipe, 0x383d, 0x01);
	os04e10_write_register(ViPipe, 0x383e, 0x00);
	os04e10_write_register(ViPipe, 0x383f, 0x00);
	os04e10_write_register(ViPipe, 0x3843, 0x00);
	os04e10_write_register(ViPipe, 0x3848, 0x08);
	os04e10_write_register(ViPipe, 0x3849, 0x00);
	os04e10_write_register(ViPipe, 0x384a, 0x08);
	os04e10_write_register(ViPipe, 0x384b, 0x00);
	os04e10_write_register(ViPipe, 0x384c, 0x00);
	os04e10_write_register(ViPipe, 0x384d, 0x08);
	os04e10_write_register(ViPipe, 0x384e, 0x00);
	os04e10_write_register(ViPipe, 0x384f, 0x08);
	os04e10_write_register(ViPipe, 0x3880, 0x16);
	os04e10_write_register(ViPipe, 0x3881, 0x00);
	os04e10_write_register(ViPipe, 0x3882, 0x08);
	os04e10_write_register(ViPipe, 0x388a, 0x00);
	os04e10_write_register(ViPipe, 0x389a, 0x00);
	os04e10_write_register(ViPipe, 0x389b, 0x00);
	os04e10_write_register(ViPipe, 0x389c, 0x00);
	os04e10_write_register(ViPipe, 0x38a2, 0x02);
	os04e10_write_register(ViPipe, 0x38a3, 0x02);
	os04e10_write_register(ViPipe, 0x38a4, 0x02);
	os04e10_write_register(ViPipe, 0x38a5, 0x02);
	os04e10_write_register(ViPipe, 0x38a7, 0x04);
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);
	os04e10_write_register(ViPipe, 0x38b8, 0x02);
	os04e10_write_register(ViPipe, 0x38c3, 0x06);
	os04e10_write_register(ViPipe, 0x3c80, 0x3f);
	os04e10_write_register(ViPipe, 0x3c86, 0x01);
	os04e10_write_register(ViPipe, 0x3c87, 0x02);
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);
	os04e10_write_register(ViPipe, 0x3f04, 0x04);
	os04e10_write_register(ViPipe, 0x3f07, 0x04);
	os04e10_write_register(ViPipe, 0x3f09, 0x50);
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);
	os04e10_write_register(ViPipe, 0x4000, 0xf3);
	os04e10_write_register(ViPipe, 0x4002, 0x00);
	os04e10_write_register(ViPipe, 0x4003, 0x40);
	os04e10_write_register(ViPipe, 0x4008, 0x00);
	os04e10_write_register(ViPipe, 0x4009, 0x0f);
	os04e10_write_register(ViPipe, 0x400a, 0x01);
	os04e10_write_register(ViPipe, 0x400b, 0x78);
	os04e10_write_register(ViPipe, 0x400f, 0x89);
	os04e10_write_register(ViPipe, 0x4040, 0x00);
	os04e10_write_register(ViPipe, 0x4041, 0x07);
	os04e10_write_register(ViPipe, 0x4090, 0x14);
	os04e10_write_register(ViPipe, 0x40b0, 0x00);
	os04e10_write_register(ViPipe, 0x40b1, 0x00);
	os04e10_write_register(ViPipe, 0x40b2, 0x00);
	os04e10_write_register(ViPipe, 0x40b3, 0x00);
	os04e10_write_register(ViPipe, 0x40b4, 0x00);
	os04e10_write_register(ViPipe, 0x40b5, 0x00);
	os04e10_write_register(ViPipe, 0x40b7, 0x00);
	os04e10_write_register(ViPipe, 0x40b8, 0x00);
	os04e10_write_register(ViPipe, 0x40b9, 0x00);
	os04e10_write_register(ViPipe, 0x40ba, 0x00);
	os04e10_write_register(ViPipe, 0x4300, 0xff);
	os04e10_write_register(ViPipe, 0x4301, 0x00);
	os04e10_write_register(ViPipe, 0x4302, 0x0f);
	os04e10_write_register(ViPipe, 0x4303, 0x01);
	os04e10_write_register(ViPipe, 0x4304, 0x01);
	os04e10_write_register(ViPipe, 0x4305, 0x83);
	os04e10_write_register(ViPipe, 0x4306, 0x21);
	os04e10_write_register(ViPipe, 0x430d, 0x00);
	os04e10_write_register(ViPipe, 0x4501, 0x00);
	os04e10_write_register(ViPipe, 0x4505, 0xc4);
	os04e10_write_register(ViPipe, 0x4506, 0x00);
	os04e10_write_register(ViPipe, 0x4507, 0x60);
	os04e10_write_register(ViPipe, 0x4508, 0x00);
	os04e10_write_register(ViPipe, 0x4600, 0x00);
	os04e10_write_register(ViPipe, 0x4601, 0x40);
	os04e10_write_register(ViPipe, 0x4603, 0x03);
	os04e10_write_register(ViPipe, 0x4800, 0x04);
	os04e10_write_register(ViPipe, 0x4803, 0x00);
	os04e10_write_register(ViPipe, 0x4809, 0x8e);
	os04e10_write_register(ViPipe, 0x480e, 0x00);
	os04e10_write_register(ViPipe, 0x4813, 0x00);
	os04e10_write_register(ViPipe, 0x4814, 0x2a);
	os04e10_write_register(ViPipe, 0x481b, 0x3c);
	os04e10_write_register(ViPipe, 0x481f, 0x26);
	os04e10_write_register(ViPipe, 0x4825, 0x32);
	os04e10_write_register(ViPipe, 0x4829, 0x64);
	os04e10_write_register(ViPipe, 0x4837, 0x11);
	os04e10_write_register(ViPipe, 0x484b, 0x07);
	os04e10_write_register(ViPipe, 0x4883, 0x36);
	os04e10_write_register(ViPipe, 0x4885, 0x03);
	os04e10_write_register(ViPipe, 0x488b, 0x00);
	os04e10_write_register(ViPipe, 0x4d00, 0x04);
	os04e10_write_register(ViPipe, 0x4d01, 0x99);
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);
	os04e10_write_register(ViPipe, 0x4d03, 0xac);
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);
	os04e10_write_register(ViPipe, 0x4d05, 0x54);
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);
	os04e10_write_register(ViPipe, 0x5000, 0xbb);
	os04e10_write_register(ViPipe, 0x5001, 0x09);
	os04e10_write_register(ViPipe, 0x5004, 0x00);
	os04e10_write_register(ViPipe, 0x5005, 0x0e);
	os04e10_write_register(ViPipe, 0x5036, 0x00);
	os04e10_write_register(ViPipe, 0x5080, 0x04);
	os04e10_write_register(ViPipe, 0x5082, 0x00);
	os04e10_write_register(ViPipe, 0x5180, 0x70);
	os04e10_write_register(ViPipe, 0x5181, 0x10);
	os04e10_write_register(ViPipe, 0x5182, 0x71);
	os04e10_write_register(ViPipe, 0x5183, 0xdf);
	os04e10_write_register(ViPipe, 0x5184, 0x02);
	os04e10_write_register(ViPipe, 0x5185, 0x6c);
	os04e10_write_register(ViPipe, 0x5189, 0x48);
	os04e10_write_register(ViPipe, 0x5324, 0x09);
	os04e10_write_register(ViPipe, 0x5325, 0x11);
	os04e10_write_register(ViPipe, 0x5326, 0x1f);
	os04e10_write_register(ViPipe, 0x5327, 0x3b);
	os04e10_write_register(ViPipe, 0x5328, 0x49);
	os04e10_write_register(ViPipe, 0x5329, 0x61);
	os04e10_write_register(ViPipe, 0x532a, 0x9c);
	os04e10_write_register(ViPipe, 0x532b, 0xc9);
	os04e10_write_register(ViPipe, 0x5335, 0x04);
	os04e10_write_register(ViPipe, 0x5336, 0x00);
	os04e10_write_register(ViPipe, 0x5337, 0x04);
	os04e10_write_register(ViPipe, 0x5338, 0x00);
	os04e10_write_register(ViPipe, 0x5339, 0x0b);
	os04e10_write_register(ViPipe, 0x533a, 0x00);
	os04e10_write_register(ViPipe, 0x53a4, 0x09);
	os04e10_write_register(ViPipe, 0x53a5, 0x11);
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);
	os04e10_write_register(ViPipe, 0x53a8, 0x49);
	os04e10_write_register(ViPipe, 0x53a9, 0x61);
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);
	os04e10_write_register(ViPipe, 0x53b5, 0x04);
	os04e10_write_register(ViPipe, 0x53b6, 0x00);
	os04e10_write_register(ViPipe, 0x53b7, 0x04);
	os04e10_write_register(ViPipe, 0x53b8, 0x00);
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);
	os04e10_write_register(ViPipe, 0x53ba, 0x00);
	os04e10_write_register(ViPipe, 0x580b, 0x03);
	os04e10_write_register(ViPipe, 0x580d, 0x00);
	os04e10_write_register(ViPipe, 0x580f, 0x00);
	os04e10_write_register(ViPipe, 0x5820, 0x00);
	os04e10_write_register(ViPipe, 0x5821, 0x00);
	os04e10_write_register(ViPipe, 0x5888, 0x01);

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit LINE Init OK!===2\n", ViPipe);
}

static void os04e10_linear_2048X2048_p30_2L_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);
	os04e10_write_register(ViPipe, 0x0301, 0x44);//
	os04e10_write_register(ViPipe, 0x0303, 0x02);//
	os04e10_write_register(ViPipe, 0x0304, 0x00);//
	os04e10_write_register(ViPipe, 0x0305, 0x48);//
	os04e10_write_register(ViPipe, 0x0306, 0x00);//
	os04e10_write_register(ViPipe, 0x0325, 0x3b);//
	os04e10_write_register(ViPipe, 0x0327, 0x04);//
	os04e10_write_register(ViPipe, 0x0328, 0x05);//
	os04e10_write_register(ViPipe, 0x3002, 0x21);//
	os04e10_write_register(ViPipe, 0x3016, 0x32);//
	os04e10_write_register(ViPipe, 0x301b, 0xf0);//
	os04e10_write_register(ViPipe, 0x301e, 0xb4);//
	os04e10_write_register(ViPipe, 0x301f, 0xd0);//
	os04e10_write_register(ViPipe, 0x3021, 0x03);//
	os04e10_write_register(ViPipe, 0x3022, 0x01);//
	os04e10_write_register(ViPipe, 0x3107, 0xa1);//
	os04e10_write_register(ViPipe, 0x3108, 0x7d);//
	os04e10_write_register(ViPipe, 0x3109, 0xfc);//
	os04e10_write_register(ViPipe, 0x3500, 0x00);//
	os04e10_write_register(ViPipe, 0x3501, 0x08);//
	os04e10_write_register(ViPipe, 0x3502, 0x54);//
	os04e10_write_register(ViPipe, 0x3503, 0x88);//
	os04e10_write_register(ViPipe, 0x3508, 0x01);//
	os04e10_write_register(ViPipe, 0x3509, 0x00);//
	os04e10_write_register(ViPipe, 0x350a, 0x04);//
	os04e10_write_register(ViPipe, 0x350b, 0x00);//
	os04e10_write_register(ViPipe, 0x350c, 0x04);//
	os04e10_write_register(ViPipe, 0x350d, 0x00);//
	os04e10_write_register(ViPipe, 0x350e, 0x04);//
	os04e10_write_register(ViPipe, 0x350f, 0x00);//
	os04e10_write_register(ViPipe, 0x3510, 0x00);//
	os04e10_write_register(ViPipe, 0x3511, 0x00);//
	os04e10_write_register(ViPipe, 0x3512, 0x20);//
	os04e10_write_register(ViPipe, 0x3600, 0x4c);//
	os04e10_write_register(ViPipe, 0x3601, 0x08);//
	os04e10_write_register(ViPipe, 0x3610, 0x87);//
	os04e10_write_register(ViPipe, 0x3611, 0x24);//
	os04e10_write_register(ViPipe, 0x3614, 0x4c);//
	os04e10_write_register(ViPipe, 0x3620, 0x0c);//
	os04e10_write_register(ViPipe, 0x3621, 0x04);//
	os04e10_write_register(ViPipe, 0x3632, 0x80);//
	os04e10_write_register(ViPipe, 0x3633, 0x00);//
	os04e10_write_register(ViPipe, 0x3660, 0x00);//
	os04e10_write_register(ViPipe, 0x3662, 0x10);//
	os04e10_write_register(ViPipe, 0x3664, 0x70);//
	os04e10_write_register(ViPipe, 0x3665, 0x00);//
	os04e10_write_register(ViPipe, 0x3666, 0x00);//
	os04e10_write_register(ViPipe, 0x3667, 0x00);//
	os04e10_write_register(ViPipe, 0x366a, 0x14);//
	os04e10_write_register(ViPipe, 0x3670, 0x0b);//
	os04e10_write_register(ViPipe, 0x3671, 0x0b);//
	os04e10_write_register(ViPipe, 0x3672, 0x0b);//
	os04e10_write_register(ViPipe, 0x3673, 0x0b);//
	os04e10_write_register(ViPipe, 0x3674, 0x00);//
	os04e10_write_register(ViPipe, 0x3678, 0x2b);//
	os04e10_write_register(ViPipe, 0x3679, 0x43);//
	os04e10_write_register(ViPipe, 0x3681, 0xff);//
	os04e10_write_register(ViPipe, 0x3682, 0x86);//
	os04e10_write_register(ViPipe, 0x3683, 0x44);//
	os04e10_write_register(ViPipe, 0x3684, 0x24);//
	os04e10_write_register(ViPipe, 0x3685, 0x00);//
	os04e10_write_register(ViPipe, 0x368a, 0x00);//
	os04e10_write_register(ViPipe, 0x368d, 0x2b);//
	os04e10_write_register(ViPipe, 0x368e, 0x6b);//
	os04e10_write_register(ViPipe, 0x3690, 0x00);//
	os04e10_write_register(ViPipe, 0x3691, 0x0b);//
	os04e10_write_register(ViPipe, 0x3692, 0x0b);//
	os04e10_write_register(ViPipe, 0x3693, 0x0b);//
	os04e10_write_register(ViPipe, 0x3694, 0x0b);//
	os04e10_write_register(ViPipe, 0x3699, 0x03);//
	os04e10_write_register(ViPipe, 0x369d, 0x68);//
	os04e10_write_register(ViPipe, 0x369e, 0x34);//
	os04e10_write_register(ViPipe, 0x369f, 0x1b);//
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);//
	os04e10_write_register(ViPipe, 0x36a1, 0x77);//
	os04e10_write_register(ViPipe, 0x36a2, 0x00);//
	os04e10_write_register(ViPipe, 0x36a3, 0x02);//
	os04e10_write_register(ViPipe, 0x36a4, 0x02);//
	os04e10_write_register(ViPipe, 0x36b0, 0x30);//
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);//
	os04e10_write_register(ViPipe, 0x36b2, 0x00);//
	os04e10_write_register(ViPipe, 0x36b3, 0x00);//
	os04e10_write_register(ViPipe, 0x36b4, 0x00);//
	os04e10_write_register(ViPipe, 0x36b5, 0x00);//
	os04e10_write_register(ViPipe, 0x36b6, 0x00);//
	os04e10_write_register(ViPipe, 0x36b7, 0x00);//
	os04e10_write_register(ViPipe, 0x36b8, 0x00);//
	os04e10_write_register(ViPipe, 0x36b9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ba, 0x00);//
	os04e10_write_register(ViPipe, 0x36bb, 0x00);//
	os04e10_write_register(ViPipe, 0x36bc, 0x00);//
	os04e10_write_register(ViPipe, 0x36bd, 0x00);//
	os04e10_write_register(ViPipe, 0x36be, 0x00);//
	os04e10_write_register(ViPipe, 0x36bf, 0x00);//
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);//
	os04e10_write_register(ViPipe, 0x36c1, 0x00);//
	os04e10_write_register(ViPipe, 0x36c2, 0x00);//
	os04e10_write_register(ViPipe, 0x36c3, 0x00);//
	os04e10_write_register(ViPipe, 0x36c4, 0x00);//
	os04e10_write_register(ViPipe, 0x36c5, 0x00);//
	os04e10_write_register(ViPipe, 0x36c6, 0x00);//
	os04e10_write_register(ViPipe, 0x36c7, 0x00);//
	os04e10_write_register(ViPipe, 0x36c8, 0x00);//
	os04e10_write_register(ViPipe, 0x36c9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);//
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);//
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d2, 0x00);//
	os04e10_write_register(ViPipe, 0x36d3, 0x08);//
	os04e10_write_register(ViPipe, 0x36d4, 0x10);//
	os04e10_write_register(ViPipe, 0x36d5, 0x10);//
	os04e10_write_register(ViPipe, 0x36d6, 0x00);//
	os04e10_write_register(ViPipe, 0x36d7, 0x08);//
	os04e10_write_register(ViPipe, 0x36d8, 0x10);//
	os04e10_write_register(ViPipe, 0x36d9, 0x10);//
	os04e10_write_register(ViPipe, 0x3704, 0x05);//
	os04e10_write_register(ViPipe, 0x3705, 0x00);//
	os04e10_write_register(ViPipe, 0x3706, 0x2b);//
	os04e10_write_register(ViPipe, 0x3709, 0x49);//
	os04e10_write_register(ViPipe, 0x370a, 0x00);//
	os04e10_write_register(ViPipe, 0x370b, 0x60);//
	os04e10_write_register(ViPipe, 0x370e, 0x0c);//
	os04e10_write_register(ViPipe, 0x370f, 0x1c);//
	os04e10_write_register(ViPipe, 0x3710, 0x00);//
	os04e10_write_register(ViPipe, 0x3713, 0x00);//
	os04e10_write_register(ViPipe, 0x3714, 0x24);//
	os04e10_write_register(ViPipe, 0x3716, 0x24);//
	os04e10_write_register(ViPipe, 0x371a, 0x1e);//
	os04e10_write_register(ViPipe, 0x3724, 0x0d);//
	os04e10_write_register(ViPipe, 0x3725, 0xb2);//
	os04e10_write_register(ViPipe, 0x372b, 0x54);//
	os04e10_write_register(ViPipe, 0x3739, 0x10);//
	os04e10_write_register(ViPipe, 0x373f, 0xb0);//
	os04e10_write_register(ViPipe, 0x3740, 0x2b);//
	os04e10_write_register(ViPipe, 0x3741, 0x2b);//
	os04e10_write_register(ViPipe, 0x3742, 0x2b);//
	os04e10_write_register(ViPipe, 0x3743, 0x2b);//
	os04e10_write_register(ViPipe, 0x3744, 0x60);//
	os04e10_write_register(ViPipe, 0x3745, 0x60);//
	os04e10_write_register(ViPipe, 0x3746, 0x60);//
	os04e10_write_register(ViPipe, 0x3747, 0x60);//
	os04e10_write_register(ViPipe, 0x3748, 0x00);//
	os04e10_write_register(ViPipe, 0x3749, 0x00);//
	os04e10_write_register(ViPipe, 0x374a, 0x00);//
	os04e10_write_register(ViPipe, 0x374b, 0x00);//
	os04e10_write_register(ViPipe, 0x374c, 0x00);//
	os04e10_write_register(ViPipe, 0x374d, 0x00);//
	os04e10_write_register(ViPipe, 0x374e, 0x00);//
	os04e10_write_register(ViPipe, 0x374f, 0x00);//
	os04e10_write_register(ViPipe, 0x3756, 0x00);//
	os04e10_write_register(ViPipe, 0x3757, 0x0e);//
	os04e10_write_register(ViPipe, 0x3760, 0x11);//
	os04e10_write_register(ViPipe, 0x3767, 0x08);//
	os04e10_write_register(ViPipe, 0x3773, 0x01);//
	os04e10_write_register(ViPipe, 0x3774, 0x02);//
	os04e10_write_register(ViPipe, 0x3775, 0x12);//
	os04e10_write_register(ViPipe, 0x3776, 0x02);//
	os04e10_write_register(ViPipe, 0x377b, 0x40);//
	os04e10_write_register(ViPipe, 0x377c, 0x00);//
	os04e10_write_register(ViPipe, 0x377d, 0x0c);//
	os04e10_write_register(ViPipe, 0x3782, 0x02);//
	os04e10_write_register(ViPipe, 0x3787, 0x24);//
	os04e10_write_register(ViPipe, 0x3795, 0x24);//
	os04e10_write_register(ViPipe, 0x3796, 0x01);//
	os04e10_write_register(ViPipe, 0x3798, 0x40);//
	os04e10_write_register(ViPipe, 0x379c, 0x00);//
	os04e10_write_register(ViPipe, 0x379d, 0x00);//
	os04e10_write_register(ViPipe, 0x379e, 0x00);//
	os04e10_write_register(ViPipe, 0x379f, 0x01);//
	os04e10_write_register(ViPipe, 0x37a1, 0x10);//
	os04e10_write_register(ViPipe, 0x37a6, 0x00);//
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);//
	os04e10_write_register(ViPipe, 0x37bb, 0x02);//
	os04e10_write_register(ViPipe, 0x37be, 0x0a);//
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);//
	os04e10_write_register(ViPipe, 0x37c2, 0x04);//
	os04e10_write_register(ViPipe, 0x37c4, 0x11);//
	os04e10_write_register(ViPipe, 0x37c5, 0x80);//
	os04e10_write_register(ViPipe, 0x37c6, 0x14);//
	os04e10_write_register(ViPipe, 0x37c7, 0x08);//
	os04e10_write_register(ViPipe, 0x37c8, 0x42);//
	os04e10_write_register(ViPipe, 0x37cd, 0x17);//
	os04e10_write_register(ViPipe, 0x37ce, 0x04);//
	os04e10_write_register(ViPipe, 0x37d9, 0x08);//
	os04e10_write_register(ViPipe, 0x37dc, 0x01);//
	os04e10_write_register(ViPipe, 0x37e0, 0x30);//
	os04e10_write_register(ViPipe, 0x37e1, 0x10);//
	os04e10_write_register(ViPipe, 0x37e2, 0x14);//
	os04e10_write_register(ViPipe, 0x37e4, 0x28);//
	os04e10_write_register(ViPipe, 0x37ef, 0x00);//
	os04e10_write_register(ViPipe, 0x37f4, 0x00);//
	os04e10_write_register(ViPipe, 0x37f5, 0x00);//
	os04e10_write_register(ViPipe, 0x37f6, 0x00);//
	os04e10_write_register(ViPipe, 0x37f7, 0x00);//
	os04e10_write_register(ViPipe, 0x3800, 0x00);//
	os04e10_write_register(ViPipe, 0x3801, 0x00);//
	os04e10_write_register(ViPipe, 0x3802, 0x00);//
	os04e10_write_register(ViPipe, 0x3803, 0x00);//
	os04e10_write_register(ViPipe, 0x3804, 0x08);//
	os04e10_write_register(ViPipe, 0x3805, 0x0f);//
	os04e10_write_register(ViPipe, 0x3806, 0x08);//
	os04e10_write_register(ViPipe, 0x3807, 0x0f);//
	os04e10_write_register(ViPipe, 0x3808, 0x08);//
	os04e10_write_register(ViPipe, 0x3809, 0x00);//
	os04e10_write_register(ViPipe, 0x380a, 0x08);//
	os04e10_write_register(ViPipe, 0x380b, 0x00);//
	os04e10_write_register(ViPipe, 0x380c, 0x06);//
	os04e10_write_register(ViPipe, 0x380d, 0x34);//
	os04e10_write_register(ViPipe, 0x380e, 0x08);//
	os04e10_write_register(ViPipe, 0x380f, 0x9b);//
	os04e10_write_register(ViPipe, 0x3810, 0x00);//
	os04e10_write_register(ViPipe, 0x3811, 0x08);//
	os04e10_write_register(ViPipe, 0x3812, 0x00);//
	os04e10_write_register(ViPipe, 0x3813, 0x08);//
	os04e10_write_register(ViPipe, 0x3814, 0x01);//
	os04e10_write_register(ViPipe, 0x3815, 0x01);//
	os04e10_write_register(ViPipe, 0x3816, 0x01);//
	os04e10_write_register(ViPipe, 0x3817, 0x01);//
	os04e10_write_register(ViPipe, 0x3818, 0x00);//
	os04e10_write_register(ViPipe, 0x3819, 0x00);//
	os04e10_write_register(ViPipe, 0x381a, 0x00);//
	os04e10_write_register(ViPipe, 0x381b, 0x01);//
	os04e10_write_register(ViPipe, 0x3820, 0x88);//
	os04e10_write_register(ViPipe, 0x3821, 0x00);//
	os04e10_write_register(ViPipe, 0x3822, 0x14);//
	os04e10_write_register(ViPipe, 0x3823, 0x08);//
	os04e10_write_register(ViPipe, 0x3824, 0x00);//
	os04e10_write_register(ViPipe, 0x3825, 0x20);//
	os04e10_write_register(ViPipe, 0x3826, 0x00);//
	os04e10_write_register(ViPipe, 0x3827, 0x08);//
	os04e10_write_register(ViPipe, 0x3829, 0x03);//
	os04e10_write_register(ViPipe, 0x382a, 0x00);//
	os04e10_write_register(ViPipe, 0x382b, 0x00);//
	os04e10_write_register(ViPipe, 0x3832, 0x08);//
	os04e10_write_register(ViPipe, 0x3838, 0x00);//
	os04e10_write_register(ViPipe, 0x3839, 0x00);//
	os04e10_write_register(ViPipe, 0x383a, 0x00);//
	os04e10_write_register(ViPipe, 0x383b, 0x00);//
	os04e10_write_register(ViPipe, 0x383d, 0x01);//
	os04e10_write_register(ViPipe, 0x383e, 0x00);//
	os04e10_write_register(ViPipe, 0x383f, 0x00);//
	os04e10_write_register(ViPipe, 0x3843, 0x00);//
	os04e10_write_register(ViPipe, 0x3848, 0x08);//
	os04e10_write_register(ViPipe, 0x3849, 0x00);//
	os04e10_write_register(ViPipe, 0x384a, 0x08);//
	os04e10_write_register(ViPipe, 0x384b, 0x00);//
	os04e10_write_register(ViPipe, 0x384c, 0x00);//
	os04e10_write_register(ViPipe, 0x384d, 0x08);//
	os04e10_write_register(ViPipe, 0x384e, 0x00);//
	os04e10_write_register(ViPipe, 0x384f, 0x08);//
	os04e10_write_register(ViPipe, 0x3880, 0x16);//
	os04e10_write_register(ViPipe, 0x3881, 0x00);//
	os04e10_write_register(ViPipe, 0x3882, 0x08);//
	os04e10_write_register(ViPipe, 0x388a, 0x00);//
	os04e10_write_register(ViPipe, 0x389a, 0x00);//
	os04e10_write_register(ViPipe, 0x389b, 0x00);//
	os04e10_write_register(ViPipe, 0x389c, 0x00);//
	os04e10_write_register(ViPipe, 0x38a2, 0x02);//
	os04e10_write_register(ViPipe, 0x38a3, 0x02);//
	os04e10_write_register(ViPipe, 0x38a4, 0x02);//
	os04e10_write_register(ViPipe, 0x38a5, 0x02);//
	os04e10_write_register(ViPipe, 0x38a7, 0x04);//
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);//
	os04e10_write_register(ViPipe, 0x38b8, 0x02);//
	os04e10_write_register(ViPipe, 0x38c3, 0x06);//
	os04e10_write_register(ViPipe, 0x3c80, 0x3f);//
	os04e10_write_register(ViPipe, 0x3c86, 0x01);//
	os04e10_write_register(ViPipe, 0x3c87, 0x02);//
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);//
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);//
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);//
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);//
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);//
	os04e10_write_register(ViPipe, 0x3f04, 0x04);//
	os04e10_write_register(ViPipe, 0x3f07, 0x04);//
	os04e10_write_register(ViPipe, 0x3f09, 0x50);//
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);//
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);//
	os04e10_write_register(ViPipe, 0x4000, 0xf3);//
	os04e10_write_register(ViPipe, 0x4002, 0x00);//
	os04e10_write_register(ViPipe, 0x4003, 0x40);//
	os04e10_write_register(ViPipe, 0x4008, 0x00);//
	os04e10_write_register(ViPipe, 0x4009, 0x0f);//
	os04e10_write_register(ViPipe, 0x400a, 0x01);//
	os04e10_write_register(ViPipe, 0x400b, 0x78);//
	os04e10_write_register(ViPipe, 0x400f, 0x89);//
	os04e10_write_register(ViPipe, 0x4040, 0x00);//
	os04e10_write_register(ViPipe, 0x4041, 0x07);//
	os04e10_write_register(ViPipe, 0x4090, 0x14);//
	os04e10_write_register(ViPipe, 0x40b0, 0x00);//
	os04e10_write_register(ViPipe, 0x40b1, 0x00);//
	os04e10_write_register(ViPipe, 0x40b2, 0x00);//
	os04e10_write_register(ViPipe, 0x40b3, 0x00);//
	os04e10_write_register(ViPipe, 0x40b4, 0x00);//
	os04e10_write_register(ViPipe, 0x40b5, 0x00);//
	os04e10_write_register(ViPipe, 0x40b7, 0x00);//
	os04e10_write_register(ViPipe, 0x40b8, 0x00);//
	os04e10_write_register(ViPipe, 0x40b9, 0x00);//
	os04e10_write_register(ViPipe, 0x40ba, 0x00);//
	os04e10_write_register(ViPipe, 0x4300, 0xff);//
	os04e10_write_register(ViPipe, 0x4301, 0x00);//
	os04e10_write_register(ViPipe, 0x4302, 0x0f);//
	os04e10_write_register(ViPipe, 0x4303, 0x01);//
	os04e10_write_register(ViPipe, 0x4304, 0x01);//
	os04e10_write_register(ViPipe, 0x4305, 0x83);//
	os04e10_write_register(ViPipe, 0x4306, 0x21);//
	os04e10_write_register(ViPipe, 0x430d, 0x00);//
	os04e10_write_register(ViPipe, 0x4501, 0x00);//
	os04e10_write_register(ViPipe, 0x4505, 0xc4);//
	os04e10_write_register(ViPipe, 0x4506, 0x00);//
	os04e10_write_register(ViPipe, 0x4507, 0x60);//
	os04e10_write_register(ViPipe, 0x4508, 0x00);//
	os04e10_write_register(ViPipe, 0x4600, 0x00);//
	os04e10_write_register(ViPipe, 0x4601, 0x40);//
	os04e10_write_register(ViPipe, 0x4603, 0x03);//
	os04e10_write_register(ViPipe, 0x4800, 0x64);//
	os04e10_write_register(ViPipe, 0x4803, 0x00);//
	os04e10_write_register(ViPipe, 0x4809, 0x8e);//
	os04e10_write_register(ViPipe, 0x480e, 0x00);//
	os04e10_write_register(ViPipe, 0x4813, 0x00);//
	os04e10_write_register(ViPipe, 0x4814, 0x2a);//
	os04e10_write_register(ViPipe, 0x481b, 0x3c);//
	os04e10_write_register(ViPipe, 0x481f, 0x26);//
	os04e10_write_register(ViPipe, 0x4825, 0x32);//
	os04e10_write_register(ViPipe, 0x4829, 0x64);//
	os04e10_write_register(ViPipe, 0x4837, 0x11);//
	os04e10_write_register(ViPipe, 0x484b, 0x07);//
	os04e10_write_register(ViPipe, 0x4883, 0x36);//
	os04e10_write_register(ViPipe, 0x4885, 0x03);//
	os04e10_write_register(ViPipe, 0x488b, 0x00);//
	os04e10_write_register(ViPipe, 0x4d00, 0x04);//
	os04e10_write_register(ViPipe, 0x4d01, 0x99);//
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);//
	os04e10_write_register(ViPipe, 0x4d03, 0xac);//
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);//
	os04e10_write_register(ViPipe, 0x4d05, 0x54);//
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);//
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);//
	os04e10_write_register(ViPipe, 0x5000, 0xbb);//
	os04e10_write_register(ViPipe, 0x5001, 0x09);//
	os04e10_write_register(ViPipe, 0x5004, 0x00);//
	os04e10_write_register(ViPipe, 0x5005, 0x0e);//
	os04e10_write_register(ViPipe, 0x5036, 0x00);//
	os04e10_write_register(ViPipe, 0x5080, 0x04);//
	os04e10_write_register(ViPipe, 0x5082, 0x00);//
	os04e10_write_register(ViPipe, 0x5180, 0x70);//
	os04e10_write_register(ViPipe, 0x5181, 0x10);//
	os04e10_write_register(ViPipe, 0x5182, 0x71);//
	os04e10_write_register(ViPipe, 0x5183, 0xdf);//
	os04e10_write_register(ViPipe, 0x5184, 0x02);//
	os04e10_write_register(ViPipe, 0x5185, 0x6c);//
	os04e10_write_register(ViPipe, 0x5189, 0x48);//
	os04e10_write_register(ViPipe, 0x5324, 0x09);//
	os04e10_write_register(ViPipe, 0x5325, 0x11);//
	os04e10_write_register(ViPipe, 0x5326, 0x1f);//
	os04e10_write_register(ViPipe, 0x5327, 0x3b);//
	os04e10_write_register(ViPipe, 0x5328, 0x49);//
	os04e10_write_register(ViPipe, 0x5329, 0x61);//
	os04e10_write_register(ViPipe, 0x532a, 0x9c);//
	os04e10_write_register(ViPipe, 0x532b, 0xc9);//
	os04e10_write_register(ViPipe, 0x5335, 0x04);//
	os04e10_write_register(ViPipe, 0x5336, 0x00);//
	os04e10_write_register(ViPipe, 0x5337, 0x04);//
	os04e10_write_register(ViPipe, 0x5338, 0x00);//
	os04e10_write_register(ViPipe, 0x5339, 0x0b);//
	os04e10_write_register(ViPipe, 0x533a, 0x00);//
	os04e10_write_register(ViPipe, 0x53a4, 0x09);//
	os04e10_write_register(ViPipe, 0x53a5, 0x11);//
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);//
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);//
	os04e10_write_register(ViPipe, 0x53a8, 0x49);//
	os04e10_write_register(ViPipe, 0x53a9, 0x61);//
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);//
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);//
	os04e10_write_register(ViPipe, 0x53b5, 0x04);//
	os04e10_write_register(ViPipe, 0x53b6, 0x00);//
	os04e10_write_register(ViPipe, 0x53b7, 0x04);//
	os04e10_write_register(ViPipe, 0x53b8, 0x00);//
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);//
	os04e10_write_register(ViPipe, 0x53ba, 0x00);//
	os04e10_write_register(ViPipe, 0x580b, 0x03);//
	os04e10_write_register(ViPipe, 0x580d, 0x00);//
	os04e10_write_register(ViPipe, 0x580f, 0x00);//
	os04e10_write_register(ViPipe, 0x5820, 0x00);//
	os04e10_write_register(ViPipe, 0x5821, 0x00);//
	os04e10_write_register(ViPipe, 0x5888, 0x01);//
	os04e10_write_register(ViPipe, 0x3002, 0x23);// ; [1] vsync_oen, [0]: fsin_oen
	os04e10_write_register(ViPipe, 0x3690, 0x00);// ; [4]: 1'b0, 1st set vsync pulse
	os04e10_write_register(ViPipe, 0x383e, 0x00);// ; vscyn_rise_rcnt_pt[23:16]
	os04e10_write_register(ViPipe, 0x3818, 0x00);// ; Slave vsync pulse position cs [15:8]
	os04e10_write_register(ViPipe, 0x3819, 0x00);// ; Slave vsync pulse position cs [7:0],max is HTS/4
	os04e10_write_register(ViPipe, 0x381a, 0x00);// ; vscyn_rise_rcnt_pt[15:8]
	os04e10_write_register(ViPipe, 0x381b, 0x00);// ; vscyn_rise_rcnt_pt[7:0], max: VTS-12 for AHBIN 720p, (VTS -12)*2 for other formats
	os04e10_write_register(ViPipe, 0x3832, 0x20);// ; default, 8'h08, [7:4] vsync pulse width
	os04e10_write_register(ViPipe, 0x368a, 0x04);// ; GPIO enable

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit 2L LINE MASTER Init OK!===\n", ViPipe);
}

static void os04e10_linear_2048X2048_p30_2L_SLAVE_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);
	os04e10_write_register(ViPipe, 0x0301, 0x44);//
	os04e10_write_register(ViPipe, 0x0303, 0x02);//
	os04e10_write_register(ViPipe, 0x0304, 0x00);//
	os04e10_write_register(ViPipe, 0x0305, 0x48);//
	os04e10_write_register(ViPipe, 0x0306, 0x00);//
	os04e10_write_register(ViPipe, 0x0325, 0x3b);//
	os04e10_write_register(ViPipe, 0x0327, 0x04);//
	os04e10_write_register(ViPipe, 0x0328, 0x05);//
	os04e10_write_register(ViPipe, 0x3002, 0x21);//
	os04e10_write_register(ViPipe, 0x3016, 0x32);//
	os04e10_write_register(ViPipe, 0x301b, 0xf0);//
	os04e10_write_register(ViPipe, 0x301e, 0xb4);//
	os04e10_write_register(ViPipe, 0x301f, 0xd0);//
	os04e10_write_register(ViPipe, 0x3021, 0x03);//
	os04e10_write_register(ViPipe, 0x3022, 0x01);//
	os04e10_write_register(ViPipe, 0x3107, 0xa1);//
	os04e10_write_register(ViPipe, 0x3108, 0x7d);//
	os04e10_write_register(ViPipe, 0x3109, 0xfc);//
	os04e10_write_register(ViPipe, 0x3500, 0x00);//
	os04e10_write_register(ViPipe, 0x3501, 0x08);//
	os04e10_write_register(ViPipe, 0x3502, 0x54);//
	os04e10_write_register(ViPipe, 0x3503, 0x88);//
	os04e10_write_register(ViPipe, 0x3508, 0x01);//
	os04e10_write_register(ViPipe, 0x3509, 0x00);//
	os04e10_write_register(ViPipe, 0x350a, 0x04);//
	os04e10_write_register(ViPipe, 0x350b, 0x00);//
	os04e10_write_register(ViPipe, 0x350c, 0x04);//
	os04e10_write_register(ViPipe, 0x350d, 0x00);//
	os04e10_write_register(ViPipe, 0x350e, 0x04);//
	os04e10_write_register(ViPipe, 0x350f, 0x00);//
	os04e10_write_register(ViPipe, 0x3510, 0x00);//
	os04e10_write_register(ViPipe, 0x3511, 0x00);//
	os04e10_write_register(ViPipe, 0x3512, 0x20);//
	os04e10_write_register(ViPipe, 0x3600, 0x4c);//
	os04e10_write_register(ViPipe, 0x3601, 0x08);//
	os04e10_write_register(ViPipe, 0x3610, 0x87);//
	os04e10_write_register(ViPipe, 0x3611, 0x24);//
	os04e10_write_register(ViPipe, 0x3614, 0x4c);//
	os04e10_write_register(ViPipe, 0x3620, 0x0c);//
	os04e10_write_register(ViPipe, 0x3621, 0x04);//
	os04e10_write_register(ViPipe, 0x3632, 0x80);//
	os04e10_write_register(ViPipe, 0x3633, 0x00);//
	os04e10_write_register(ViPipe, 0x3660, 0x00);//
	os04e10_write_register(ViPipe, 0x3662, 0x10);//
	os04e10_write_register(ViPipe, 0x3664, 0x70);//
	os04e10_write_register(ViPipe, 0x3665, 0x00);//
	os04e10_write_register(ViPipe, 0x3666, 0x00);//
	os04e10_write_register(ViPipe, 0x3667, 0x00);//
	os04e10_write_register(ViPipe, 0x366a, 0x14);//
	os04e10_write_register(ViPipe, 0x3670, 0x0b);//
	os04e10_write_register(ViPipe, 0x3671, 0x0b);//
	os04e10_write_register(ViPipe, 0x3672, 0x0b);//
	os04e10_write_register(ViPipe, 0x3673, 0x0b);//
	os04e10_write_register(ViPipe, 0x3674, 0x00);//
	os04e10_write_register(ViPipe, 0x3678, 0x2b);//
	os04e10_write_register(ViPipe, 0x3679, 0x43);//
	os04e10_write_register(ViPipe, 0x3681, 0xff);//
	os04e10_write_register(ViPipe, 0x3682, 0x86);//
	os04e10_write_register(ViPipe, 0x3683, 0x44);//
	os04e10_write_register(ViPipe, 0x3684, 0x24);//
	os04e10_write_register(ViPipe, 0x3685, 0x00);//
	os04e10_write_register(ViPipe, 0x368a, 0x00);//
	os04e10_write_register(ViPipe, 0x368d, 0x2b);//
	os04e10_write_register(ViPipe, 0x368e, 0x6b);//
	os04e10_write_register(ViPipe, 0x3690, 0x00);//
	os04e10_write_register(ViPipe, 0x3691, 0x0b);//
	os04e10_write_register(ViPipe, 0x3692, 0x0b);//
	os04e10_write_register(ViPipe, 0x3693, 0x0b);//
	os04e10_write_register(ViPipe, 0x3694, 0x0b);//
	os04e10_write_register(ViPipe, 0x3699, 0x03);//
	os04e10_write_register(ViPipe, 0x369d, 0x68);//
	os04e10_write_register(ViPipe, 0x369e, 0x34);//
	os04e10_write_register(ViPipe, 0x369f, 0x1b);//
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);//
	os04e10_write_register(ViPipe, 0x36a1, 0x77);//
	os04e10_write_register(ViPipe, 0x36a2, 0x00);//
	os04e10_write_register(ViPipe, 0x36a3, 0x02);//
	os04e10_write_register(ViPipe, 0x36a4, 0x02);//
	os04e10_write_register(ViPipe, 0x36b0, 0x30);//
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);//
	os04e10_write_register(ViPipe, 0x36b2, 0x00);//
	os04e10_write_register(ViPipe, 0x36b3, 0x00);//
	os04e10_write_register(ViPipe, 0x36b4, 0x00);//
	os04e10_write_register(ViPipe, 0x36b5, 0x00);//
	os04e10_write_register(ViPipe, 0x36b6, 0x00);//
	os04e10_write_register(ViPipe, 0x36b7, 0x00);//
	os04e10_write_register(ViPipe, 0x36b8, 0x00);//
	os04e10_write_register(ViPipe, 0x36b9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ba, 0x00);//
	os04e10_write_register(ViPipe, 0x36bb, 0x00);//
	os04e10_write_register(ViPipe, 0x36bc, 0x00);//
	os04e10_write_register(ViPipe, 0x36bd, 0x00);//
	os04e10_write_register(ViPipe, 0x36be, 0x00);//
	os04e10_write_register(ViPipe, 0x36bf, 0x00);//
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);//
	os04e10_write_register(ViPipe, 0x36c1, 0x00);//
	os04e10_write_register(ViPipe, 0x36c2, 0x00);//
	os04e10_write_register(ViPipe, 0x36c3, 0x00);//
	os04e10_write_register(ViPipe, 0x36c4, 0x00);//
	os04e10_write_register(ViPipe, 0x36c5, 0x00);//
	os04e10_write_register(ViPipe, 0x36c6, 0x00);//
	os04e10_write_register(ViPipe, 0x36c7, 0x00);//
	os04e10_write_register(ViPipe, 0x36c8, 0x00);//
	os04e10_write_register(ViPipe, 0x36c9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);//
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);//
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d2, 0x00);//
	os04e10_write_register(ViPipe, 0x36d3, 0x08);//
	os04e10_write_register(ViPipe, 0x36d4, 0x10);//
	os04e10_write_register(ViPipe, 0x36d5, 0x10);//
	os04e10_write_register(ViPipe, 0x36d6, 0x00);//
	os04e10_write_register(ViPipe, 0x36d7, 0x08);//
	os04e10_write_register(ViPipe, 0x36d8, 0x10);//
	os04e10_write_register(ViPipe, 0x36d9, 0x10);//
	os04e10_write_register(ViPipe, 0x3704, 0x05);//
	os04e10_write_register(ViPipe, 0x3705, 0x00);//
	os04e10_write_register(ViPipe, 0x3706, 0x2b);//
	os04e10_write_register(ViPipe, 0x3709, 0x49);//
	os04e10_write_register(ViPipe, 0x370a, 0x00);//
	os04e10_write_register(ViPipe, 0x370b, 0x60);//
	os04e10_write_register(ViPipe, 0x370e, 0x0c);//
	os04e10_write_register(ViPipe, 0x370f, 0x1c);//
	os04e10_write_register(ViPipe, 0x3710, 0x00);//
	os04e10_write_register(ViPipe, 0x3713, 0x00);//
	os04e10_write_register(ViPipe, 0x3714, 0x24);//
	os04e10_write_register(ViPipe, 0x3716, 0x24);//
	os04e10_write_register(ViPipe, 0x371a, 0x1e);//
	os04e10_write_register(ViPipe, 0x3724, 0x0d);//
	os04e10_write_register(ViPipe, 0x3725, 0xb2);//
	os04e10_write_register(ViPipe, 0x372b, 0x54);//
	os04e10_write_register(ViPipe, 0x3739, 0x10);//
	os04e10_write_register(ViPipe, 0x373f, 0xb0);//
	os04e10_write_register(ViPipe, 0x3740, 0x2b);//
	os04e10_write_register(ViPipe, 0x3741, 0x2b);//
	os04e10_write_register(ViPipe, 0x3742, 0x2b);//
	os04e10_write_register(ViPipe, 0x3743, 0x2b);//
	os04e10_write_register(ViPipe, 0x3744, 0x60);//
	os04e10_write_register(ViPipe, 0x3745, 0x60);//
	os04e10_write_register(ViPipe, 0x3746, 0x60);//
	os04e10_write_register(ViPipe, 0x3747, 0x60);//
	os04e10_write_register(ViPipe, 0x3748, 0x00);//
	os04e10_write_register(ViPipe, 0x3749, 0x00);//
	os04e10_write_register(ViPipe, 0x374a, 0x00);//
	os04e10_write_register(ViPipe, 0x374b, 0x00);//
	os04e10_write_register(ViPipe, 0x374c, 0x00);//
	os04e10_write_register(ViPipe, 0x374d, 0x00);//
	os04e10_write_register(ViPipe, 0x374e, 0x00);//
	os04e10_write_register(ViPipe, 0x374f, 0x00);//
	os04e10_write_register(ViPipe, 0x3756, 0x00);//
	os04e10_write_register(ViPipe, 0x3757, 0x0e);//
	os04e10_write_register(ViPipe, 0x3760, 0x11);//
	os04e10_write_register(ViPipe, 0x3767, 0x08);//
	os04e10_write_register(ViPipe, 0x3773, 0x01);//
	os04e10_write_register(ViPipe, 0x3774, 0x02);//
	os04e10_write_register(ViPipe, 0x3775, 0x12);//
	os04e10_write_register(ViPipe, 0x3776, 0x02);//
	os04e10_write_register(ViPipe, 0x377b, 0x40);//
	os04e10_write_register(ViPipe, 0x377c, 0x00);//
	os04e10_write_register(ViPipe, 0x377d, 0x0c);//
	os04e10_write_register(ViPipe, 0x3782, 0x02);//
	os04e10_write_register(ViPipe, 0x3787, 0x24);//
	os04e10_write_register(ViPipe, 0x3795, 0x24);//
	os04e10_write_register(ViPipe, 0x3796, 0x01);//
	os04e10_write_register(ViPipe, 0x3798, 0x40);//
	os04e10_write_register(ViPipe, 0x379c, 0x00);//
	os04e10_write_register(ViPipe, 0x379d, 0x00);//
	os04e10_write_register(ViPipe, 0x379e, 0x00);//
	os04e10_write_register(ViPipe, 0x379f, 0x01);//
	os04e10_write_register(ViPipe, 0x37a1, 0x10);//
	os04e10_write_register(ViPipe, 0x37a6, 0x00);//
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);//
	os04e10_write_register(ViPipe, 0x37bb, 0x02);//
	os04e10_write_register(ViPipe, 0x37be, 0x0a);//
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);//
	os04e10_write_register(ViPipe, 0x37c2, 0x04);//
	os04e10_write_register(ViPipe, 0x37c4, 0x11);//
	os04e10_write_register(ViPipe, 0x37c5, 0x80);//
	os04e10_write_register(ViPipe, 0x37c6, 0x14);//
	os04e10_write_register(ViPipe, 0x37c7, 0x08);//
	os04e10_write_register(ViPipe, 0x37c8, 0x42);//
	os04e10_write_register(ViPipe, 0x37cd, 0x17);//
	os04e10_write_register(ViPipe, 0x37ce, 0x04);//
	os04e10_write_register(ViPipe, 0x37d9, 0x08);//
	os04e10_write_register(ViPipe, 0x37dc, 0x01);//
	os04e10_write_register(ViPipe, 0x37e0, 0x30);//
	os04e10_write_register(ViPipe, 0x37e1, 0x10);//
	os04e10_write_register(ViPipe, 0x37e2, 0x14);//
	os04e10_write_register(ViPipe, 0x37e4, 0x28);//
	os04e10_write_register(ViPipe, 0x37ef, 0x00);//
	os04e10_write_register(ViPipe, 0x37f4, 0x00);//
	os04e10_write_register(ViPipe, 0x37f5, 0x00);//
	os04e10_write_register(ViPipe, 0x37f6, 0x00);//
	os04e10_write_register(ViPipe, 0x37f7, 0x00);//
	os04e10_write_register(ViPipe, 0x3800, 0x00);//
	os04e10_write_register(ViPipe, 0x3801, 0x00);//
	os04e10_write_register(ViPipe, 0x3802, 0x00);//
	os04e10_write_register(ViPipe, 0x3803, 0x00);//
	os04e10_write_register(ViPipe, 0x3804, 0x08);//
	os04e10_write_register(ViPipe, 0x3805, 0x0f);//
	os04e10_write_register(ViPipe, 0x3806, 0x08);//
	os04e10_write_register(ViPipe, 0x3807, 0x0f);//
	os04e10_write_register(ViPipe, 0x3808, 0x08);//
	os04e10_write_register(ViPipe, 0x3809, 0x00);//
	os04e10_write_register(ViPipe, 0x380a, 0x08);//
	os04e10_write_register(ViPipe, 0x380b, 0x00);//
	os04e10_write_register(ViPipe, 0x380c, 0x06);//
	os04e10_write_register(ViPipe, 0x380d, 0x34);//
	os04e10_write_register(ViPipe, 0x380e, 0x08);//
	os04e10_write_register(ViPipe, 0x380f, 0x9b);//
	os04e10_write_register(ViPipe, 0x3810, 0x00);//
	os04e10_write_register(ViPipe, 0x3811, 0x08);//
	os04e10_write_register(ViPipe, 0x3812, 0x00);//
	os04e10_write_register(ViPipe, 0x3813, 0x08);//
	os04e10_write_register(ViPipe, 0x3814, 0x01);//
	os04e10_write_register(ViPipe, 0x3815, 0x01);//
	os04e10_write_register(ViPipe, 0x3816, 0x01);//
	os04e10_write_register(ViPipe, 0x3817, 0x01);//
	os04e10_write_register(ViPipe, 0x3818, 0x00);//
	os04e10_write_register(ViPipe, 0x3819, 0x00);//
	os04e10_write_register(ViPipe, 0x381a, 0x00);//
	os04e10_write_register(ViPipe, 0x381b, 0x01);//
	os04e10_write_register(ViPipe, 0x3820, 0x88);//
	os04e10_write_register(ViPipe, 0x3821, 0x00);//
	os04e10_write_register(ViPipe, 0x3822, 0x14);//
	os04e10_write_register(ViPipe, 0x3823, 0x08);//
	os04e10_write_register(ViPipe, 0x3824, 0x00);//
	os04e10_write_register(ViPipe, 0x3825, 0x20);//
	os04e10_write_register(ViPipe, 0x3826, 0x00);//
	os04e10_write_register(ViPipe, 0x3827, 0x08);//
	os04e10_write_register(ViPipe, 0x3829, 0x03);//
	os04e10_write_register(ViPipe, 0x382a, 0x00);//
	os04e10_write_register(ViPipe, 0x382b, 0x00);//
	os04e10_write_register(ViPipe, 0x3832, 0x08);//
	os04e10_write_register(ViPipe, 0x3838, 0x00);//
	os04e10_write_register(ViPipe, 0x3839, 0x00);//
	os04e10_write_register(ViPipe, 0x383a, 0x00);//
	os04e10_write_register(ViPipe, 0x383b, 0x00);//
	os04e10_write_register(ViPipe, 0x383d, 0x01);//
	os04e10_write_register(ViPipe, 0x383e, 0x00);//
	os04e10_write_register(ViPipe, 0x383f, 0x00);//
	os04e10_write_register(ViPipe, 0x3843, 0x00);//
	os04e10_write_register(ViPipe, 0x3848, 0x08);//
	os04e10_write_register(ViPipe, 0x3849, 0x00);//
	os04e10_write_register(ViPipe, 0x384a, 0x08);//
	os04e10_write_register(ViPipe, 0x384b, 0x00);//
	os04e10_write_register(ViPipe, 0x384c, 0x00);//
	os04e10_write_register(ViPipe, 0x384d, 0x08);//
	os04e10_write_register(ViPipe, 0x384e, 0x00);//
	os04e10_write_register(ViPipe, 0x384f, 0x08);//
	os04e10_write_register(ViPipe, 0x3880, 0x16);//
	os04e10_write_register(ViPipe, 0x3881, 0x00);//
	os04e10_write_register(ViPipe, 0x3882, 0x08);//
	os04e10_write_register(ViPipe, 0x388a, 0x00);//
	os04e10_write_register(ViPipe, 0x389a, 0x00);//
	os04e10_write_register(ViPipe, 0x389b, 0x00);//
	os04e10_write_register(ViPipe, 0x389c, 0x00);//
	os04e10_write_register(ViPipe, 0x38a2, 0x02);//
	os04e10_write_register(ViPipe, 0x38a3, 0x02);//
	os04e10_write_register(ViPipe, 0x38a4, 0x02);//
	os04e10_write_register(ViPipe, 0x38a5, 0x02);//
	os04e10_write_register(ViPipe, 0x38a7, 0x04);//
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);//
	os04e10_write_register(ViPipe, 0x38b8, 0x02);//
	os04e10_write_register(ViPipe, 0x38c3, 0x06);//
	os04e10_write_register(ViPipe, 0x3c80, 0x3f);//
	os04e10_write_register(ViPipe, 0x3c86, 0x01);//
	os04e10_write_register(ViPipe, 0x3c87, 0x02);//
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);//
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);//
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);//
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);//
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);//
	os04e10_write_register(ViPipe, 0x3f04, 0x04);//
	os04e10_write_register(ViPipe, 0x3f07, 0x04);//
	os04e10_write_register(ViPipe, 0x3f09, 0x50);//
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);//
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);//
	os04e10_write_register(ViPipe, 0x4000, 0xf3);//
	os04e10_write_register(ViPipe, 0x4002, 0x00);//
	os04e10_write_register(ViPipe, 0x4003, 0x40);//
	os04e10_write_register(ViPipe, 0x4008, 0x00);//
	os04e10_write_register(ViPipe, 0x4009, 0x0f);//
	os04e10_write_register(ViPipe, 0x400a, 0x01);//
	os04e10_write_register(ViPipe, 0x400b, 0x78);//
	os04e10_write_register(ViPipe, 0x400f, 0x89);//
	os04e10_write_register(ViPipe, 0x4040, 0x00);//
	os04e10_write_register(ViPipe, 0x4041, 0x07);//
	os04e10_write_register(ViPipe, 0x4090, 0x14);//
	os04e10_write_register(ViPipe, 0x40b0, 0x00);//
	os04e10_write_register(ViPipe, 0x40b1, 0x00);//
	os04e10_write_register(ViPipe, 0x40b2, 0x00);//
	os04e10_write_register(ViPipe, 0x40b3, 0x00);//
	os04e10_write_register(ViPipe, 0x40b4, 0x00);//
	os04e10_write_register(ViPipe, 0x40b5, 0x00);//
	os04e10_write_register(ViPipe, 0x40b7, 0x00);//
	os04e10_write_register(ViPipe, 0x40b8, 0x00);//
	os04e10_write_register(ViPipe, 0x40b9, 0x00);//
	os04e10_write_register(ViPipe, 0x40ba, 0x00);//
	os04e10_write_register(ViPipe, 0x4300, 0xff);//
	os04e10_write_register(ViPipe, 0x4301, 0x00);//
	os04e10_write_register(ViPipe, 0x4302, 0x0f);//
	os04e10_write_register(ViPipe, 0x4303, 0x01);//
	os04e10_write_register(ViPipe, 0x4304, 0x01);//
	os04e10_write_register(ViPipe, 0x4305, 0x83);//
	os04e10_write_register(ViPipe, 0x4306, 0x21);//
	os04e10_write_register(ViPipe, 0x430d, 0x00);//
	os04e10_write_register(ViPipe, 0x4501, 0x00);//
	os04e10_write_register(ViPipe, 0x4505, 0xc4);//
	os04e10_write_register(ViPipe, 0x4506, 0x00);//
	os04e10_write_register(ViPipe, 0x4507, 0x60);//
	os04e10_write_register(ViPipe, 0x4508, 0x00);//
	os04e10_write_register(ViPipe, 0x4600, 0x00);//
	os04e10_write_register(ViPipe, 0x4601, 0x40);//
	os04e10_write_register(ViPipe, 0x4603, 0x03);//
	os04e10_write_register(ViPipe, 0x4800, 0x64);//
	os04e10_write_register(ViPipe, 0x4803, 0x00);//
	os04e10_write_register(ViPipe, 0x4809, 0x8e);//
	os04e10_write_register(ViPipe, 0x480e, 0x00);//
	os04e10_write_register(ViPipe, 0x4813, 0x00);//
	os04e10_write_register(ViPipe, 0x4814, 0x2a);//
	os04e10_write_register(ViPipe, 0x481b, 0x3c);//
	os04e10_write_register(ViPipe, 0x481f, 0x26);//
	os04e10_write_register(ViPipe, 0x4825, 0x32);//
	os04e10_write_register(ViPipe, 0x4829, 0x64);//
	os04e10_write_register(ViPipe, 0x4837, 0x11);//
	os04e10_write_register(ViPipe, 0x484b, 0x07);//
	os04e10_write_register(ViPipe, 0x4883, 0x36);//
	os04e10_write_register(ViPipe, 0x4885, 0x03);//
	os04e10_write_register(ViPipe, 0x488b, 0x00);//
	os04e10_write_register(ViPipe, 0x4d00, 0x04);//
	os04e10_write_register(ViPipe, 0x4d01, 0x99);//
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);//
	os04e10_write_register(ViPipe, 0x4d03, 0xac);//
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);//
	os04e10_write_register(ViPipe, 0x4d05, 0x54);//
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);//
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);//
	os04e10_write_register(ViPipe, 0x5000, 0xbb);//
	os04e10_write_register(ViPipe, 0x5001, 0x09);//
	os04e10_write_register(ViPipe, 0x5004, 0x00);//
	os04e10_write_register(ViPipe, 0x5005, 0x0e);//
	os04e10_write_register(ViPipe, 0x5036, 0x00);//
	os04e10_write_register(ViPipe, 0x5080, 0x04);//
	os04e10_write_register(ViPipe, 0x5082, 0x00);//
	os04e10_write_register(ViPipe, 0x5180, 0x70);//
	os04e10_write_register(ViPipe, 0x5181, 0x10);//
	os04e10_write_register(ViPipe, 0x5182, 0x71);//
	os04e10_write_register(ViPipe, 0x5183, 0xdf);//
	os04e10_write_register(ViPipe, 0x5184, 0x02);//
	os04e10_write_register(ViPipe, 0x5185, 0x6c);//
	os04e10_write_register(ViPipe, 0x5189, 0x48);//
	os04e10_write_register(ViPipe, 0x5324, 0x09);//
	os04e10_write_register(ViPipe, 0x5325, 0x11);//
	os04e10_write_register(ViPipe, 0x5326, 0x1f);//
	os04e10_write_register(ViPipe, 0x5327, 0x3b);//
	os04e10_write_register(ViPipe, 0x5328, 0x49);//
	os04e10_write_register(ViPipe, 0x5329, 0x61);//
	os04e10_write_register(ViPipe, 0x532a, 0x9c);//
	os04e10_write_register(ViPipe, 0x532b, 0xc9);//
	os04e10_write_register(ViPipe, 0x5335, 0x04);//
	os04e10_write_register(ViPipe, 0x5336, 0x00);//
	os04e10_write_register(ViPipe, 0x5337, 0x04);//
	os04e10_write_register(ViPipe, 0x5338, 0x00);//
	os04e10_write_register(ViPipe, 0x5339, 0x0b);//
	os04e10_write_register(ViPipe, 0x533a, 0x00);//
	os04e10_write_register(ViPipe, 0x53a4, 0x09);//
	os04e10_write_register(ViPipe, 0x53a5, 0x11);//
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);//
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);//
	os04e10_write_register(ViPipe, 0x53a8, 0x49);//
	os04e10_write_register(ViPipe, 0x53a9, 0x61);//
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);//
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);//
	os04e10_write_register(ViPipe, 0x53b5, 0x04);//
	os04e10_write_register(ViPipe, 0x53b6, 0x00);//
	os04e10_write_register(ViPipe, 0x53b7, 0x04);//
	os04e10_write_register(ViPipe, 0x53b8, 0x00);//
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);//
	os04e10_write_register(ViPipe, 0x53ba, 0x00);//
	os04e10_write_register(ViPipe, 0x580b, 0x03);//
	os04e10_write_register(ViPipe, 0x580d, 0x00);//
	os04e10_write_register(ViPipe, 0x580f, 0x00);//
	os04e10_write_register(ViPipe, 0x5820, 0x00);//
	os04e10_write_register(ViPipe, 0x5821, 0x00);//
	os04e10_write_register(ViPipe, 0x5888, 0x01);//
	os04e10_write_register(ViPipe, 0x3002, 0x22);// ; [1] vsync_oen, [0]: fsin_oen
	os04e10_write_register(ViPipe, 0x3663, 0x22);// ; [2] fsin pad disable
	os04e10_write_register(ViPipe, 0x3823, 0x00);// ; [3:0]r_ext_vsync_div, SYNC every frame
	os04e10_write_register(ViPipe, 0x3822, 0x44);// ; [6]: ext_vs_adj_vts mode enable [5]: fix_cnt_en
	os04e10_write_register(ViPipe, 0x3832, 0x20);// ; [7:4] vsync pulse width
	os04e10_write_register(ViPipe, 0x368a, 0x04);// ; GPIO enable
	os04e10_write_register(ViPipe, 0x3829, 0x03);// ; [5:4]: vts_adj_threshold = 0, other bits follow base setting
	os04e10_write_register(ViPipe, 0x3844, 0x06);// ; threshold_vts_sub[7:0]
	os04e10_write_register(ViPipe, 0x3843, 0x00);// ; man_vts_adj_val[23:16]
	os04e10_write_register(ViPipe, 0x382a, 0x00);// ; man_vts_adj_val[15:8]
	os04e10_write_register(ViPipe, 0x382b, 0x00);// ; man_vts_adj_val[7:0]

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit 2L LINE SALVE Init OK!===\n", ViPipe);
}

static void os04e10_wdr_2048X2048_30_2to1_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);
	os04e10_write_register(ViPipe, 0x0301, 0x44);
	os04e10_write_register(ViPipe, 0x0303, 0x02);
	os04e10_write_register(ViPipe, 0x0304, 0x00);
	os04e10_write_register(ViPipe, 0x0305, 0x48);
	os04e10_write_register(ViPipe, 0x0306, 0x00);
	os04e10_write_register(ViPipe, 0x0325, 0x3b);
	os04e10_write_register(ViPipe, 0x0327, 0x04);
	os04e10_write_register(ViPipe, 0x0328, 0x05);
	os04e10_write_register(ViPipe, 0x3002, 0x21);
	os04e10_write_register(ViPipe, 0x3016, 0x72);
	os04e10_write_register(ViPipe, 0x301b, 0xf0);
	os04e10_write_register(ViPipe, 0x301e, 0xb4);
	os04e10_write_register(ViPipe, 0x301f, 0xf0);
	os04e10_write_register(ViPipe, 0x3021, 0x03);
	os04e10_write_register(ViPipe, 0x3022, 0x01);
	os04e10_write_register(ViPipe, 0x3107, 0xa1);
	os04e10_write_register(ViPipe, 0x3108, 0x7d);
	os04e10_write_register(ViPipe, 0x3109, 0xfc);
	os04e10_write_register(ViPipe, 0x3500, 0x00);
	os04e10_write_register(ViPipe, 0x3501, 0x04);
	os04e10_write_register(ViPipe, 0x3502, 0x2a);
	os04e10_write_register(ViPipe, 0x3503, 0x88);
	os04e10_write_register(ViPipe, 0x3508, 0x01);
	os04e10_write_register(ViPipe, 0x3509, 0x00);
	os04e10_write_register(ViPipe, 0x350a, 0x04);
	os04e10_write_register(ViPipe, 0x350b, 0x00);
	os04e10_write_register(ViPipe, 0x350c, 0x04);
	os04e10_write_register(ViPipe, 0x350d, 0x00);
	os04e10_write_register(ViPipe, 0x350e, 0x04);
	os04e10_write_register(ViPipe, 0x350f, 0x00);
	os04e10_write_register(ViPipe, 0x3510, 0x00);
	os04e10_write_register(ViPipe, 0x3511, 0x00);
	os04e10_write_register(ViPipe, 0x3512, 0x20);
	os04e10_write_register(ViPipe, 0x3600, 0x4c);
	os04e10_write_register(ViPipe, 0x3601, 0x08);
	os04e10_write_register(ViPipe, 0x3610, 0x87);
	os04e10_write_register(ViPipe, 0x3611, 0x24);
	os04e10_write_register(ViPipe, 0x3614, 0x4c);
	os04e10_write_register(ViPipe, 0x3620, 0x0c);
	os04e10_write_register(ViPipe, 0x3621, 0x04);
	os04e10_write_register(ViPipe, 0x3632, 0x80);
	os04e10_write_register(ViPipe, 0x3633, 0x00);
	os04e10_write_register(ViPipe, 0x3660, 0x04);
	os04e10_write_register(ViPipe, 0x3662, 0x10);
	os04e10_write_register(ViPipe, 0x3664, 0x70);
	os04e10_write_register(ViPipe, 0x3665, 0x00);
	os04e10_write_register(ViPipe, 0x3666, 0x00);
	os04e10_write_register(ViPipe, 0x3667, 0x00);
	os04e10_write_register(ViPipe, 0x366a, 0x54);
	os04e10_write_register(ViPipe, 0x3670, 0x0b);
	os04e10_write_register(ViPipe, 0x3671, 0x0b);
	os04e10_write_register(ViPipe, 0x3672, 0x0b);
	os04e10_write_register(ViPipe, 0x3673, 0x0b);
	os04e10_write_register(ViPipe, 0x3674, 0x00);
	os04e10_write_register(ViPipe, 0x3678, 0x2b);
	os04e10_write_register(ViPipe, 0x3679, 0x43);
	os04e10_write_register(ViPipe, 0x3681, 0xff);
	os04e10_write_register(ViPipe, 0x3682, 0x86);
	os04e10_write_register(ViPipe, 0x3683, 0x44);
	os04e10_write_register(ViPipe, 0x3684, 0x24);
	os04e10_write_register(ViPipe, 0x3685, 0x00);
	os04e10_write_register(ViPipe, 0x368a, 0x00);
	os04e10_write_register(ViPipe, 0x368d, 0x2b);
	os04e10_write_register(ViPipe, 0x368e, 0x6b);
	os04e10_write_register(ViPipe, 0x3690, 0x00);
	os04e10_write_register(ViPipe, 0x3691, 0x0b);
	os04e10_write_register(ViPipe, 0x3692, 0x0b);
	os04e10_write_register(ViPipe, 0x3693, 0x0b);
	os04e10_write_register(ViPipe, 0x3694, 0x0b);
	os04e10_write_register(ViPipe, 0x3699, 0x03);
	os04e10_write_register(ViPipe, 0x369d, 0x68);
	os04e10_write_register(ViPipe, 0x369e, 0x34);
	os04e10_write_register(ViPipe, 0x369f, 0x1b);
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);
	os04e10_write_register(ViPipe, 0x36a1, 0x77);
	os04e10_write_register(ViPipe, 0x36a2, 0xf0);
	os04e10_write_register(ViPipe, 0x36a3, 0x82);
	os04e10_write_register(ViPipe, 0x36a4, 0x82);
	os04e10_write_register(ViPipe, 0x36b0, 0x30);
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);
	os04e10_write_register(ViPipe, 0x36b2, 0x00);
	os04e10_write_register(ViPipe, 0x36b3, 0x00);
	os04e10_write_register(ViPipe, 0x36b4, 0x00);
	os04e10_write_register(ViPipe, 0x36b5, 0x00);
	os04e10_write_register(ViPipe, 0x36b6, 0x00);
	os04e10_write_register(ViPipe, 0x36b7, 0x00);
	os04e10_write_register(ViPipe, 0x36b8, 0x00);
	os04e10_write_register(ViPipe, 0x36b9, 0x00);
	os04e10_write_register(ViPipe, 0x36ba, 0x00);
	os04e10_write_register(ViPipe, 0x36bb, 0x00);
	os04e10_write_register(ViPipe, 0x36bc, 0x00);
	os04e10_write_register(ViPipe, 0x36bd, 0x00);
	os04e10_write_register(ViPipe, 0x36be, 0x00);
	os04e10_write_register(ViPipe, 0x36bf, 0x00);
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);
	os04e10_write_register(ViPipe, 0x36c1, 0x00);
	os04e10_write_register(ViPipe, 0x36c2, 0x00);
	os04e10_write_register(ViPipe, 0x36c3, 0x00);
	os04e10_write_register(ViPipe, 0x36c4, 0x00);
	os04e10_write_register(ViPipe, 0x36c5, 0x00);
	os04e10_write_register(ViPipe, 0x36c6, 0x00);
	os04e10_write_register(ViPipe, 0x36c7, 0x00);
	os04e10_write_register(ViPipe, 0x36c8, 0x00);
	os04e10_write_register(ViPipe, 0x36c9, 0x00);
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);
	os04e10_write_register(ViPipe, 0x36d2, 0x00);
	os04e10_write_register(ViPipe, 0x36d3, 0x08);
	os04e10_write_register(ViPipe, 0x36d4, 0x10);
	os04e10_write_register(ViPipe, 0x36d5, 0x10);
	os04e10_write_register(ViPipe, 0x36d6, 0x00);
	os04e10_write_register(ViPipe, 0x36d7, 0x08);
	os04e10_write_register(ViPipe, 0x36d8, 0x10);
	os04e10_write_register(ViPipe, 0x36d9, 0x10);
	os04e10_write_register(ViPipe, 0x3704, 0x01);
	os04e10_write_register(ViPipe, 0x3705, 0x00);
	os04e10_write_register(ViPipe, 0x3706, 0x2b);
	os04e10_write_register(ViPipe, 0x3709, 0x46);
	os04e10_write_register(ViPipe, 0x370a, 0x00);
	os04e10_write_register(ViPipe, 0x370b, 0x60);
	os04e10_write_register(ViPipe, 0x370e, 0x0c);
	os04e10_write_register(ViPipe, 0x370f, 0x1c);
	os04e10_write_register(ViPipe, 0x3710, 0x00);
	os04e10_write_register(ViPipe, 0x3713, 0x00);
	os04e10_write_register(ViPipe, 0x3714, 0x24);
	os04e10_write_register(ViPipe, 0x3716, 0x24);
	os04e10_write_register(ViPipe, 0x371a, 0x1e);
	os04e10_write_register(ViPipe, 0x3724, 0x0d);
	os04e10_write_register(ViPipe, 0x3725, 0xb2);
	os04e10_write_register(ViPipe, 0x372b, 0x54);
	os04e10_write_register(ViPipe, 0x3739, 0x10);
	os04e10_write_register(ViPipe, 0x373f, 0xb0);
	os04e10_write_register(ViPipe, 0x3740, 0x2b);
	os04e10_write_register(ViPipe, 0x3741, 0x2b);
	os04e10_write_register(ViPipe, 0x3742, 0x2b);
	os04e10_write_register(ViPipe, 0x3743, 0x2b);
	os04e10_write_register(ViPipe, 0x3744, 0x60);
	os04e10_write_register(ViPipe, 0x3745, 0x60);
	os04e10_write_register(ViPipe, 0x3746, 0x60);
	os04e10_write_register(ViPipe, 0x3747, 0x60);
	os04e10_write_register(ViPipe, 0x3748, 0x00);
	os04e10_write_register(ViPipe, 0x3749, 0x00);
	os04e10_write_register(ViPipe, 0x374a, 0x00);
	os04e10_write_register(ViPipe, 0x374b, 0x00);
	os04e10_write_register(ViPipe, 0x374c, 0x00);
	os04e10_write_register(ViPipe, 0x374d, 0x00);
	os04e10_write_register(ViPipe, 0x374e, 0x00);
	os04e10_write_register(ViPipe, 0x374f, 0x00);
	os04e10_write_register(ViPipe, 0x3756, 0x00);
	os04e10_write_register(ViPipe, 0x3757, 0x00);
	os04e10_write_register(ViPipe, 0x3760, 0x22);
	os04e10_write_register(ViPipe, 0x3767, 0x08);
	os04e10_write_register(ViPipe, 0x3773, 0x01);
	os04e10_write_register(ViPipe, 0x3774, 0x02);
	os04e10_write_register(ViPipe, 0x3775, 0x12);
	os04e10_write_register(ViPipe, 0x3776, 0x02);
	os04e10_write_register(ViPipe, 0x377b, 0x4a);
	os04e10_write_register(ViPipe, 0x377c, 0x00);
	os04e10_write_register(ViPipe, 0x377d, 0x0c);
	os04e10_write_register(ViPipe, 0x3782, 0x01);
	os04e10_write_register(ViPipe, 0x3787, 0x14);
	os04e10_write_register(ViPipe, 0x3795, 0x14);
	os04e10_write_register(ViPipe, 0x3796, 0x01);
	os04e10_write_register(ViPipe, 0x3798, 0x40);
	os04e10_write_register(ViPipe, 0x379c, 0x00);
	os04e10_write_register(ViPipe, 0x379d, 0x00);
	os04e10_write_register(ViPipe, 0x379e, 0x00);
	os04e10_write_register(ViPipe, 0x379f, 0x01);
	os04e10_write_register(ViPipe, 0x37a1, 0x10);
	os04e10_write_register(ViPipe, 0x37a6, 0x00);
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);
	os04e10_write_register(ViPipe, 0x37bb, 0x01);
	os04e10_write_register(ViPipe, 0x37be, 0x0a);
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);
	os04e10_write_register(ViPipe, 0x37c2, 0x04);
	os04e10_write_register(ViPipe, 0x37c4, 0x11);
	os04e10_write_register(ViPipe, 0x37c5, 0x80);
	os04e10_write_register(ViPipe, 0x37c6, 0x14);
	os04e10_write_register(ViPipe, 0x37c7, 0x08);
	os04e10_write_register(ViPipe, 0x37c8, 0x42);
	os04e10_write_register(ViPipe, 0x37cd, 0x17);
	os04e10_write_register(ViPipe, 0x37ce, 0x04);
	os04e10_write_register(ViPipe, 0x37d9, 0x08);
	os04e10_write_register(ViPipe, 0x37dc, 0x01);
	os04e10_write_register(ViPipe, 0x37e0, 0x30);
	os04e10_write_register(ViPipe, 0x37e1, 0x10);
	os04e10_write_register(ViPipe, 0x37e2, 0x14);
	os04e10_write_register(ViPipe, 0x37e4, 0x28);
	os04e10_write_register(ViPipe, 0x37ef, 0x00);
	os04e10_write_register(ViPipe, 0x37f4, 0x00);
	os04e10_write_register(ViPipe, 0x37f5, 0x00);
	os04e10_write_register(ViPipe, 0x37f6, 0x00);
	os04e10_write_register(ViPipe, 0x37f7, 0x00);
	os04e10_write_register(ViPipe, 0x3800, 0x00);
	os04e10_write_register(ViPipe, 0x3801, 0x00);
	os04e10_write_register(ViPipe, 0x3802, 0x00);
	os04e10_write_register(ViPipe, 0x3803, 0x00);
	os04e10_write_register(ViPipe, 0x3804, 0x08);
	os04e10_write_register(ViPipe, 0x3805, 0x0f);
	os04e10_write_register(ViPipe, 0x3806, 0x08);
	os04e10_write_register(ViPipe, 0x3807, 0x0f);
	os04e10_write_register(ViPipe, 0x3808, 0x08);
	os04e10_write_register(ViPipe, 0x3809, 0x00);
	os04e10_write_register(ViPipe, 0x380a, 0x08);
	os04e10_write_register(ViPipe, 0x380b, 0x00);
	os04e10_write_register(ViPipe, 0x380c, 0x03);
	os04e10_write_register(ViPipe, 0x380d, 0x1a);
	os04e10_write_register(ViPipe, 0x380e, 0x08);
	os04e10_write_register(ViPipe, 0x380f, 0x9b);
	os04e10_write_register(ViPipe, 0x3810, 0x00);
	os04e10_write_register(ViPipe, 0x3811, 0x08);
	os04e10_write_register(ViPipe, 0x3812, 0x00);
	os04e10_write_register(ViPipe, 0x3813, 0x08);
	os04e10_write_register(ViPipe, 0x3814, 0x01);
	os04e10_write_register(ViPipe, 0x3815, 0x01);
	os04e10_write_register(ViPipe, 0x3816, 0x01);
	os04e10_write_register(ViPipe, 0x3817, 0x01);
	os04e10_write_register(ViPipe, 0x3818, 0x00);
	os04e10_write_register(ViPipe, 0x3819, 0x00);
	os04e10_write_register(ViPipe, 0x381a, 0x00);
	os04e10_write_register(ViPipe, 0x381b, 0x01);
	os04e10_write_register(ViPipe, 0x3820, 0x88);
	os04e10_write_register(ViPipe, 0x3821, 0x04);
	os04e10_write_register(ViPipe, 0x3822, 0x14);
	os04e10_write_register(ViPipe, 0x3823, 0x08);
	os04e10_write_register(ViPipe, 0x3824, 0x00);
	os04e10_write_register(ViPipe, 0x3825, 0x20);
	os04e10_write_register(ViPipe, 0x3826, 0x00);
	os04e10_write_register(ViPipe, 0x3827, 0x08);
	os04e10_write_register(ViPipe, 0x3829, 0x03);
	os04e10_write_register(ViPipe, 0x382a, 0x00);
	os04e10_write_register(ViPipe, 0x382b, 0x00);
	os04e10_write_register(ViPipe, 0x3832, 0x08);
	os04e10_write_register(ViPipe, 0x3838, 0x00);
	os04e10_write_register(ViPipe, 0x3839, 0x00);
	os04e10_write_register(ViPipe, 0x383a, 0x00);
	os04e10_write_register(ViPipe, 0x383b, 0x00);
	os04e10_write_register(ViPipe, 0x383d, 0x01);
	os04e10_write_register(ViPipe, 0x383e, 0x00);
	os04e10_write_register(ViPipe, 0x383f, 0x00);
	os04e10_write_register(ViPipe, 0x3843, 0x00);
	os04e10_write_register(ViPipe, 0x3848, 0x08);
	os04e10_write_register(ViPipe, 0x3849, 0x00);
	os04e10_write_register(ViPipe, 0x384a, 0x08);
	os04e10_write_register(ViPipe, 0x384b, 0x00);
	os04e10_write_register(ViPipe, 0x384c, 0x00);
	os04e10_write_register(ViPipe, 0x384d, 0x08);
	os04e10_write_register(ViPipe, 0x384e, 0x00);
	os04e10_write_register(ViPipe, 0x384f, 0x08);
	os04e10_write_register(ViPipe, 0x3880, 0x16);
	os04e10_write_register(ViPipe, 0x3881, 0x00);
	os04e10_write_register(ViPipe, 0x3882, 0x08);
	os04e10_write_register(ViPipe, 0x388a, 0x00);
	os04e10_write_register(ViPipe, 0x389a, 0x00);
	os04e10_write_register(ViPipe, 0x389b, 0x00);
	os04e10_write_register(ViPipe, 0x389c, 0x00);
	os04e10_write_register(ViPipe, 0x38a2, 0x01);
	os04e10_write_register(ViPipe, 0x38a3, 0x01);
	os04e10_write_register(ViPipe, 0x38a4, 0x01);
	os04e10_write_register(ViPipe, 0x38a5, 0x01);
	os04e10_write_register(ViPipe, 0x38a7, 0x04);
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);
	os04e10_write_register(ViPipe, 0x38b8, 0x02);
	os04e10_write_register(ViPipe, 0x38c3, 0x06);
	os04e10_write_register(ViPipe, 0x3c80, 0x3e);
	os04e10_write_register(ViPipe, 0x3c86, 0x01);
	os04e10_write_register(ViPipe, 0x3c87, 0x02);
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);
	os04e10_write_register(ViPipe, 0x3f04, 0x04);
	os04e10_write_register(ViPipe, 0x3f07, 0x04);
	os04e10_write_register(ViPipe, 0x3f09, 0x50);
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);
	os04e10_write_register(ViPipe, 0x4000, 0xf3);
	os04e10_write_register(ViPipe, 0x4002, 0x00);
	os04e10_write_register(ViPipe, 0x4003, 0x40);
	os04e10_write_register(ViPipe, 0x4008, 0x00);
	os04e10_write_register(ViPipe, 0x4009, 0x0f);
	os04e10_write_register(ViPipe, 0x400a, 0x01);
	os04e10_write_register(ViPipe, 0x400b, 0x78);
	os04e10_write_register(ViPipe, 0x400f, 0x89);
	os04e10_write_register(ViPipe, 0x4040, 0x00);
	os04e10_write_register(ViPipe, 0x4041, 0x07);
	os04e10_write_register(ViPipe, 0x4090, 0x14);
	os04e10_write_register(ViPipe, 0x40b0, 0x00);
	os04e10_write_register(ViPipe, 0x40b1, 0x00);
	os04e10_write_register(ViPipe, 0x40b2, 0x00);
	os04e10_write_register(ViPipe, 0x40b3, 0x00);
	os04e10_write_register(ViPipe, 0x40b4, 0x00);
	os04e10_write_register(ViPipe, 0x40b5, 0x00);
	os04e10_write_register(ViPipe, 0x40b7, 0x00);
	os04e10_write_register(ViPipe, 0x40b8, 0x00);
	os04e10_write_register(ViPipe, 0x40b9, 0x00);
	os04e10_write_register(ViPipe, 0x40ba, 0x01);
	os04e10_write_register(ViPipe, 0x4300, 0xff);
	os04e10_write_register(ViPipe, 0x4301, 0x00);
	os04e10_write_register(ViPipe, 0x4302, 0x0f);
	os04e10_write_register(ViPipe, 0x4303, 0x01);
	os04e10_write_register(ViPipe, 0x4304, 0x01);
	os04e10_write_register(ViPipe, 0x4305, 0x83);
	os04e10_write_register(ViPipe, 0x4306, 0x21);
	os04e10_write_register(ViPipe, 0x430d, 0x00);
	os04e10_write_register(ViPipe, 0x4501, 0x00);
	os04e10_write_register(ViPipe, 0x4505, 0xc4);
	os04e10_write_register(ViPipe, 0x4506, 0x00);
	os04e10_write_register(ViPipe, 0x4507, 0x43);
	os04e10_write_register(ViPipe, 0x4508, 0x00);
	os04e10_write_register(ViPipe, 0x4600, 0x00);
	os04e10_write_register(ViPipe, 0x4601, 0x40);
	os04e10_write_register(ViPipe, 0x4603, 0x03);
	os04e10_write_register(ViPipe, 0x4800, 0x64);
	os04e10_write_register(ViPipe, 0x4803, 0x00);
	os04e10_write_register(ViPipe, 0x4809, 0x8e);
	os04e10_write_register(ViPipe, 0x480e, 0x04);
	os04e10_write_register(ViPipe, 0x4813, 0xe4);
	os04e10_write_register(ViPipe, 0x4814, 0x2a);
	os04e10_write_register(ViPipe, 0x481b, 0x3c);
	os04e10_write_register(ViPipe, 0x481f, 0x26);
	os04e10_write_register(ViPipe, 0x4825, 0x32);
	os04e10_write_register(ViPipe, 0x4829, 0x64);
	os04e10_write_register(ViPipe, 0x4837, 0x11);
	os04e10_write_register(ViPipe, 0x484b, 0x27);
	os04e10_write_register(ViPipe, 0x4883, 0x36);
	os04e10_write_register(ViPipe, 0x4885, 0x03);
	os04e10_write_register(ViPipe, 0x488b, 0x00);
	os04e10_write_register(ViPipe, 0x4d00, 0x04);
	os04e10_write_register(ViPipe, 0x4d01, 0x99);
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);
	os04e10_write_register(ViPipe, 0x4d03, 0xac);
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);
	os04e10_write_register(ViPipe, 0x4d05, 0x54);
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);
	os04e10_write_register(ViPipe, 0x5000, 0xbb);
	os04e10_write_register(ViPipe, 0x5001, 0x09);
	os04e10_write_register(ViPipe, 0x5004, 0x00);
	os04e10_write_register(ViPipe, 0x5005, 0x0e);
	os04e10_write_register(ViPipe, 0x5036, 0x80);
	os04e10_write_register(ViPipe, 0x5080, 0x04);
	os04e10_write_register(ViPipe, 0x5082, 0x00);
	os04e10_write_register(ViPipe, 0x5180, 0x70);
	os04e10_write_register(ViPipe, 0x5181, 0x10);
	os04e10_write_register(ViPipe, 0x5182, 0x71);
	os04e10_write_register(ViPipe, 0x5183, 0xdf);
	os04e10_write_register(ViPipe, 0x5184, 0x02);
	os04e10_write_register(ViPipe, 0x5185, 0x6c);
	os04e10_write_register(ViPipe, 0x5189, 0x48);
	os04e10_write_register(ViPipe, 0x5324, 0x09);
	os04e10_write_register(ViPipe, 0x5325, 0x11);
	os04e10_write_register(ViPipe, 0x5326, 0x1f);
	os04e10_write_register(ViPipe, 0x5327, 0x3b);
	os04e10_write_register(ViPipe, 0x5328, 0x49);
	os04e10_write_register(ViPipe, 0x5329, 0x61);
	os04e10_write_register(ViPipe, 0x532a, 0x9c);
	os04e10_write_register(ViPipe, 0x532b, 0xc9);
	os04e10_write_register(ViPipe, 0x5335, 0x04);
	os04e10_write_register(ViPipe, 0x5336, 0x00);
	os04e10_write_register(ViPipe, 0x5337, 0x04);
	os04e10_write_register(ViPipe, 0x5338, 0x00);
	os04e10_write_register(ViPipe, 0x5339, 0x0b);
	os04e10_write_register(ViPipe, 0x533a, 0x00);
	os04e10_write_register(ViPipe, 0x53a4, 0x09);
	os04e10_write_register(ViPipe, 0x53a5, 0x11);
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);
	os04e10_write_register(ViPipe, 0x53a8, 0x49);
	os04e10_write_register(ViPipe, 0x53a9, 0x61);
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);
	os04e10_write_register(ViPipe, 0x53b5, 0x04);
	os04e10_write_register(ViPipe, 0x53b6, 0x00);
	os04e10_write_register(ViPipe, 0x53b7, 0x04);
	os04e10_write_register(ViPipe, 0x53b8, 0x00);
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);
	os04e10_write_register(ViPipe, 0x53ba, 0x00);
	os04e10_write_register(ViPipe, 0x580b, 0x03);
	os04e10_write_register(ViPipe, 0x580d, 0x00);
	os04e10_write_register(ViPipe, 0x580f, 0x00);
	os04e10_write_register(ViPipe, 0x5820, 0x00);
	os04e10_write_register(ViPipe, 0x5821, 0x00);
	os04e10_write_register(ViPipe, 0x5888, 0x01);

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit 2to1 WDR Init OK!===\n", ViPipe);
}

static void os04e10_wdr_2048X2048_30_2to1_2L_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);//
	os04e10_write_register(ViPipe, 0x0301, 0x44);//
	os04e10_write_register(ViPipe, 0x0303, 0x02);//
	os04e10_write_register(ViPipe, 0x0304, 0x00);//
	os04e10_write_register(ViPipe, 0x0305, 0x50);//
	os04e10_write_register(ViPipe, 0x0306, 0x00);//
	os04e10_write_register(ViPipe, 0x0325, 0x3b);//
	os04e10_write_register(ViPipe, 0x0327, 0x04);//
	os04e10_write_register(ViPipe, 0x0328, 0x05);//
	os04e10_write_register(ViPipe, 0x3002, 0x21);//
	os04e10_write_register(ViPipe, 0x3016, 0x32);//
	os04e10_write_register(ViPipe, 0x301b, 0xf0);//
	os04e10_write_register(ViPipe, 0x301e, 0xb4);//
	os04e10_write_register(ViPipe, 0x301f, 0xf0);//
	os04e10_write_register(ViPipe, 0x3021, 0x03);//
	os04e10_write_register(ViPipe, 0x3022, 0x01);//
	os04e10_write_register(ViPipe, 0x3107, 0xa1);//
	os04e10_write_register(ViPipe, 0x3108, 0x7d);//
	os04e10_write_register(ViPipe, 0x3109, 0xfc);//
	os04e10_write_register(ViPipe, 0x3500, 0x00);//
	os04e10_write_register(ViPipe, 0x3501, 0x04);//
	os04e10_write_register(ViPipe, 0x3502, 0x2a);//
	os04e10_write_register(ViPipe, 0x3503, 0x88);//
	os04e10_write_register(ViPipe, 0x3508, 0x01);//
	os04e10_write_register(ViPipe, 0x3509, 0x00);//
	os04e10_write_register(ViPipe, 0x350a, 0x04);//
	os04e10_write_register(ViPipe, 0x350b, 0x00);//
	os04e10_write_register(ViPipe, 0x350c, 0x04);//
	os04e10_write_register(ViPipe, 0x350d, 0x00);//
	os04e10_write_register(ViPipe, 0x350e, 0x04);//
	os04e10_write_register(ViPipe, 0x350f, 0x00);//
	os04e10_write_register(ViPipe, 0x3510, 0x00);//
	os04e10_write_register(ViPipe, 0x3511, 0x00);//
	os04e10_write_register(ViPipe, 0x3512, 0x20);//
	os04e10_write_register(ViPipe, 0x3600, 0x4c);//
	os04e10_write_register(ViPipe, 0x3601, 0x08);//
	os04e10_write_register(ViPipe, 0x3610, 0x87);//
	os04e10_write_register(ViPipe, 0x3611, 0x24);//
	os04e10_write_register(ViPipe, 0x3614, 0x4c);//
	os04e10_write_register(ViPipe, 0x3620, 0x0c);//
	os04e10_write_register(ViPipe, 0x3621, 0x04);//
	os04e10_write_register(ViPipe, 0x3632, 0x80);//
	os04e10_write_register(ViPipe, 0x3633, 0x00);//
	os04e10_write_register(ViPipe, 0x3660, 0x04);//
	os04e10_write_register(ViPipe, 0x3662, 0x10);//
	os04e10_write_register(ViPipe, 0x3664, 0x70);//
	os04e10_write_register(ViPipe, 0x3665, 0x00);//
	os04e10_write_register(ViPipe, 0x3666, 0x00);//
	os04e10_write_register(ViPipe, 0x3667, 0x00);//
	os04e10_write_register(ViPipe, 0x366a, 0x54);//
	os04e10_write_register(ViPipe, 0x3670, 0x0b);//
	os04e10_write_register(ViPipe, 0x3671, 0x0b);//
	os04e10_write_register(ViPipe, 0x3672, 0x0b);//
	os04e10_write_register(ViPipe, 0x3673, 0x0b);//
	os04e10_write_register(ViPipe, 0x3674, 0x00);//
	os04e10_write_register(ViPipe, 0x3678, 0x2b);//
	os04e10_write_register(ViPipe, 0x3679, 0x43);//
	os04e10_write_register(ViPipe, 0x3681, 0xff);//
	os04e10_write_register(ViPipe, 0x3682, 0x86);//
	os04e10_write_register(ViPipe, 0x3683, 0x44);//
	os04e10_write_register(ViPipe, 0x3684, 0x24);//
	os04e10_write_register(ViPipe, 0x3685, 0x00);//
	os04e10_write_register(ViPipe, 0x368a, 0x00);//
	os04e10_write_register(ViPipe, 0x368d, 0x2b);//
	os04e10_write_register(ViPipe, 0x368e, 0x6b);//
	os04e10_write_register(ViPipe, 0x3690, 0x00);//
	os04e10_write_register(ViPipe, 0x3691, 0x0b);//
	os04e10_write_register(ViPipe, 0x3692, 0x0b);//
	os04e10_write_register(ViPipe, 0x3693, 0x0b);//
	os04e10_write_register(ViPipe, 0x3694, 0x0b);//
	os04e10_write_register(ViPipe, 0x3699, 0x03);//
	os04e10_write_register(ViPipe, 0x369d, 0x68);//
	os04e10_write_register(ViPipe, 0x369e, 0x34);//
	os04e10_write_register(ViPipe, 0x369f, 0x1b);//
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);//
	os04e10_write_register(ViPipe, 0x36a1, 0x77);//
	os04e10_write_register(ViPipe, 0x36a2, 0xf0);//
	os04e10_write_register(ViPipe, 0x36a3, 0x82);//
	os04e10_write_register(ViPipe, 0x36a4, 0x82);//
	os04e10_write_register(ViPipe, 0x36b0, 0x30);//
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);//
	os04e10_write_register(ViPipe, 0x36b2, 0x00);//
	os04e10_write_register(ViPipe, 0x36b3, 0x00);//
	os04e10_write_register(ViPipe, 0x36b4, 0x00);//
	os04e10_write_register(ViPipe, 0x36b5, 0x00);//
	os04e10_write_register(ViPipe, 0x36b6, 0x00);//
	os04e10_write_register(ViPipe, 0x36b7, 0x00);//
	os04e10_write_register(ViPipe, 0x36b8, 0x00);//
	os04e10_write_register(ViPipe, 0x36b9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ba, 0x00);//
	os04e10_write_register(ViPipe, 0x36bb, 0x00);//
	os04e10_write_register(ViPipe, 0x36bc, 0x00);//
	os04e10_write_register(ViPipe, 0x36bd, 0x00);//
	os04e10_write_register(ViPipe, 0x36be, 0x00);//
	os04e10_write_register(ViPipe, 0x36bf, 0x00);//
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);//
	os04e10_write_register(ViPipe, 0x36c1, 0x00);//
	os04e10_write_register(ViPipe, 0x36c2, 0x00);//
	os04e10_write_register(ViPipe, 0x36c3, 0x00);//
	os04e10_write_register(ViPipe, 0x36c4, 0x00);//
	os04e10_write_register(ViPipe, 0x36c5, 0x00);//
	os04e10_write_register(ViPipe, 0x36c6, 0x00);//
	os04e10_write_register(ViPipe, 0x36c7, 0x00);//
	os04e10_write_register(ViPipe, 0x36c8, 0x00);//
	os04e10_write_register(ViPipe, 0x36c9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);//
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);//
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d2, 0x00);//
	os04e10_write_register(ViPipe, 0x36d3, 0x08);//
	os04e10_write_register(ViPipe, 0x36d4, 0x10);//
	os04e10_write_register(ViPipe, 0x36d5, 0x10);//
	os04e10_write_register(ViPipe, 0x36d6, 0x00);//
	os04e10_write_register(ViPipe, 0x36d7, 0x08);//
	os04e10_write_register(ViPipe, 0x36d8, 0x10);//
	os04e10_write_register(ViPipe, 0x36d9, 0x10);//
	os04e10_write_register(ViPipe, 0x3704, 0x01);//
	os04e10_write_register(ViPipe, 0x3705, 0x00);//
	os04e10_write_register(ViPipe, 0x3706, 0x2b);//
	os04e10_write_register(ViPipe, 0x3709, 0x46);//
	os04e10_write_register(ViPipe, 0x370a, 0x00);//
	os04e10_write_register(ViPipe, 0x370b, 0x60);//
	os04e10_write_register(ViPipe, 0x370e, 0x0c);//
	os04e10_write_register(ViPipe, 0x370f, 0x1c);//
	os04e10_write_register(ViPipe, 0x3710, 0x00);//
	os04e10_write_register(ViPipe, 0x3713, 0x00);//
	os04e10_write_register(ViPipe, 0x3714, 0x24);//
	os04e10_write_register(ViPipe, 0x3716, 0x24);//
	os04e10_write_register(ViPipe, 0x371a, 0x1e);//
	os04e10_write_register(ViPipe, 0x3724, 0x0d);//
	os04e10_write_register(ViPipe, 0x3725, 0xb2);//
	os04e10_write_register(ViPipe, 0x372b, 0x54);//
	os04e10_write_register(ViPipe, 0x3739, 0x10);//
	os04e10_write_register(ViPipe, 0x373f, 0xb0);//
	os04e10_write_register(ViPipe, 0x3740, 0x2b);//
	os04e10_write_register(ViPipe, 0x3741, 0x2b);//
	os04e10_write_register(ViPipe, 0x3742, 0x2b);//
	os04e10_write_register(ViPipe, 0x3743, 0x2b);//
	os04e10_write_register(ViPipe, 0x3744, 0x60);//
	os04e10_write_register(ViPipe, 0x3745, 0x60);//
	os04e10_write_register(ViPipe, 0x3746, 0x60);//
	os04e10_write_register(ViPipe, 0x3747, 0x60);//
	os04e10_write_register(ViPipe, 0x3748, 0x00);//
	os04e10_write_register(ViPipe, 0x3749, 0x00);//
	os04e10_write_register(ViPipe, 0x374a, 0x00);//
	os04e10_write_register(ViPipe, 0x374b, 0x00);//
	os04e10_write_register(ViPipe, 0x374c, 0x00);//
	os04e10_write_register(ViPipe, 0x374d, 0x00);//
	os04e10_write_register(ViPipe, 0x374e, 0x00);//
	os04e10_write_register(ViPipe, 0x374f, 0x00);//
	os04e10_write_register(ViPipe, 0x3756, 0x00);//
	os04e10_write_register(ViPipe, 0x3757, 0x00);//
	os04e10_write_register(ViPipe, 0x3760, 0x22);//
	os04e10_write_register(ViPipe, 0x3767, 0x08);//
	os04e10_write_register(ViPipe, 0x3773, 0x01);//
	os04e10_write_register(ViPipe, 0x3774, 0x02);//
	os04e10_write_register(ViPipe, 0x3775, 0x12);//
	os04e10_write_register(ViPipe, 0x3776, 0x02);//
	os04e10_write_register(ViPipe, 0x377b, 0x4a);//
	os04e10_write_register(ViPipe, 0x377c, 0x00);//
	os04e10_write_register(ViPipe, 0x377d, 0x0c);//
	os04e10_write_register(ViPipe, 0x3782, 0x01);//
	os04e10_write_register(ViPipe, 0x3787, 0x14);//
	os04e10_write_register(ViPipe, 0x3795, 0x14);//
	os04e10_write_register(ViPipe, 0x3796, 0x01);//
	os04e10_write_register(ViPipe, 0x3798, 0x40);//
	os04e10_write_register(ViPipe, 0x379c, 0x00);//
	os04e10_write_register(ViPipe, 0x379d, 0x00);//
	os04e10_write_register(ViPipe, 0x379e, 0x00);//
	os04e10_write_register(ViPipe, 0x379f, 0x01);//
	os04e10_write_register(ViPipe, 0x37a1, 0x10);//
	os04e10_write_register(ViPipe, 0x37a6, 0x00);//
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);//
	os04e10_write_register(ViPipe, 0x37bb, 0x01);//
	os04e10_write_register(ViPipe, 0x37be, 0x0a);//
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);//
	os04e10_write_register(ViPipe, 0x37c2, 0x04);//
	os04e10_write_register(ViPipe, 0x37c4, 0x11);//
	os04e10_write_register(ViPipe, 0x37c5, 0x80);//
	os04e10_write_register(ViPipe, 0x37c6, 0x14);//
	os04e10_write_register(ViPipe, 0x37c7, 0x08);//
	os04e10_write_register(ViPipe, 0x37c8, 0x42);//
	os04e10_write_register(ViPipe, 0x37cd, 0x17);//
	os04e10_write_register(ViPipe, 0x37ce, 0x04);//
	os04e10_write_register(ViPipe, 0x37d9, 0x08);//
	os04e10_write_register(ViPipe, 0x37dc, 0x01);//
	os04e10_write_register(ViPipe, 0x37e0, 0x30);//
	os04e10_write_register(ViPipe, 0x37e1, 0x10);//
	os04e10_write_register(ViPipe, 0x37e2, 0x14);//
	os04e10_write_register(ViPipe, 0x37e4, 0x28);//
	os04e10_write_register(ViPipe, 0x37ef, 0x00);//
	os04e10_write_register(ViPipe, 0x37f4, 0x00);//
	os04e10_write_register(ViPipe, 0x37f5, 0x00);//
	os04e10_write_register(ViPipe, 0x37f6, 0x00);//
	os04e10_write_register(ViPipe, 0x37f7, 0x00);//
	os04e10_write_register(ViPipe, 0x3800, 0x00);//
	os04e10_write_register(ViPipe, 0x3801, 0x00);//
	os04e10_write_register(ViPipe, 0x3802, 0x00);//
	os04e10_write_register(ViPipe, 0x3803, 0x00);//
	os04e10_write_register(ViPipe, 0x3804, 0x08);//
	os04e10_write_register(ViPipe, 0x3805, 0x0f);//
	os04e10_write_register(ViPipe, 0x3806, 0x08);//
	os04e10_write_register(ViPipe, 0x3807, 0x0f);//
	os04e10_write_register(ViPipe, 0x3808, 0x08);//
	os04e10_write_register(ViPipe, 0x3809, 0x00);//
	os04e10_write_register(ViPipe, 0x380a, 0x08);//
	os04e10_write_register(ViPipe, 0x380b, 0x00);//
	os04e10_write_register(ViPipe, 0x380c, 0x04);//
	os04e10_write_register(ViPipe, 0x380d, 0xd1);//
	os04e10_write_register(ViPipe, 0x380e, 0x08);//
	os04e10_write_register(ViPipe, 0x380f, 0x50);//
	os04e10_write_register(ViPipe, 0x3810, 0x00);//
	os04e10_write_register(ViPipe, 0x3811, 0x08);//
	os04e10_write_register(ViPipe, 0x3812, 0x00);//
	os04e10_write_register(ViPipe, 0x3813, 0x08);//
	os04e10_write_register(ViPipe, 0x3814, 0x01);//
	os04e10_write_register(ViPipe, 0x3815, 0x01);//
	os04e10_write_register(ViPipe, 0x3816, 0x01);//
	os04e10_write_register(ViPipe, 0x3817, 0x01);//
	os04e10_write_register(ViPipe, 0x3818, 0x00);//
	os04e10_write_register(ViPipe, 0x3819, 0x00);//
	os04e10_write_register(ViPipe, 0x381a, 0x00);//
	os04e10_write_register(ViPipe, 0x381b, 0x01);//
	os04e10_write_register(ViPipe, 0x3820, 0x88);//
	os04e10_write_register(ViPipe, 0x3821, 0x04);//
	os04e10_write_register(ViPipe, 0x3822, 0x14);//
	os04e10_write_register(ViPipe, 0x3823, 0x08);//
	os04e10_write_register(ViPipe, 0x3824, 0x00);//
	os04e10_write_register(ViPipe, 0x3825, 0x20);//
	os04e10_write_register(ViPipe, 0x3826, 0x00);//
	os04e10_write_register(ViPipe, 0x3827, 0x08);//
	os04e10_write_register(ViPipe, 0x3829, 0x03);//
	os04e10_write_register(ViPipe, 0x382a, 0x00);//
	os04e10_write_register(ViPipe, 0x382b, 0x00);//
	os04e10_write_register(ViPipe, 0x3832, 0x08);//
	os04e10_write_register(ViPipe, 0x3838, 0x00);//
	os04e10_write_register(ViPipe, 0x3839, 0x00);//
	os04e10_write_register(ViPipe, 0x383a, 0x00);//
	os04e10_write_register(ViPipe, 0x383b, 0x00);//
	os04e10_write_register(ViPipe, 0x383d, 0x01);//
	os04e10_write_register(ViPipe, 0x383e, 0x00);//
	os04e10_write_register(ViPipe, 0x383f, 0x00);//
	os04e10_write_register(ViPipe, 0x3843, 0x00);//
	os04e10_write_register(ViPipe, 0x3848, 0x08);//
	os04e10_write_register(ViPipe, 0x3849, 0x00);//
	os04e10_write_register(ViPipe, 0x384a, 0x08);//
	os04e10_write_register(ViPipe, 0x384b, 0x00);//
	os04e10_write_register(ViPipe, 0x384c, 0x00);//
	os04e10_write_register(ViPipe, 0x384d, 0x08);//
	os04e10_write_register(ViPipe, 0x384e, 0x00);//
	os04e10_write_register(ViPipe, 0x384f, 0x08);//
	os04e10_write_register(ViPipe, 0x3880, 0x16);//
	os04e10_write_register(ViPipe, 0x3881, 0x00);//
	os04e10_write_register(ViPipe, 0x3882, 0x08);//
	os04e10_write_register(ViPipe, 0x388a, 0x00);//
	os04e10_write_register(ViPipe, 0x389a, 0x00);//
	os04e10_write_register(ViPipe, 0x389b, 0x00);//
	os04e10_write_register(ViPipe, 0x389c, 0x00);//
	os04e10_write_register(ViPipe, 0x38a2, 0x01);//
	os04e10_write_register(ViPipe, 0x38a3, 0x01);//
	os04e10_write_register(ViPipe, 0x38a4, 0x01);//
	os04e10_write_register(ViPipe, 0x38a5, 0x01);//
	os04e10_write_register(ViPipe, 0x38a7, 0x04);//
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);//
	os04e10_write_register(ViPipe, 0x38b8, 0x02);//
	os04e10_write_register(ViPipe, 0x38c3, 0x06);//
	os04e10_write_register(ViPipe, 0x3c80, 0x3e);//
	os04e10_write_register(ViPipe, 0x3c86, 0x01);//
	os04e10_write_register(ViPipe, 0x3c87, 0x02);//
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);//
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);//
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);//
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);//
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);//
	os04e10_write_register(ViPipe, 0x3f04, 0x04);//
	os04e10_write_register(ViPipe, 0x3f07, 0x04);//
	os04e10_write_register(ViPipe, 0x3f09, 0x50);//
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);//
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);//
	os04e10_write_register(ViPipe, 0x4000, 0xf3);//
	os04e10_write_register(ViPipe, 0x4002, 0x00);//
	os04e10_write_register(ViPipe, 0x4003, 0x40);//
	os04e10_write_register(ViPipe, 0x4008, 0x00);//
	os04e10_write_register(ViPipe, 0x4009, 0x0f);//
	os04e10_write_register(ViPipe, 0x400a, 0x01);//
	os04e10_write_register(ViPipe, 0x400b, 0x78);//
	os04e10_write_register(ViPipe, 0x400f, 0x89);//
	os04e10_write_register(ViPipe, 0x4040, 0x00);//
	os04e10_write_register(ViPipe, 0x4041, 0x07);//
	os04e10_write_register(ViPipe, 0x4090, 0x14);//
	os04e10_write_register(ViPipe, 0x40b0, 0x00);//
	os04e10_write_register(ViPipe, 0x40b1, 0x00);//
	os04e10_write_register(ViPipe, 0x40b2, 0x00);//
	os04e10_write_register(ViPipe, 0x40b3, 0x00);//
	os04e10_write_register(ViPipe, 0x40b4, 0x00);//
	os04e10_write_register(ViPipe, 0x40b5, 0x00);//
	os04e10_write_register(ViPipe, 0x40b7, 0x00);//
	os04e10_write_register(ViPipe, 0x40b8, 0x00);//
	os04e10_write_register(ViPipe, 0x40b9, 0x00);//
	os04e10_write_register(ViPipe, 0x40ba, 0x01);//
	os04e10_write_register(ViPipe, 0x4300, 0xff);//
	os04e10_write_register(ViPipe, 0x4301, 0x00);//
	os04e10_write_register(ViPipe, 0x4302, 0x0f);//
	os04e10_write_register(ViPipe, 0x4303, 0x01);//
	os04e10_write_register(ViPipe, 0x4304, 0x01);//
	os04e10_write_register(ViPipe, 0x4305, 0x83);//
	os04e10_write_register(ViPipe, 0x4306, 0x21);//
	os04e10_write_register(ViPipe, 0x430d, 0x00);//
	os04e10_write_register(ViPipe, 0x4501, 0x00);//
	os04e10_write_register(ViPipe, 0x4505, 0xc4);//
	os04e10_write_register(ViPipe, 0x4506, 0x00);//
	os04e10_write_register(ViPipe, 0x4507, 0x43);//
	os04e10_write_register(ViPipe, 0x4508, 0x00);//
	os04e10_write_register(ViPipe, 0x4600, 0x00);//
	os04e10_write_register(ViPipe, 0x4601, 0x40);//
	os04e10_write_register(ViPipe, 0x4603, 0x03);//
	os04e10_write_register(ViPipe, 0x4800, 0x64);//
	os04e10_write_register(ViPipe, 0x4803, 0x00);//
	os04e10_write_register(ViPipe, 0x4809, 0x8e);//
	os04e10_write_register(ViPipe, 0x480e, 0x04);//
	os04e10_write_register(ViPipe, 0x4813, 0xe4);//
	os04e10_write_register(ViPipe, 0x4814, 0x2a);//
	os04e10_write_register(ViPipe, 0x481b, 0x3c);//
	os04e10_write_register(ViPipe, 0x481f, 0x26);//
	os04e10_write_register(ViPipe, 0x4825, 0x32);//
	os04e10_write_register(ViPipe, 0x4829, 0x64);//
	os04e10_write_register(ViPipe, 0x4837, 0x11);//
	os04e10_write_register(ViPipe, 0x484b, 0x27);//
	os04e10_write_register(ViPipe, 0x4883, 0x36);//
	os04e10_write_register(ViPipe, 0x4885, 0x03);//
	os04e10_write_register(ViPipe, 0x488b, 0x00);//
	os04e10_write_register(ViPipe, 0x4d00, 0x04);//
	os04e10_write_register(ViPipe, 0x4d01, 0x99);//
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);//
	os04e10_write_register(ViPipe, 0x4d03, 0xac);//
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);//
	os04e10_write_register(ViPipe, 0x4d05, 0x54);//
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);//
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);//
	os04e10_write_register(ViPipe, 0x5000, 0xbb);//
	os04e10_write_register(ViPipe, 0x5001, 0x09);//
	os04e10_write_register(ViPipe, 0x5004, 0x00);//
	os04e10_write_register(ViPipe, 0x5005, 0x0e);//
	os04e10_write_register(ViPipe, 0x5036, 0x80);//
	os04e10_write_register(ViPipe, 0x5080, 0x04);//
	os04e10_write_register(ViPipe, 0x5082, 0x00);//
	os04e10_write_register(ViPipe, 0x5180, 0x70);//
	os04e10_write_register(ViPipe, 0x5181, 0x10);//
	os04e10_write_register(ViPipe, 0x5182, 0x71);//
	os04e10_write_register(ViPipe, 0x5183, 0xdf);//
	os04e10_write_register(ViPipe, 0x5184, 0x02);//
	os04e10_write_register(ViPipe, 0x5185, 0x6c);//
	os04e10_write_register(ViPipe, 0x5189, 0x48);//
	os04e10_write_register(ViPipe, 0x5324, 0x09);//
	os04e10_write_register(ViPipe, 0x5325, 0x11);//
	os04e10_write_register(ViPipe, 0x5326, 0x1f);//
	os04e10_write_register(ViPipe, 0x5327, 0x3b);//
	os04e10_write_register(ViPipe, 0x5328, 0x49);//
	os04e10_write_register(ViPipe, 0x5329, 0x61);//
	os04e10_write_register(ViPipe, 0x532a, 0x9c);//
	os04e10_write_register(ViPipe, 0x532b, 0xc9);//
	os04e10_write_register(ViPipe, 0x5335, 0x04);//
	os04e10_write_register(ViPipe, 0x5336, 0x00);//
	os04e10_write_register(ViPipe, 0x5337, 0x04);//
	os04e10_write_register(ViPipe, 0x5338, 0x00);//
	os04e10_write_register(ViPipe, 0x5339, 0x0b);//
	os04e10_write_register(ViPipe, 0x533a, 0x00);//
	os04e10_write_register(ViPipe, 0x53a4, 0x09);//
	os04e10_write_register(ViPipe, 0x53a5, 0x11);//
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);//
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);//
	os04e10_write_register(ViPipe, 0x53a8, 0x49);//
	os04e10_write_register(ViPipe, 0x53a9, 0x61);//
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);//
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);//
	os04e10_write_register(ViPipe, 0x53b5, 0x04);//
	os04e10_write_register(ViPipe, 0x53b6, 0x00);//
	os04e10_write_register(ViPipe, 0x53b7, 0x04);//
	os04e10_write_register(ViPipe, 0x53b8, 0x00);//
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);//
	os04e10_write_register(ViPipe, 0x53ba, 0x00);//
	os04e10_write_register(ViPipe, 0x580b, 0x03);//
	os04e10_write_register(ViPipe, 0x580d, 0x00);//
	os04e10_write_register(ViPipe, 0x580f, 0x00);//
	os04e10_write_register(ViPipe, 0x5820, 0x00);//
	os04e10_write_register(ViPipe, 0x5821, 0x00);//
	os04e10_write_register(ViPipe, 0x5888, 0x01);//
	os04e10_write_register(ViPipe, 0x3002, 0x23);// ; [1] vsync_oen, [0]: fsin_oen
	os04e10_write_register(ViPipe, 0x3690, 0x00);// ; [4]: 1'b0, 1st set vsync pulse
	os04e10_write_register(ViPipe, 0x383e, 0x00);// ; vscyn_rise_rcnt_pt[23:16]
	os04e10_write_register(ViPipe, 0x3818, 0x00);// ; Slave vsync pulse position cs [15:8]
	os04e10_write_register(ViPipe, 0x3819, 0x00);// ; Slave vsync pulse position cs [7:0],max is HTS/4
	os04e10_write_register(ViPipe, 0x381a, 0x00);// ; vscyn_rise_rcnt_pt[15:8]
	os04e10_write_register(ViPipe, 0x381b, 0x00);// ; vscyn_rise_rcnt_pt[7:0], max: VTS-12 for AHBIN 720p, (VTS -12)*2 for other formats
	os04e10_write_register(ViPipe, 0x3832, 0x20);// ; default, 8'h08, [7:4] vsync pulse width
	os04e10_write_register(ViPipe, 0x368a, 0x04);// ; GPIO enable

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit 2to1 WDR 2L MASTER Init OK!===\n", ViPipe);
}

static void os04e10_wdr_2048X2048_30_2to1_2L_SLAVE_init(VI_PIPE ViPipe)
{
	os04e10_write_register(ViPipe, 0x0103, 0x01);
	os04e10_write_register(ViPipe, 0x0301, 0x44);//
	os04e10_write_register(ViPipe, 0x0303, 0x02);//
	os04e10_write_register(ViPipe, 0x0304, 0x00);//
	os04e10_write_register(ViPipe, 0x0305, 0x50);//
	os04e10_write_register(ViPipe, 0x0306, 0x00);//
	os04e10_write_register(ViPipe, 0x0325, 0x3b);//
	os04e10_write_register(ViPipe, 0x0327, 0x04);//
	os04e10_write_register(ViPipe, 0x0328, 0x05);//
	os04e10_write_register(ViPipe, 0x3002, 0x21);//
	os04e10_write_register(ViPipe, 0x3016, 0x32);//
	os04e10_write_register(ViPipe, 0x301b, 0xf0);//
	os04e10_write_register(ViPipe, 0x301e, 0xb4);//
	os04e10_write_register(ViPipe, 0x301f, 0xf0);//
	os04e10_write_register(ViPipe, 0x3021, 0x03);//
	os04e10_write_register(ViPipe, 0x3022, 0x01);//
	os04e10_write_register(ViPipe, 0x3107, 0xa1);//
	os04e10_write_register(ViPipe, 0x3108, 0x7d);//
	os04e10_write_register(ViPipe, 0x3109, 0xfc);//
	os04e10_write_register(ViPipe, 0x3500, 0x00);//
	os04e10_write_register(ViPipe, 0x3501, 0x04);//
	os04e10_write_register(ViPipe, 0x3502, 0x2a);//
	os04e10_write_register(ViPipe, 0x3503, 0x88);//
	os04e10_write_register(ViPipe, 0x3508, 0x01);//
	os04e10_write_register(ViPipe, 0x3509, 0x00);//
	os04e10_write_register(ViPipe, 0x350a, 0x04);//
	os04e10_write_register(ViPipe, 0x350b, 0x00);//
	os04e10_write_register(ViPipe, 0x350c, 0x04);//
	os04e10_write_register(ViPipe, 0x350d, 0x00);//
	os04e10_write_register(ViPipe, 0x350e, 0x04);//
	os04e10_write_register(ViPipe, 0x350f, 0x00);//
	os04e10_write_register(ViPipe, 0x3510, 0x00);//
	os04e10_write_register(ViPipe, 0x3511, 0x00);//
	os04e10_write_register(ViPipe, 0x3512, 0x20);//
	os04e10_write_register(ViPipe, 0x3600, 0x4c);//
	os04e10_write_register(ViPipe, 0x3601, 0x08);//
	os04e10_write_register(ViPipe, 0x3610, 0x87);//
	os04e10_write_register(ViPipe, 0x3611, 0x24);//
	os04e10_write_register(ViPipe, 0x3614, 0x4c);//
	os04e10_write_register(ViPipe, 0x3620, 0x0c);//
	os04e10_write_register(ViPipe, 0x3621, 0x04);//
	os04e10_write_register(ViPipe, 0x3632, 0x80);//
	os04e10_write_register(ViPipe, 0x3633, 0x00);//
	os04e10_write_register(ViPipe, 0x3660, 0x04);//
	os04e10_write_register(ViPipe, 0x3662, 0x10);//
	os04e10_write_register(ViPipe, 0x3664, 0x70);//
	os04e10_write_register(ViPipe, 0x3665, 0x00);//
	os04e10_write_register(ViPipe, 0x3666, 0x00);//
	os04e10_write_register(ViPipe, 0x3667, 0x00);//
	os04e10_write_register(ViPipe, 0x366a, 0x54);//
	os04e10_write_register(ViPipe, 0x3670, 0x0b);//
	os04e10_write_register(ViPipe, 0x3671, 0x0b);//
	os04e10_write_register(ViPipe, 0x3672, 0x0b);//
	os04e10_write_register(ViPipe, 0x3673, 0x0b);//
	os04e10_write_register(ViPipe, 0x3674, 0x00);//
	os04e10_write_register(ViPipe, 0x3678, 0x2b);//
	os04e10_write_register(ViPipe, 0x3679, 0x43);//
	os04e10_write_register(ViPipe, 0x3681, 0xff);//
	os04e10_write_register(ViPipe, 0x3682, 0x86);//
	os04e10_write_register(ViPipe, 0x3683, 0x44);//
	os04e10_write_register(ViPipe, 0x3684, 0x24);//
	os04e10_write_register(ViPipe, 0x3685, 0x00);//
	os04e10_write_register(ViPipe, 0x368a, 0x00);//
	os04e10_write_register(ViPipe, 0x368d, 0x2b);//
	os04e10_write_register(ViPipe, 0x368e, 0x6b);//
	os04e10_write_register(ViPipe, 0x3690, 0x00);//
	os04e10_write_register(ViPipe, 0x3691, 0x0b);//
	os04e10_write_register(ViPipe, 0x3692, 0x0b);//
	os04e10_write_register(ViPipe, 0x3693, 0x0b);//
	os04e10_write_register(ViPipe, 0x3694, 0x0b);//
	os04e10_write_register(ViPipe, 0x3699, 0x03);//
	os04e10_write_register(ViPipe, 0x369d, 0x68);//
	os04e10_write_register(ViPipe, 0x369e, 0x34);//
	os04e10_write_register(ViPipe, 0x369f, 0x1b);//
	os04e10_write_register(ViPipe, 0x36a0, 0x0f);//
	os04e10_write_register(ViPipe, 0x36a1, 0x77);//
	os04e10_write_register(ViPipe, 0x36a2, 0xf0);//
	os04e10_write_register(ViPipe, 0x36a3, 0x82);//
	os04e10_write_register(ViPipe, 0x36a4, 0x82);//
	os04e10_write_register(ViPipe, 0x36b0, 0x30);//
	os04e10_write_register(ViPipe, 0x36b1, 0xf0);//
	os04e10_write_register(ViPipe, 0x36b2, 0x00);//
	os04e10_write_register(ViPipe, 0x36b3, 0x00);//
	os04e10_write_register(ViPipe, 0x36b4, 0x00);//
	os04e10_write_register(ViPipe, 0x36b5, 0x00);//
	os04e10_write_register(ViPipe, 0x36b6, 0x00);//
	os04e10_write_register(ViPipe, 0x36b7, 0x00);//
	os04e10_write_register(ViPipe, 0x36b8, 0x00);//
	os04e10_write_register(ViPipe, 0x36b9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ba, 0x00);//
	os04e10_write_register(ViPipe, 0x36bb, 0x00);//
	os04e10_write_register(ViPipe, 0x36bc, 0x00);//
	os04e10_write_register(ViPipe, 0x36bd, 0x00);//
	os04e10_write_register(ViPipe, 0x36be, 0x00);//
	os04e10_write_register(ViPipe, 0x36bf, 0x00);//
	os04e10_write_register(ViPipe, 0x36c0, 0x1f);//
	os04e10_write_register(ViPipe, 0x36c1, 0x00);//
	os04e10_write_register(ViPipe, 0x36c2, 0x00);//
	os04e10_write_register(ViPipe, 0x36c3, 0x00);//
	os04e10_write_register(ViPipe, 0x36c4, 0x00);//
	os04e10_write_register(ViPipe, 0x36c5, 0x00);//
	os04e10_write_register(ViPipe, 0x36c6, 0x00);//
	os04e10_write_register(ViPipe, 0x36c7, 0x00);//
	os04e10_write_register(ViPipe, 0x36c8, 0x00);//
	os04e10_write_register(ViPipe, 0x36c9, 0x00);//
	os04e10_write_register(ViPipe, 0x36ca, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cb, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cc, 0x0e);//
	os04e10_write_register(ViPipe, 0x36cd, 0x0e);//
	os04e10_write_register(ViPipe, 0x36ce, 0x0c);//
	os04e10_write_register(ViPipe, 0x36cf, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d0, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d1, 0x0c);//
	os04e10_write_register(ViPipe, 0x36d2, 0x00);//
	os04e10_write_register(ViPipe, 0x36d3, 0x08);//
	os04e10_write_register(ViPipe, 0x36d4, 0x10);//
	os04e10_write_register(ViPipe, 0x36d5, 0x10);//
	os04e10_write_register(ViPipe, 0x36d6, 0x00);//
	os04e10_write_register(ViPipe, 0x36d7, 0x08);//
	os04e10_write_register(ViPipe, 0x36d8, 0x10);//
	os04e10_write_register(ViPipe, 0x36d9, 0x10);//
	os04e10_write_register(ViPipe, 0x3704, 0x01);//
	os04e10_write_register(ViPipe, 0x3705, 0x00);//
	os04e10_write_register(ViPipe, 0x3706, 0x2b);//
	os04e10_write_register(ViPipe, 0x3709, 0x46);//
	os04e10_write_register(ViPipe, 0x370a, 0x00);//
	os04e10_write_register(ViPipe, 0x370b, 0x60);//
	os04e10_write_register(ViPipe, 0x370e, 0x0c);//
	os04e10_write_register(ViPipe, 0x370f, 0x1c);//
	os04e10_write_register(ViPipe, 0x3710, 0x00);//
	os04e10_write_register(ViPipe, 0x3713, 0x00);//
	os04e10_write_register(ViPipe, 0x3714, 0x24);//
	os04e10_write_register(ViPipe, 0x3716, 0x24);//
	os04e10_write_register(ViPipe, 0x371a, 0x1e);//
	os04e10_write_register(ViPipe, 0x3724, 0x0d);//
	os04e10_write_register(ViPipe, 0x3725, 0xb2);//
	os04e10_write_register(ViPipe, 0x372b, 0x54);//
	os04e10_write_register(ViPipe, 0x3739, 0x10);//
	os04e10_write_register(ViPipe, 0x373f, 0xb0);//
	os04e10_write_register(ViPipe, 0x3740, 0x2b);//
	os04e10_write_register(ViPipe, 0x3741, 0x2b);//
	os04e10_write_register(ViPipe, 0x3742, 0x2b);//
	os04e10_write_register(ViPipe, 0x3743, 0x2b);//
	os04e10_write_register(ViPipe, 0x3744, 0x60);//
	os04e10_write_register(ViPipe, 0x3745, 0x60);//
	os04e10_write_register(ViPipe, 0x3746, 0x60);//
	os04e10_write_register(ViPipe, 0x3747, 0x60);//
	os04e10_write_register(ViPipe, 0x3748, 0x00);//
	os04e10_write_register(ViPipe, 0x3749, 0x00);//
	os04e10_write_register(ViPipe, 0x374a, 0x00);//
	os04e10_write_register(ViPipe, 0x374b, 0x00);//
	os04e10_write_register(ViPipe, 0x374c, 0x00);//
	os04e10_write_register(ViPipe, 0x374d, 0x00);//
	os04e10_write_register(ViPipe, 0x374e, 0x00);//
	os04e10_write_register(ViPipe, 0x374f, 0x00);//
	os04e10_write_register(ViPipe, 0x3756, 0x00);//
	os04e10_write_register(ViPipe, 0x3757, 0x00);//
	os04e10_write_register(ViPipe, 0x3760, 0x22);//
	os04e10_write_register(ViPipe, 0x3767, 0x08);//
	os04e10_write_register(ViPipe, 0x3773, 0x01);//
	os04e10_write_register(ViPipe, 0x3774, 0x02);//
	os04e10_write_register(ViPipe, 0x3775, 0x12);//
	os04e10_write_register(ViPipe, 0x3776, 0x02);//
	os04e10_write_register(ViPipe, 0x377b, 0x4a);//
	os04e10_write_register(ViPipe, 0x377c, 0x00);//
	os04e10_write_register(ViPipe, 0x377d, 0x0c);//
	os04e10_write_register(ViPipe, 0x3782, 0x01);//
	os04e10_write_register(ViPipe, 0x3787, 0x14);//
	os04e10_write_register(ViPipe, 0x3795, 0x14);//
	os04e10_write_register(ViPipe, 0x3796, 0x01);//
	os04e10_write_register(ViPipe, 0x3798, 0x40);//
	os04e10_write_register(ViPipe, 0x379c, 0x00);//
	os04e10_write_register(ViPipe, 0x379d, 0x00);//
	os04e10_write_register(ViPipe, 0x379e, 0x00);//
	os04e10_write_register(ViPipe, 0x379f, 0x01);//
	os04e10_write_register(ViPipe, 0x37a1, 0x10);//
	os04e10_write_register(ViPipe, 0x37a6, 0x00);//
	os04e10_write_register(ViPipe, 0x37ac, 0xa0);//
	os04e10_write_register(ViPipe, 0x37bb, 0x01);//
	os04e10_write_register(ViPipe, 0x37be, 0x0a);//
	os04e10_write_register(ViPipe, 0x37bf, 0x0a);//
	os04e10_write_register(ViPipe, 0x37c2, 0x04);//
	os04e10_write_register(ViPipe, 0x37c4, 0x11);//
	os04e10_write_register(ViPipe, 0x37c5, 0x80);//
	os04e10_write_register(ViPipe, 0x37c6, 0x14);//
	os04e10_write_register(ViPipe, 0x37c7, 0x08);//
	os04e10_write_register(ViPipe, 0x37c8, 0x42);//
	os04e10_write_register(ViPipe, 0x37cd, 0x17);//
	os04e10_write_register(ViPipe, 0x37ce, 0x04);//
	os04e10_write_register(ViPipe, 0x37d9, 0x08);//
	os04e10_write_register(ViPipe, 0x37dc, 0x01);//
	os04e10_write_register(ViPipe, 0x37e0, 0x30);//
	os04e10_write_register(ViPipe, 0x37e1, 0x10);//
	os04e10_write_register(ViPipe, 0x37e2, 0x14);//
	os04e10_write_register(ViPipe, 0x37e4, 0x28);//
	os04e10_write_register(ViPipe, 0x37ef, 0x00);//
	os04e10_write_register(ViPipe, 0x37f4, 0x00);//
	os04e10_write_register(ViPipe, 0x37f5, 0x00);//
	os04e10_write_register(ViPipe, 0x37f6, 0x00);//
	os04e10_write_register(ViPipe, 0x37f7, 0x00);//
	os04e10_write_register(ViPipe, 0x3800, 0x00);//
	os04e10_write_register(ViPipe, 0x3801, 0x00);//
	os04e10_write_register(ViPipe, 0x3802, 0x00);//
	os04e10_write_register(ViPipe, 0x3803, 0x00);//
	os04e10_write_register(ViPipe, 0x3804, 0x08);//
	os04e10_write_register(ViPipe, 0x3805, 0x0f);//
	os04e10_write_register(ViPipe, 0x3806, 0x08);//
	os04e10_write_register(ViPipe, 0x3807, 0x0f);//
	os04e10_write_register(ViPipe, 0x3808, 0x08);//
	os04e10_write_register(ViPipe, 0x3809, 0x00);//
	os04e10_write_register(ViPipe, 0x380a, 0x08);//
	os04e10_write_register(ViPipe, 0x380b, 0x00);//
	os04e10_write_register(ViPipe, 0x380c, 0x04);//
	os04e10_write_register(ViPipe, 0x380d, 0xd1);//
	os04e10_write_register(ViPipe, 0x380e, 0x08);//
	os04e10_write_register(ViPipe, 0x380f, 0x50);//
	os04e10_write_register(ViPipe, 0x3810, 0x00);//
	os04e10_write_register(ViPipe, 0x3811, 0x08);//
	os04e10_write_register(ViPipe, 0x3812, 0x00);//
	os04e10_write_register(ViPipe, 0x3813, 0x08);//
	os04e10_write_register(ViPipe, 0x3814, 0x01);//
	os04e10_write_register(ViPipe, 0x3815, 0x01);//
	os04e10_write_register(ViPipe, 0x3816, 0x01);//
	os04e10_write_register(ViPipe, 0x3817, 0x01);//
	os04e10_write_register(ViPipe, 0x3818, 0x00);//
	os04e10_write_register(ViPipe, 0x3819, 0x00);//
	os04e10_write_register(ViPipe, 0x381a, 0x00);//
	os04e10_write_register(ViPipe, 0x381b, 0x01);//
	os04e10_write_register(ViPipe, 0x3820, 0x88);//
	os04e10_write_register(ViPipe, 0x3821, 0x04);//
	os04e10_write_register(ViPipe, 0x3822, 0x14);//
	os04e10_write_register(ViPipe, 0x3823, 0x08);//
	os04e10_write_register(ViPipe, 0x3824, 0x00);//
	os04e10_write_register(ViPipe, 0x3825, 0x20);//
	os04e10_write_register(ViPipe, 0x3826, 0x00);//
	os04e10_write_register(ViPipe, 0x3827, 0x08);//
	os04e10_write_register(ViPipe, 0x3829, 0x03);//
	os04e10_write_register(ViPipe, 0x382a, 0x00);//
	os04e10_write_register(ViPipe, 0x382b, 0x00);//
	os04e10_write_register(ViPipe, 0x3832, 0x08);//
	os04e10_write_register(ViPipe, 0x3838, 0x00);//
	os04e10_write_register(ViPipe, 0x3839, 0x00);//
	os04e10_write_register(ViPipe, 0x383a, 0x00);//
	os04e10_write_register(ViPipe, 0x383b, 0x00);//
	os04e10_write_register(ViPipe, 0x383d, 0x01);//
	os04e10_write_register(ViPipe, 0x383e, 0x00);//
	os04e10_write_register(ViPipe, 0x383f, 0x00);//
	os04e10_write_register(ViPipe, 0x3843, 0x00);//
	os04e10_write_register(ViPipe, 0x3848, 0x08);//
	os04e10_write_register(ViPipe, 0x3849, 0x00);//
	os04e10_write_register(ViPipe, 0x384a, 0x08);//
	os04e10_write_register(ViPipe, 0x384b, 0x00);//
	os04e10_write_register(ViPipe, 0x384c, 0x00);//
	os04e10_write_register(ViPipe, 0x384d, 0x08);//
	os04e10_write_register(ViPipe, 0x384e, 0x00);//
	os04e10_write_register(ViPipe, 0x384f, 0x08);//
	os04e10_write_register(ViPipe, 0x3880, 0x16);//
	os04e10_write_register(ViPipe, 0x3881, 0x00);//
	os04e10_write_register(ViPipe, 0x3882, 0x08);//
	os04e10_write_register(ViPipe, 0x388a, 0x00);//
	os04e10_write_register(ViPipe, 0x389a, 0x00);//
	os04e10_write_register(ViPipe, 0x389b, 0x00);//
	os04e10_write_register(ViPipe, 0x389c, 0x00);//
	os04e10_write_register(ViPipe, 0x38a2, 0x01);//
	os04e10_write_register(ViPipe, 0x38a3, 0x01);//
	os04e10_write_register(ViPipe, 0x38a4, 0x01);//
	os04e10_write_register(ViPipe, 0x38a5, 0x01);//
	os04e10_write_register(ViPipe, 0x38a7, 0x04);//
	os04e10_write_register(ViPipe, 0x38ae, 0x1e);//
	os04e10_write_register(ViPipe, 0x38b8, 0x02);//
	os04e10_write_register(ViPipe, 0x38c3, 0x06);//
	os04e10_write_register(ViPipe, 0x3c80, 0x3e);//
	os04e10_write_register(ViPipe, 0x3c86, 0x01);//
	os04e10_write_register(ViPipe, 0x3c87, 0x02);//
	os04e10_write_register(ViPipe, 0x3ca0, 0x01);//
	os04e10_write_register(ViPipe, 0x3ca2, 0x0c);//
	os04e10_write_register(ViPipe, 0x3d8c, 0x71);//
	os04e10_write_register(ViPipe, 0x3d8d, 0xe2);//
	os04e10_write_register(ViPipe, 0x3f00, 0xcb);//
	os04e10_write_register(ViPipe, 0x3f04, 0x04);//
	os04e10_write_register(ViPipe, 0x3f07, 0x04);//
	os04e10_write_register(ViPipe, 0x3f09, 0x50);//
	os04e10_write_register(ViPipe, 0x3f9e, 0x07);//
	os04e10_write_register(ViPipe, 0x3f9f, 0x04);//
	os04e10_write_register(ViPipe, 0x4000, 0xf3);//
	os04e10_write_register(ViPipe, 0x4002, 0x00);//
	os04e10_write_register(ViPipe, 0x4003, 0x40);//
	os04e10_write_register(ViPipe, 0x4008, 0x00);//
	os04e10_write_register(ViPipe, 0x4009, 0x0f);//
	os04e10_write_register(ViPipe, 0x400a, 0x01);//
	os04e10_write_register(ViPipe, 0x400b, 0x78);//
	os04e10_write_register(ViPipe, 0x400f, 0x89);//
	os04e10_write_register(ViPipe, 0x4040, 0x00);//
	os04e10_write_register(ViPipe, 0x4041, 0x07);//
	os04e10_write_register(ViPipe, 0x4090, 0x14);//
	os04e10_write_register(ViPipe, 0x40b0, 0x00);//
	os04e10_write_register(ViPipe, 0x40b1, 0x00);//
	os04e10_write_register(ViPipe, 0x40b2, 0x00);//
	os04e10_write_register(ViPipe, 0x40b3, 0x00);//
	os04e10_write_register(ViPipe, 0x40b4, 0x00);//
	os04e10_write_register(ViPipe, 0x40b5, 0x00);//
	os04e10_write_register(ViPipe, 0x40b7, 0x00);//
	os04e10_write_register(ViPipe, 0x40b8, 0x00);//
	os04e10_write_register(ViPipe, 0x40b9, 0x00);//
	os04e10_write_register(ViPipe, 0x40ba, 0x01);//
	os04e10_write_register(ViPipe, 0x4300, 0xff);//
	os04e10_write_register(ViPipe, 0x4301, 0x00);//
	os04e10_write_register(ViPipe, 0x4302, 0x0f);//
	os04e10_write_register(ViPipe, 0x4303, 0x01);//
	os04e10_write_register(ViPipe, 0x4304, 0x01);//
	os04e10_write_register(ViPipe, 0x4305, 0x83);//
	os04e10_write_register(ViPipe, 0x4306, 0x21);//
	os04e10_write_register(ViPipe, 0x430d, 0x00);//
	os04e10_write_register(ViPipe, 0x4501, 0x00);//
	os04e10_write_register(ViPipe, 0x4505, 0xc4);//
	os04e10_write_register(ViPipe, 0x4506, 0x00);//
	os04e10_write_register(ViPipe, 0x4507, 0x43);//
	os04e10_write_register(ViPipe, 0x4508, 0x00);//
	os04e10_write_register(ViPipe, 0x4600, 0x00);//
	os04e10_write_register(ViPipe, 0x4601, 0x40);//
	os04e10_write_register(ViPipe, 0x4603, 0x03);//
	os04e10_write_register(ViPipe, 0x4800, 0x64);//
	os04e10_write_register(ViPipe, 0x4803, 0x00);//
	os04e10_write_register(ViPipe, 0x4809, 0x8e);//
	os04e10_write_register(ViPipe, 0x480e, 0x04);//
	os04e10_write_register(ViPipe, 0x4813, 0xe4);//
	os04e10_write_register(ViPipe, 0x4814, 0x2a);//
	os04e10_write_register(ViPipe, 0x481b, 0x3c);//
	os04e10_write_register(ViPipe, 0x481f, 0x26);//
	os04e10_write_register(ViPipe, 0x4825, 0x32);//
	os04e10_write_register(ViPipe, 0x4829, 0x64);//
	os04e10_write_register(ViPipe, 0x4837, 0x11);//
	os04e10_write_register(ViPipe, 0x484b, 0x27);//
	os04e10_write_register(ViPipe, 0x4883, 0x36);//
	os04e10_write_register(ViPipe, 0x4885, 0x03);//
	os04e10_write_register(ViPipe, 0x488b, 0x00);//
	os04e10_write_register(ViPipe, 0x4d00, 0x04);//
	os04e10_write_register(ViPipe, 0x4d01, 0x99);//
	os04e10_write_register(ViPipe, 0x4d02, 0xbd);//
	os04e10_write_register(ViPipe, 0x4d03, 0xac);//
	os04e10_write_register(ViPipe, 0x4d04, 0xf2);//
	os04e10_write_register(ViPipe, 0x4d05, 0x54);//
	os04e10_write_register(ViPipe, 0x4e00, 0x2a);//
	os04e10_write_register(ViPipe, 0x4e0d, 0x00);//
	os04e10_write_register(ViPipe, 0x5000, 0xbb);//
	os04e10_write_register(ViPipe, 0x5001, 0x09);//
	os04e10_write_register(ViPipe, 0x5004, 0x00);//
	os04e10_write_register(ViPipe, 0x5005, 0x0e);//
	os04e10_write_register(ViPipe, 0x5036, 0x80);//
	os04e10_write_register(ViPipe, 0x5080, 0x04);//
	os04e10_write_register(ViPipe, 0x5082, 0x00);//
	os04e10_write_register(ViPipe, 0x5180, 0x70);//
	os04e10_write_register(ViPipe, 0x5181, 0x10);//
	os04e10_write_register(ViPipe, 0x5182, 0x71);//
	os04e10_write_register(ViPipe, 0x5183, 0xdf);//
	os04e10_write_register(ViPipe, 0x5184, 0x02);//
	os04e10_write_register(ViPipe, 0x5185, 0x6c);//
	os04e10_write_register(ViPipe, 0x5189, 0x48);//
	os04e10_write_register(ViPipe, 0x5324, 0x09);//
	os04e10_write_register(ViPipe, 0x5325, 0x11);//
	os04e10_write_register(ViPipe, 0x5326, 0x1f);//
	os04e10_write_register(ViPipe, 0x5327, 0x3b);//
	os04e10_write_register(ViPipe, 0x5328, 0x49);//
	os04e10_write_register(ViPipe, 0x5329, 0x61);//
	os04e10_write_register(ViPipe, 0x532a, 0x9c);//
	os04e10_write_register(ViPipe, 0x532b, 0xc9);//
	os04e10_write_register(ViPipe, 0x5335, 0x04);//
	os04e10_write_register(ViPipe, 0x5336, 0x00);//
	os04e10_write_register(ViPipe, 0x5337, 0x04);//
	os04e10_write_register(ViPipe, 0x5338, 0x00);//
	os04e10_write_register(ViPipe, 0x5339, 0x0b);//
	os04e10_write_register(ViPipe, 0x533a, 0x00);//
	os04e10_write_register(ViPipe, 0x53a4, 0x09);//
	os04e10_write_register(ViPipe, 0x53a5, 0x11);//
	os04e10_write_register(ViPipe, 0x53a6, 0x1f);//
	os04e10_write_register(ViPipe, 0x53a7, 0x3b);//
	os04e10_write_register(ViPipe, 0x53a8, 0x49);//
	os04e10_write_register(ViPipe, 0x53a9, 0x61);//
	os04e10_write_register(ViPipe, 0x53aa, 0x9c);//
	os04e10_write_register(ViPipe, 0x53ab, 0xc9);//
	os04e10_write_register(ViPipe, 0x53b5, 0x04);//
	os04e10_write_register(ViPipe, 0x53b6, 0x00);//
	os04e10_write_register(ViPipe, 0x53b7, 0x04);//
	os04e10_write_register(ViPipe, 0x53b8, 0x00);//
	os04e10_write_register(ViPipe, 0x53b9, 0x0b);//
	os04e10_write_register(ViPipe, 0x53ba, 0x00);//
	os04e10_write_register(ViPipe, 0x580b, 0x03);//
	os04e10_write_register(ViPipe, 0x580d, 0x00);//
	os04e10_write_register(ViPipe, 0x580f, 0x00);//
	os04e10_write_register(ViPipe, 0x5820, 0x00);//
	os04e10_write_register(ViPipe, 0x5821, 0x00);//
	os04e10_write_register(ViPipe, 0x5888, 0x01);//
	os04e10_write_register(ViPipe, 0x3002, 0x22);//
	os04e10_write_register(ViPipe, 0x3663, 0x22);// ; [1] vsync_oen, [0]: fsin_oen
	os04e10_write_register(ViPipe, 0x3823, 0x00);// ; [2] fsin pad disable
	os04e10_write_register(ViPipe, 0x3822, 0x44);// ; [3:0]r_ext_vsync_div, SYNC every frame
	os04e10_write_register(ViPipe, 0x3832, 0x20);// ; [6]: ext_vs_adj_vts mode enable [5]: fix_cnt_en
	os04e10_write_register(ViPipe, 0x368a, 0x04);// ; [7:4] vsync pulse width
	os04e10_write_register(ViPipe, 0x3829, 0x03);// ; GPIO enable
	os04e10_write_register(ViPipe, 0x3844, 0x06);// ; [5:4]: vts_adj_threshold = 0, other bits follow base setting
	os04e10_write_register(ViPipe, 0x3843, 0x00);// ; threshold_vts_sub[7:0]
	os04e10_write_register(ViPipe, 0x382a, 0x00);// ; man_vts_adj_val[23:16]
	os04e10_write_register(ViPipe, 0x382b, 0x00);// ; man_vts_adj_val[15:8]

	os04e10_default_reg_init(ViPipe);
	os04e10_write_register(ViPipe, 0x0100, 0x01);
	usleep(150 * 1000);

	printf("ViPipe:%d,===OS04E10 2048x2048 30fps 10bit 2to1 WDR 2L SLAVE Init OK!===\n", ViPipe);
}