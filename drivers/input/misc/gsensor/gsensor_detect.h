#ifndef _GSENSOR_DETECT_H_
#define _GSENSOR_DETECT_H_

struct gsensor_device
{
    char * name;            //0.IC\C3\FB\B3\C6
    char * ko_name;         //1.ko\C3\FB\B3\C6
    bool has_sa0;             //2.\D3\D0sa0 pin
    unsigned char i2c_addr;    //3.i2c\B5\D8ַ
    bool has_chipid;        //4.\D3\D0chipid
    unsigned char chipid_reg; //5.chipid\BCĴ\E6\C6\F7
    unsigned char chipid[2];   //6.chipid 
    bool need_detect;	     //7.\CAǷ\F1ɨ\C3\E8
};

//ÿ\D4\F6\BC\D3һ\BF\EEIC\A3\AC\CD\F9\D5\E2\B8\F6\C1б\ED\C0\EF\CC\ED\BC\D3
//ע\D2\E2\C8\E7\B9\FB\C1\BD\B8\F6ic\B5\C4i2c\B5\D8ַ\CF\E0ͬ\A3\AC\B0\D1\D3\D0chipid\B5ķ\C5\D4\DAǰ\C3档
struct gsensor_device gsensor_device_list[]=
{
    // AFA750
    {
        "afa750",                    //0.IC\C3\FB\B3\C6
        "gsensor_afa750.ko",    //1.ko\C3\FB\B3\C6
        true,                           //2.\D3\D0sa0 pin
        0x3c,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x37,                          //5.chipid\BCĴ\E6\C6\F7
        {0x3c, 0x3d},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    // BMA220
    {
        "bma220",                    //0.IC\C3\FB\B3\C6
        "gsensor_bma220.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x0a,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0xdd, 0xdd},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // BMA222 bma223
    {
        "bma222",                    //0.IC\C3\FB\B3\C6
        "gsensor_bma222.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x18,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x02, 0xf8},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // BMA250/BMA250E
    {
        "bma250",                    //0.IC\C3\FB\B3\C6
        "gsensor_bma250.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x18,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x03, 0xf9},              //6.chipid
        //true,                           //7.\CAǷ\F1ɨ\C3\E8
        false,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    // DMARD10
    {
        "dmard10",                    //0.IC\C3\FB\B3\C6
        "gsensor_dmard10.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x18,                          //3.i2c\B5\D8ַ
        false,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x00, 0x00},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // kxtj9-1007
    {
        "kxtj9",                    //0.IC\C3\FB\B3\C6
        "gsensor_kionix_accel.ko",    //1.ko\C3\FB\B3\C6
        true,                           //2.\D3\D0sa0 pin
        0x0e,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x0f,                          //5.chipid\BCĴ\E6\C6\F7
        {0x08, 0x08},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // lis3dh
    {
        "lis3dh",                    //0.IC\C3\FB\B3\C6
        "gsensor_lis3dh_acc.ko",    //1.ko\C3\FB\B3\C6
        true,                           //2.\D3\D0sa0 pin
        0x18,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x0f,                          //5.chipid\BCĴ\E6\C6\F7
        {0x33, 0x33},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // mc3210
    {
        "mc3210",                    //0.IC\C3\FB\B3\C6
        "gsensor_mc3210.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x4c,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x3b,                          //5.chipid\BCĴ\E6\C6\F7
        {0x90, 0x90},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },

    // mc3232
    {
        "mc3232",                    //0.IC\C3\FB\B3\C6
        "gsensor_mc3232.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x4c,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x3b,                          //5.chipid\BCĴ\E6\C6\F7
        {0x19, 0x19},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },

    // mc3236
    {
        "mc3236",                    //0.IC\C3\FB\B3\C6
        "gsensor_mc3236.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x4c,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x3b,                          //5.chipid\BCĴ\E6\C6\F7
        {0x60, 0x60},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },

    // mma7660
    {
        "mma7660",                    //0.IC\C3\FB\B3\C6
        "gsensor_mma7660.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x4c,                          //3.i2c\B5\D8ַ
        false,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x00, 0x00},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // mma8452
    {
        "mma8452",                    //0.IC\C3\FB\B3\C6
        "gsensor_mma8452.ko",    //1.ko\C3\FB\B3\C6
        true,                           //2.\D3\D0sa0 pin
        0x1c,                          //3.i2c\B5\D8ַ
        true,                           //4.\D3\D0chipid
        0x0d,                          //5.chipid\BCĴ\E6\C6\F7
        {0x2a, 0x2a},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // stk8312
    {
        "stk8312",                    //0.IC\C3\FB\B3\C6
        "gsensor_stk8312.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x3d,                          //3.i2c\B5\D8ַ
        false,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x00, 0x00},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
    
    // stk8313
    {
        "stk8313",                    //0.IC\C3\FB\B3\C6
        "gsensor_stk8313.ko",    //1.ko\C3\FB\B3\C6
        false,                           //2.\D3\D0sa0 pin
        0x22,                          //3.i2c\B5\D8ַ
        false,                           //4.\D3\D0chipid
        0x00,                          //5.chipid\BCĴ\E6\C6\F7
        {0x00, 0x00},              //6.chipid
        true,                           //7.\CAǷ\F1ɨ\C3\E8
    },
};


#endif
