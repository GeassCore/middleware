#ifndef __GC2093_CMOS_PARAM_H_
#define __GC2093_CMOS_PARAM_H_

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
#include "gc2093_cmos_ex.h"

static const GC2093_MODE_S g_astGc2093_mode[GC2093_MODE_NUM] = {
	[GC2093_MODE_1920X1080P30] = {
		.name = "1920X1080P30",
		.astImg[0] = {
			.stSnsSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stMaxSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
		},
		.f32MaxFps = 30,
		.f32MinFps = 2.07, /* 1125 * 30 / 0x3FFF */
		.u32HtsDef = 2200,
		.u32VtsDef = 1125,
		.stExp[0] = {
			.u16Min = 1,
			.u16Max = 0x3fff,
			.u16Def = 0x2000,
			.u16Step = 1,
		},
		.stAgain[0] = {
			.u32Min = 64,
			.u32Max = 62977,
			.u32Def = 64,
			.u32Step = 1,
		},
		.stDgain[0] = {
			.u32Min = 64*16,
			.u32Max = 7073*16,
			.u32Def = 581*16,
			.u32Step = 10*16,
		},
	},
	[GC2093_MODE_1920X1080P60] = {
		.name = "1920X1080P60",
		.astImg[0] = {
			.stSnsSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stMaxSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
		},
		.f32MaxFps = 60,
		.f32MinFps = 2.07, /* 1125 * 30 / 0x3FFF */
		.u32HtsDef = 2200,
		.u32VtsDef = 1250,
		.stExp[0] = {
			.u16Min = 1,
			.u16Max = 0x3fff,
			.u16Def = 0x2000,
			.u16Step = 1,
		},
		.stAgain[0] = {
			.u32Min = 64,
			.u32Max = 62977,
			.u32Def = 64,
			.u32Step = 1,
		},
		.stDgain[0] = {
			.u32Min = 64*16,
			.u32Max = 7073*16,
			.u32Def = 581*16,
			.u32Step = 10*16,
		},
	},
	[GC2093_MODE_1920X1080P30_WDR] = {
		.name = "1920X1080P30_WDR",
		.astImg[0] = {
		/* sef */
			.stSnsSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stMaxSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
		},
		.astImg[1] = {
		/* lef */
			.stSnsSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 1920,
				.u32Height = 1080,
			},
			.stMaxSize = {
				.u32Width = 1920,
				.u32Height = 1080,
			},
		},
		.f32MaxFps = 30,
		.f32MinFps = 2.29, /* 1250 * 30 / 0x3FFF */
		.u32HtsDef = 2200,
		.u32VtsDef = 1250,
		.stAgain[0] = {
			.u32Min = 64,
			.u32Max = 62977,
			.u32Def = 64,
			.u32Step = 1,
		},
		.stAgain[1] = {
			.u32Min = 64,
			.u32Max = 62977,
			.u32Def = 64,
			.u32Step = 1,
		},
		.stDgain[0] = {
			.u32Min = 64*16,
			.u32Max = 7073*16,
			.u32Def = 581*16,
			.u32Step = 10*16,
		},
		.stDgain[1] = {
			.u32Min = 64*16,
			.u32Max = 7073*16,
			.u32Def = 581*16,
			.u32Step = 10*16,
		},
	},
};

