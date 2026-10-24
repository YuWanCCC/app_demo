/*
 *  Copyright (C) 2021, Fuzhou Rockchip Electronics Co., Ltd.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of Fuzhou Rockchip Electronics Co., Ltd. nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SKV_LIB_DEFINE_
#define _SKV_LIB_DEFINE_

#define INFO_RIGHT 1            // Succeed initialize SKV preprocessor
#define INFO_PARAM_ERROR 2      // Cann't support the parameter of audio that you input
#define INFO_EXCEEDDATE_ERROR 3 // Exceed the limited date
#define INFO_UNKNOW_ERROR 4     // Unknown error

#define FRAME_SIZE 256
#define FRAME_SIZE_PBNLMS 256
#define NUM_PBNLMS_FILTER 4

typedef enum _SkvAECEnable {
	AEC_EN_DELAY = 1 << 0,
	AEC_EN_CHN_MAP = 1 << 1,
} SkvAecEnable;

typedef enum _SkvBFEnable {
	BF_EN_FAST_AEC = 1 << 0,
	BF_EN_WAKEUP = 1 << 1,
	BF_EN_DEREV = 1 << 2,
	BF_EN_NLP = 1 << 3,
	BF_EN_AES = 1 << 4,
	BF_EN_AGC = 1 << 5,
	BF_EN_ANR = 1 << 6,
	BF_EN_GSC = 1 << 7,
	BF_GSC_METHOD = 1 << 8,
	BF_EN_FIX = 1 << 9,
	BF_EN_STDT = 1 << 10,
	BF_EN_CNG = 1 << 11,
	BF_EN_EQ = 1 << 12,
	BF_EN_CHN_SELECT = 1 << 13,
	BF_EN_HOWLING = 1 << 14,
	BF_EN_DOA = 1 << 15,
	BF_EN_WIND = 1 << 16,
	BF_EN_AINR = 1 << 17,
	BF_EN_FILTER = 1 << 18,
} SkvBFEnable;

typedef enum _SkvLiteEnable {
	LITE_EN_ANR = 1 << 0,
} SkvLiteEnable;

typedef enum _SkvRxEnable {
	RX_EN_ANR = 1 << 0,
	RX_EN_HOWLING = 1 << 1,
	RX_EN_AGC = 1 << 2,
	RX_EN_FILTER = 1 << 3,
} SkvRxEnable;

typedef struct _SkvDelayParam {
	short MaxFrame;
	short LeastDelay;
	short JumpFrame;
	short DelayOffset;
	short MicAmpThr;
	short RefAmpThr;
	short StartFreq;
	short EndFreq;
	float SmoothFactor;
} SkvDelayParam;

typedef struct _SkvAecParam {
	int pos;
	int drop_ref_channel;
	int model_aec_en;
	int delay_len;
	int look_ahead;
	short *Array_list;
	short int filter_len;
	SkvDelayParam *delay;
} SkvAecParam;

typedef struct _SkvAnrParam {
	float noiseFactor;
	int swU;
	float psiMin;
	float psiMax;
	float fGmin;

	short Sup_Freq1;
	short Sup_Freq2;
	float Sup_Energy1;
	float Sup_Energy2;

	short InterV;
	float BiasMin;
	short UpdateFrm;
	float NPreGammaThr;
	float NPreZetaThr;
	float SabsGammaThr0;
	float SabsGammaThr1;
	float InfSmooth;
	float ProbSmooth;
	float CompCoeff;
	float PrioriMin;
	float PostMax;
	float PrioriRatio;
	float PrioriRatioLow;
	int SplitBand;
	float PrioriSmooth;
	short TranMode; // transient mode
	int MotorBins;
	float MotorFactor;
	short MotorMute;
} SkvAnrParam;

typedef struct _SkvAgcParam {
	float attack_time;
	float release_time;
	float max_gain;
	float max_peak;
	float fRth0;
	float fRk0;
	float fRth1;

	int fs;
	int frmlen;
	float attenuate_time;
	float fRth2;
	float fRk1;
	float fRk2;
	float fLineGainDb;
	int swSmL0;
	int swSmL1;
	int swSmL2;
} SkvAgcParam;

typedef struct _SkvDereverbParam {
	int rlsLg;
	int curveLg;
	int delay;
	float forgetting;
	float t60;
	float coCoeff;
} SkvDereverbParam;

typedef struct _SkvNlpParam {
	short nlp16k[8][2]; // obsolete but kept
} SkvNlpParam;

typedef struct _SkvCngParam {
	float fGain;
	float fMpy;
	float fSmoothAlpha;
	float fSpeechGain;
} SkvCngParam;

typedef struct _SkvDtdParam {
	float ksiThd_high;
	float ksiThd_low;
} SkvDtdParam;

typedef struct _SkvAesParam {
	float beta_up;
	float beta_down;
	float beta_up_low;
	float beta_down_low;
	short low_freq;
	short high_freq;
	short thd_flag;
	short hard_flag;
	float LimitRatio[2][3];
	short ThdSplitFreq[4][2];
	float ThdSupDegree[4][10];
	short HardSplitFreq[5][2];
	float HardThreshold[4];
} SkvAesParam;

typedef struct _SkvEqParam {
	int shwParaLen;
	short pfCoeff[5][13];
} SkvEqParam;

typedef struct _SkvHowlParam {
	short mode;
} SkvHowlParam;

typedef struct _SkvFilterParam {
	/* basic params */
	int type;
	float post_gain;
	/* master filter params */
	int mode;
	float fc;
	float bw;
	float gain;
	float slope;
	/* combined and post filter params */
	int mode1;
	float fc1;
	float bw1;
	float gain1;
	float slope1;
} SkvFilterParam;

