#ifndef __SC4336P_SLAVE_CMOS_EX_H_
#define __SC4336P_SLAVE_CMOS_EX_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef ARCH_CV182X
#include <linux/cvi_vip_cif.h>
#include <linux/cvi_vip_snsr.h>
#include "cvi_type.h"
#else
#include <linux/cif_uapi.h>
#include <linux/vi_snsr.h>
#include <linux/cvi_type.h>
#endif
#include "cvi_sns_ctrl.h"


enum sc4336p_slave_linear_regs_e {
	LINEAR_SHS1_0_ADDR,
	LINEAR_SHS1_1_ADDR,
	LINEAR_SHS1_2_ADDR,
	LINEAR_AGAIN_ADDR,
	LINEAR_DGAIN_ADDR,
	LINEAR_D_FINEGAIN_ADDR,
	LINEAR_VMAX_0_ADDR,
	LINEAR_VMAX_1_ADDR,
	LINEAR_REGS_NUM
};

typedef enum _SC4336P_SLAVE_MODE_E {
	SC4336P_SLAVE_MODE_1440P30 = 0,
	SC4336P_SLAVE_MODE_NUM
} SC4336P_SLAVE_MODE_E;

typedef struct _SC4336P_SLAVE_MODE_S {
	ISP_WDR_SIZE_S astImg[2];
	CVI_FLOAT f32MaxFps;
	CVI_FLOAT f32MinFps;
	CVI_U32 u32HtsDef;
	CVI_U32 u32VtsDef;
	SNS_ATTR_LARGE_S stExp[2];
	SNS_ATTR_LARGE_S stAgain[2];
	SNS_ATTR_LARGE_S stDgain[2];
	char name[64];
} SC4336P_SLAVE_MODE_S;

/****************************************************************************
 * external variables and functions                                         *
 ****************************************************************************/

extern ISP_SNS_STATE_S *g_pastSC4336P_SLAVE[VI_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U g_aunSC4336P_SLAVE_BusInfo[];
extern ISP_SNS_COMMADDR_U g_aunSc4336p_slave_AddrInfo[];
extern CVI_U16 g_au16SC4336P_SLAVE_GainMode[];
extern CVI_U16 g_au16SC4336P_SLAVE_L2SMode[];
extern CVI_U8 sc4336p_slave_i2c_addr;
extern const CVI_U32 sc4336p_slave_addr_byte;
extern const CVI_U32 sc4336p_slave_data_byte;
extern void sc4336p_slave_init(VI_PIPE ViPipe);
extern void sc4336p_slave_exit(VI_PIPE ViPipe);
extern int sc4336p_slave_i2c_exit(VI_PIPE ViPipe);
extern void sc4336p_slave_standby(VI_PIPE ViPipe);
extern void sc4336p_slave_restart(VI_PIPE ViPipe);
extern int  sc4336p_slave_write_register(VI_PIPE ViPipe, int addr, int data);
extern int  sc4336p_slave_read_register(VI_PIPE ViPipe, int addr);
extern void sc4336p_slave_mirror_flip(VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eSnsMirrorFlip);
extern int  sc4336p_slave_probe(VI_PIPE ViPipe);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* __SC4336P_SLAVE_CMOS_EX_H_ */