static ISP_CMOS_NOISE_CALIBRATION_S g_stIspNoiseCalibratio = {.CalibrationCoef = {
	{	//iso  100
		{0.05999477580189704895,	0.13019448518753051758}, //B: slope, intercept
		{0.06732148677110671997,	-1.36387133598327636719}, //Gb: slope, intercept
		{0.06651904433965682983,	-1.10093510150909423828}, //Gr: slope, intercept
		{0.06406146287918090820,	0.33316791057586669922}, //R: slope, intercept
	},
	{	//iso  200
		{0.06256803125143051147,	4.54908418655395507813}, //B: slope, intercept
		{0.06911934912204742432,	2.79023528099060058594}, //Gb: slope, intercept
		{0.06846688687801361084,	2.88726186752319335938}, //Gr: slope, intercept
		{0.06652788817882537842,	4.40276956558227539063}, //R: slope, intercept
	},
	{	//iso  400
		{0.06841833144426345825,	11.72280883789062500000}, //B: slope, intercept
		{0.07257881015539169312,	10.86985683441162109375}, //Gb: slope, intercept
		{0.07174283266067504883,	11.20646286010742187500}, //Gr: slope, intercept
		{0.07294593751430511475,	11.17350578308105468750}, //R: slope, intercept
	},
	{	//iso  800
		{0.07805790752172470093,	20.62956619262695312500}, //B: slope, intercept
		{0.07694032043218612671,	22.20356750488281250000}, //Gb: slope, intercept
		{0.07647507637739181519,	22.50957298278808593750}, //Gr: slope, intercept
		{0.08402533829212188721,	19.11953735351562500000}, //R: slope, intercept
	},
	{	//iso  1600
		{0.09468275308609008789,	34.07563018798828125000}, //B: slope, intercept
		{0.08710632473230361938,	39.15500259399414062500}, //Gb: slope, intercept
		{0.08662072569131851196,	39.37175750732421875000}, //Gr: slope, intercept
		{0.10222808271646499634,	31.34789276123046875000}, //R: slope, intercept
	},
	{	//iso  3200
		{0.12651191651821136475,	49.56183242797851562500}, //B: slope, intercept
		{0.10816962271928787231,	59.42719650268554687500}, //Gb: slope, intercept
		{0.10751257836818695068,	59.90552902221679687500}, //Gr: slope, intercept
		{0.13802853226661682129,	45.09576034545898437500}, //R: slope, intercept
	},
	{	//iso  6400
		{0.17422541975975036621,	70.04063415527343750000}, //B: slope, intercept
		{0.14234761893749237061,	85.51583862304687500000}, //Gb: slope, intercept
		{0.14159946143627166748,	86.23278045654296875000}, //Gr: slope, intercept
		{0.19450971484184265137,	62.65447235107421875000}, //R: slope, intercept
	},
	{	//iso  12800
		{0.24947367608547210693,	108.30633544921875000000}, //B: slope, intercept
		{0.19751225411891937256,	130.88159179687500000000}, //Gb: slope, intercept
		{0.19614629447460174561,	132.49082946777343750000}, //Gr: slope, intercept
		{0.28106108307838439941,	97.15969085693359375000}, //R: slope, intercept
	},
	{	//iso  25600
		{0.35420843958854675293,	137.06745910644531250000}, //B: slope, intercept
		{0.27778801321983337402,	168.72366333007812500000}, //Gb: slope, intercept
		{0.27540388703346252441,	170.54939270019531250000}, //Gr: slope, intercept
		{0.39949953556060791016,	123.29409790039062500000}, //R: slope, intercept
	},
	{	//iso  51200
		{0.45704349875450134277,	179.20147705078125000000}, //B: slope, intercept
		{0.32142028212547302246,	246.71363830566406250000}, //Gb: slope, intercept
		{0.31958609819412231445,	246.82630920410156250000}, //Gr: slope, intercept
		{0.51058447360992431641,	161.86299133300781250000}, //R: slope, intercept
	},
	{	//iso  102400
		{0.61760461330413818359,	222.90534973144531250000}, //B: slope, intercept
		{0.42568457126617431641,	319.29257202148437500000}, //Gb: slope, intercept
		{0.41750904917716979980,	324.93432617187500000000}, //Gr: slope, intercept
		{0.67956107854843139648,	203.78948974609375000000}, //R: slope, intercept
	},
	{	//iso  204800
		{0.63289469480514526367,	216.99952697753906250000}, //B: slope, intercept
		{0.44890350103378295898,	306.80810546875000000000}, //Gb: slope, intercept
		{0.44229975342750549316,	310.13763427734375000000}, //Gr: slope, intercept
		{0.69596910476684570313,	196.70443725585937500000}, //R: slope, intercept
	},
	{	//iso  409600
		{0.71106964349746704102,	187.98352050781250000000}, //B: slope, intercept
		{0.55859673023223876953,	246.22378540039062500000}, //Gb: slope, intercept
		{0.55284017324447631836,	249.86463928222656250000}, //Gr: slope, intercept
		{0.77318203449249267578,	168.85035705566406250000}, //R: slope, intercept
	},
	{	//iso  819200
		{0.70888006687164306641,	188.44216918945312500000}, //B: slope, intercept
		{0.56110274791717529297,	245.46603393554687500000}, //Gb: slope, intercept
		{0.55100852251052856445,	250.33049011230468750000}, //Gr: slope, intercept
		{0.76897650957107543945,	169.31251525878906250000}, //R: slope, intercept
	},
	{	//iso  1638400
		{0.70520979166030883789,	188.93899536132812500000}, //B: slope, intercept
		{0.56178557872772216797,	245.21235656738281250000}, //Gb: slope, intercept
		{0.55338454246520996094,	249.57423400878906250000}, //Gr: slope, intercept
		{0.77306479215621948242,	168.86497497558593750000}, //R: slope, intercept
	},
	{	//iso  3276800
		{0.71255809068679809570,	187.86839294433593750000}, //B: slope, intercept
		{0.56056070327758789063,	245.57748413085937500000}, //Gb: slope, intercept
		{0.55358195304870605469,	249.62020874023437500000}, //Gr: slope, intercept
		{0.77431541681289672852,	168.74313354492187500000}, //R: slope, intercept
	},
} };