typedef struct _SkvDoaParam {
	float rad;
	short start_freq;
	short end_freq;
	short lg_num;
	short lg_pitch_num;
} SkvDoaParam;

typedef struct _SkvWakeupParam {
	int mode;          // 0: oneshot, 1: wakeup + command
	int wakeup_frames; // (default: 180) the frame counts of keeping wakeup and waiting
	                   // command word
	int wakeup_words;  // the max of wakeup word, which is >= 1
	char *model_word1; // wakeup word
	char *model_word2; // command word
} SkvWakeupParam;

typedef struct _SkvAiNRParam {
	short mode;       // [1, 2]
	short datrs_optm; // [0, 1, 2]
	float pregain;    // pregain before sent to ainr
	float mini_gain;  // environment voice level. 0~1
	float alpha1;     // smooth (speech ascend alpha)
	float alpha2;     // smooth (speech descend alpha)
	float preserve1;  // if mode==1, used as noise threshold
	float preserve2;  // if mode==1, used as speech threshold
	float preserve3;  // if mode==1, used as noise ascend alpha
	float preserve4;  // if mode==1, used as noise descend alpha
	//
	short sfs_enable;
	int Sup_Freq1;
	int Sup_Freq2;
	float Sup_Energy1;
	float Sup_Energy2;
	//
	short eq_enable;
	short speech_eq;
	int eq_frqs_hz[11];
	float eq_gains[11];
	char *model_path; // AI-NR model
} SkvAiNRParam;

typedef struct _SkvBeamFormParam {
	int model_en;
	int ref_pos;
	int targ;
	int num_ref_channel;
	int drop_ref_channel;

	SkvDereverbParam *dereverb;
	SkvAesParam *aes;
	SkvNlpParam *nlp;
	SkvAnrParam *anr;
	SkvAgcParam *agc;
	SkvCngParam *cng;
	SkvDtdParam *dtd;
	SkvEqParam *eq;
	SkvHowlParam *howl;
	SkvDoaParam *doa;
	SkvAiNRParam *ainr;
	SkvFilterParam *filter;
	SkvWakeupParam *wkp; // NOTE: rockit only, there is no param in algorithm
} SkvBeamFormParam;

#if 0 // TODO FOR SKVLITE
typedef struct _SkvLiteParam
{
    int model_en;
    SkvAnrParam *anr;
} SkvLiteParam;
#endif

typedef struct _SkvRxParam {
	/* Parameters of agc */
	int model_en;
	SkvAnrParam *anr;
	SkvHowlParam *howl;
	SkvAgcParam *agc;
	SkvFilterParam *filter;
} SkvRxParam;

typedef struct _SkvResult {
	int mWakeupStatus;
	int mWakeupMode; /* 0: none; 1: oneshot; 2: wakeup+command; 3: oneshot and
	                    wakeup+command */
	int mWakeupCmdID;
	int mWakeupCmdScore;
} SkvResult;
#endif // _SKV_LIB_DEFINE_
