#ifndef _CTP_DETECT_H_
#define _CTP_DETECT_H_

struct ctp_device
{
	char * name;			//0.IC\C3\FB\B3\C6
	char * ko_name; 		//1.ko\C3\FB\B3\C6
	bool need_detect;		//2.\CAǷ\F1ɨ\C3\E8
	unsigned int i2c_addr;	//3.i2c\B5\D8ַ
	bool has_chipid;		//4.\D3\D0chipid
	unsigned int chipid_req;//5.chipid\BCĴ\E6\C6\F7
	unsigned int chipid;	//6.chipid
};

//ÿ\D4\F6\BC\D3һ\BF\EEIC\A3\AC\CD\F9\D5\E2\B8\F6\C1б\ED\C0\EF\CC\ED\BC\D3
struct ctp_device ctp_device_list[]=
{
//ע\D2\E2\C8\E7\B9\FB\C1\BD\B8\F6ic\B5\C4i2c\B5\D8ַ\CF\E0ͬ\A3\AC\B0\D1\D3\D0chipid\B5ķ\C5\D4\DAǰ\C3档
	//ICN83XX
	{
		"ICN83XX",			//0.IC\C3\FB\B3\C6
		"ctp_icn83xx.ko", //"ctp_icn838x_ts_iic.ko"	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x40,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0x0a,				//5.chipid\BCĴ\E6\C6\F7
		0x83,				//6.chipid
	},
	//GSL1680,GSL3670,GSL3680,\D2\F2\D5\E2\C8\FD\BF\EEͨ\B5\C0\CA\FD\B2\BBͬ\A3\ACһ\B0㲻\BB\E1\BB\EC\D3ã\AC
	//\C7\D21680\D3\D0D\B0\E6E\B0棬3680\D3\D0A\B0\E6B\B0棬chipid\BE\F9\B2\BBͬ\A3\ACȫ\B2\BFɨ\C3\E8̫\C2鷳\A3\AC\D5\E2\C0ﲻ\D4\D9\C7\F8\B7֡\A3
	{
		"GSLX6X0",			//0.IC\C3\FB\B3\C6
		"ctp_gslX680.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x40,				//3.i2c\B5\D8ַ
		false,				//4.\D3\D0chipid
		0xfc,					//5.chipid\BCĴ\E6\C6\F7
		0x0,//1680:0x0			//6.chipid
	},
	//FT5206,FT5406
	{
		"FT52-406",			//0.IC\C3\FB\B3\C6
		"ctp_ft5X06.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x38,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0xA3,				//5.chipid\BCĴ\E6\C6\F7
		0x55,				//6.chipid
	},
	//FT5606
	{
		"FT5606",			//0.IC\C3\FB\B3\C6
		"ctp_ft5X06.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x38,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0xA3,				//5.chipid\BCĴ\E6\C6\F7
		0x08,				//6.chipid
	},
	//GT813
	{
		"GT813",			//0.IC\C3\FB\B3\C6
		"ctp_goodix_touch.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x5d,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0xf7d,				//5.chipid\BCĴ\E6\C6\F7
		0x13,				//6.chipid
	},
	//AW5206
	{
		"AW5206",			//0.IC\C3\FB\B3\C6
		"ctp_aw5306.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x38,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0x01,				//5.chipid\BCĴ\E6\C6\F7
		0xA8,				//6.chipid
	},
	//AW5209
	{
		"AW5209",			//0.IC\C3\FB\B3\C6
		"ctp_aw5209.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x38,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0x01,				//5.chipid\BCĴ\E6\C6\F7
		0xB8,				//6.chipid
	},
	//CT36X
	{
		"CT36X",			//0.IC\C3\FB\B3\C6
		"ctp_ct36x_i2c_ts.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x01,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0x00,//???				//5.chipid\BCĴ\E6\C6\F7
		0x00,//0x02:CT360,0x01:CT363,CT365	//6.chipid
	},
	//HL3X06
	{
		"HL3X06",			//0.IC\C3\FB\B3\C6
		"ctp_hl3x06.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x3B,				//3.i2c\B5\D8ַ
		false,				//4.\D3\D0chipid
		0x00,				//5.chipid\BCĴ\E6\C6\F7
		0x30,				//6.chipid
	},
	//ILITEK
	{
		"ILITEK",			//0.IC\C3\FB\B3\C6
		"ctp_ilitek_aimvF.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x41,				//3.i2c\B5\D8ַ
		false,	//ȷʵ\CE\DE			//4.\D3\D0chipid
		0,				//5.chipid\BCĴ\E6\C6\F7
		0,				//6.chipid
	},
	//ili2672
	{
		"ili2672",			//0.IC\C3\FB\B3\C6
		"ctp_ili2672.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x41,				//3.i2c\B5\D8ַ
		false,	//ȷʵ\CE\DE			//4.\D3\D0chipid
		0,				//5.chipid\BCĴ\E6\C6\F7
		0,				//6.chipid
	},
	//ft5x06
	{
		"ft5x06",			//0.IC\C3\FB\B3\C6
		"ctp_ft5x06.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x38,				//3.i2c\B5\D8ַ
		false,	//ȷʵ\CE\DE			//4.\D3\D0chipid
		0,				//5.chipid\BCĴ\E6\C6\F7
		0,				//6.chipid
	},	
	//MT395
	{
		"MT395",			//0.IC\C3\FB\B3\C6
		"ctp_mt395.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x67,				//3.i2c\B5\D8ַ
		false,				//4.\D3\D0chipid
		0,				//5.chipid\BCĴ\E6\C6\F7
		0,				//6.chipid
	},	
	//NOVATEK
	{
		"NT1100X",			//0.IC\C3\FB\B3\C6
		"ctp_Novatek_TouchDriver.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x01,				//3.i2c\B5\D8ַ
		false,				//4.\D3\D0chipid
		0x00,	//???		//5.chipid\BCĴ\E6\C6\F7
		0,	//???			//6.chipid
	},	
	//SSD254X
	{
		"SSD254X",			//0.IC\C3\FB\B3\C6
		"ctp_ssd254x.ko",	//1.ko\C3\FB\B3\C6
		true,				//2.\CAǷ\F1ɨ\C3\E8
		0x48,				//3.i2c\B5\D8ַ
		true,				//4.\D3\D0chipid
		0x02,					//5.chipid\BCĴ\E6\C6\F7
		0x25,	//0x2541,0x2543			//6.chipid
	},		
};


#endif