static ISP_CMOS_BLACK_LEVEL_S g_stIspBlcCalibratio = {
	.bUpdate = CVI_TRUE,
	.blcAttr = {
		.Enable = 1,
		.enOpType = OP_TYPE_AUTO,
		.stManual = {256, 256, 256, 256, 0, 0, 0, 0
#ifdef ARCH_CV182X
			, 1092, 1092, 1092, 1092
#endif
		},
		.stAuto = {
			{256, 257, 257, 257, 259, 259, 260, 267, 278, 298, 366, 383, 366, 373, 372, 372 },
			{256, 257, 257, 257, 258, 259, 261, 266, 274, 297, 379, 377, 372, 365, 373, 374 },
			{256, 257, 257, 257, 258, 259, 261, 266, 275, 296, 376, 388, 366, 374, 376, 372 },
			{256, 257, 257, 257, 258, 259, 260, 264, 274, 294, 362, 363, 365, 361, 353, 367 },
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
#ifdef ARCH_CV182X
			{1092, 1093, 1093, 1093, 1093, 1093, 1093, 1095, 1099, 1104, 1125, 1130, 1125, 1127,
				1126, 1126},
			{1092, 1093, 1093, 1093, 1093, 1093, 1094, 1095, 1097, 1104, 1128, 1128, 1126, 1124,
				1127, 1127},
			{1092, 1093, 1093, 1093, 1093, 1093, 1094, 1095, 1098, 1104, 1128, 1131, 1125, 1127,
				1128, 1126},
			{1092, 1093, 1093, 1093, 1093, 1093, 1093, 1095, 1097, 1103, 1123, 1124, 1124, 1123,
				1121, 1125},
#endif
		},
	},
};

struct combo_dev_attr_s gc2093_rx_attr = {
	.input_mode = INPUT_MODE_MIPI,
	.mac_clk = RX_MAC_CLK_200M,
	.mipi_attr = {
		.raw_data_type = RAW_DATA_10BIT,
		.lane_id = {1, 0, 2, -1, -1},
		.pn_swap = {1, 1, 1, 0, 0},
		.wdr_mode = CVI_MIPI_WDR_MODE_VC,
	},
	.mclk = {
		.cam = 0,
		.freq = CAMPLL_FREQ_27M,
	},
	.devno = 0,
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* __GC2093_CMOS_PARAM_H_ */

