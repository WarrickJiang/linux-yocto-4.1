#ifndef __BISP_DRV__
#define __BISP_DRV__

typedef struct{
	int chid;
	int offset1;
	int offset2;
	int offset3;
	int y_r;
	int y_g;
	int y_b;
	int cb_r;
	int cb_g;
	int cb_b;
	int cr_r;
	int cr_g;
	int cr_b;
}bisp_csc_t;


typedef struct{
	int chid;
	int lsc_en;
	int vinc;
	int hinc;
	int xcenter;
	int ycenter;
	int xcenter_1;
	int scaleidx;
	unsigned int lsc_coeff[64];
}bisp_lsc_t;

typedef struct{
	int chid;
	short nr_en;
	short nr_level;
	short dpc_en;
	short dpc_level;
	int ee_en;
}bisp_nr_t;

typedef struct{
	int chid;
	unsigned int sx;
  unsigned int ex;
  unsigned int sy;
  unsigned int ey;
}bisp_region_t;

typedef struct{
	int chid;
	unsigned int bgain;
	unsigned int ggain;
	unsigned int rgain;
	unsigned int stat_addr;
	unsigned int stat_vddr;
}bisp_bg_t;

typedef struct {
	int chid;
	int wb_thr;
	int wb_thg;
	int wb_thb;
	unsigned int wb_cnt;
}bisp_wp_t;

typedef struct{
	int chid;
	int gc_en;
	unsigned int gc_coeff[16];
}bisp_gc_t;

typedef struct{
	int chid;
	int ow;
	int oh;
	int ofmt;
}bisp_outinfo_t;

typedef struct{
	int chid;
	int color_seq;
	int hsync;
	int vsync;
	int isp_mode;//not care here
	int ch_IF;//parallel/mipi
	int stat_en;
	int eis_en;
}bisp_info_t;

typedef struct{
	int chid;
	int cb;
	int cr;
	int yinc;
}bisp_color_t;


typedef struct{
	int hbank;
	int vbank;
}bisp_bank_t;


typedef struct{
	int chid;
	int b_en;
	unsigned int color_info[16];
}bisp_color_adjust_t;

typedef struct{
	int b_en;
	int vnc_num;
	unsigned int color_info[3][128];
}bisp_vnc_t;

typedef struct{
	int chid;
	unsigned int ads;
	unsigned int vds;
	unsigned int last_addr;
}bisp_ads_t;

typedef struct{
	int chid;
	unsigned int buffer_num;//<16
	unsigned int pHyaddr[16];
	unsigned int pViaddr[16];
}bisp_stat_buffers_info_t;

typedef struct{
	//\CAǷ\F1\D3\D0Чͳ\BCƣ\AC1 true\A3\AC0 false
	int bPrepared;
	unsigned int pv[9];
}af_pv_t;
//\CB㷨\B2\CE\CA\FD\C9\E8\D6ã\ACͬ\BCĴ\E6\C6\F7
typedef struct{
	//ͼ\CF\F3\B5Ŀ\ED\B8\DF
	int win_w;
	int win_h;
	//\B5\F7\BD\B9\CB㷨
	int af_al;
	//\B2\C9\D1\F9\BE\AB\B6\C8
	int af_inc;
	//\D6ж\CFģʽ
	int af_mode;
	int rshift;
	int thold;
	int tshift;
	//\B4\B0\B8\F6\CA\FD-1
	int win_num;
	//\B4\B0\BFڴ\F3С
	int win_size;
	//\CF\E0\B6\D4λ\D6\C3
	unsigned int af_pos[9];
	unsigned int noise_th;
}af_param_t;

typedef struct{
	int r_off;
	int g1_off;
	int g2_off;
	int b_off;
}blc_info_t;
typedef struct{
	char buf[16];
	int  rev[4];
}module_name_t;
#define BRAWISP_DRV_IOC_MAGIC_NUMBER             'P'
//#define BRI_SCH         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x100) //not set ch
#define BRI_SEN         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x101,bisp_info_t)
#define BRI_COP         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x102,bisp_region_t)
#define BRI_VNC         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x103,bisp_vnc_t) // not support here
#define BRI_LUT         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x104,bisp_lsc_t)
#define BRI_NR          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x105,bisp_nr_t)
#define BRI_EE          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x106,bisp_nr_t)
#define BRI_DPC         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x107,bisp_nr_t)
#define BRI_GN          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x108,bisp_bg_t)
#define BRI_SRG         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x109,bisp_region_t)
#define BRI_SHG         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10A,bisp_region_t)
#define BRI_WBP         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10B,bisp_wp_t)
#define BRI_CSC         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10C,bisp_csc_t)
#define BRI_CRP         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10D,bisp_color_adjust_t)
#define BRI_CFX         _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10E,bisp_color_t)
#define BRI_GC          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x10F,bisp_gc_t)
#define BRI_HVB          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x110,bisp_bank_t)
#define BRI_SBS          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x120,bisp_stat_buffers_info_t)
#define BRI_FTB          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x121,bisp_ads_t)
#define BRI_SID          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x122,int)
#define BRI_BLC          _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x123,blc_info_t)
//\CD\EA\B3\C9irq\BA\CDenable\C9\E8\D6\C3
#define AF_CMD_ENABLE          _IO(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x1000)
//disableirq\BA\CDenable
#define AF_CMD_DISABLE         _IO(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x1001)
//\BB\F1ȡͳ\BC\C6\D0\C5Ϣ
#define AF_CMD_GET_INFO        _IOR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x1002,af_pv_t)
//\C9\E8\D6ô\B0\BFں\CD\CB㷨\B2\CE\CA\FD\A3\AC\C9\E8\D6\C3һ\B4Σ\AC\D4\CB\D0\D0\D6\D0\C9\E8\D6ã\AC\D0\E8Ҫ\B5ȴ\FDһ֡
#define AF_CMD_SET_WINPS     	 _IOW(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x1003,af_param_t)

#define BRI_GET_SDS     _IOWR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x200,bisp_ads_t)
#define BRI_GET_YDS     _IOWR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x201,bisp_ads_t)
#define BRI_GET_CRP     _IOWR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x202,bisp_region_t)
#define BRI_GET_GN     _IOWR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x203,bisp_bg_t)
#define BRI_GET_MODULE_NAME     _IOWR(BRAWISP_DRV_IOC_MAGIC_NUMBER, 0x204,module_name_t)

#endif
