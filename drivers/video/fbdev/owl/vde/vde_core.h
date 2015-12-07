#ifndef _VDE_CORE_H_
#define _VDE_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VDE_REG_NO
{
    VDE_REG0 = 0,    
    VDE_REG1 ,  VDE_REG2,  VDE_REG3,  VDE_REG4,  VDE_REG5,  VDE_REG6,  VDE_REG7,  VDE_REG8, 
    VDE_REG9 , VDE_REG10, VDE_REG11, VDE_REG12, VDE_REG13, VDE_REG14, VDE_REG15, VDE_REG16,
    VDE_REG17, VDE_REG18, VDE_REG19, VDE_REG20, VDE_REG21, VDE_REG22, VDE_REG23, VDE_REG24,
    VDE_REG25, VDE_REG26, VDE_REG27, VDE_REG28, VDE_REG29, VDE_REG30, VDE_REG31, VDE_REG32,
    VDE_REG33, VDE_REG34, VDE_REG35, VDE_REG36, VDE_REG37, VDE_REG38, VDE_REG39, VDE_REG40,          
    VDE_REG41 ,VDE_REG42, VDE_REG43, VDE_REG44, VDE_REG45, VDE_REG46, VDE_REG47, VDE_REG48,
    VDE_REG49, VDE_REG50, VDE_REG51, VDE_REG52, VDE_REG53, VDE_REG54, VDE_REG55, VDE_REG56,
    VDE_REG57, VDE_REG58, VDE_REG59, VDE_REG60, VDE_REG61, VDE_REG62, VDE_REG63, VDE_REG64,
    VDE_REG65, VDE_REG66, VDE_REG67, VDE_REG68, VDE_REG69, VDE_REG70, VDE_REG71, VDE_REG72,  
    VDE_REG73, VDE_REG74, VDE_REG75, VDE_REG76, VDE_REG77, VDE_REG78, VDE_REG79, VDE_REG80,
    VDE_REG81, VDE_REG82, VDE_REG83, VDE_REG84, VDE_REG85, VDE_REG86, VDE_REG87, VDE_REG88,
    VDE_REG89, VDE_REG90, VDE_REG91, VDE_REG92, VDE_REG93, VDE_REG94, VDE_REG_MAX
} VDE_RegNO_t;   


#define MAX_VDE_REG_NUM         (VDE_REG_MAX+1)


// \D7\F7Ϊһ\B8\F6backdoor, \CCṩ\B6\EE\CD\E2\B5Ĳ\CE\CA\FD\CA\E4\C8\EB\BDӿ\DA, ʹ\D3÷\BD\B7\A8\BA\CD\C5\E4\D6üĴ\E6\C6\F7\CE\DE\D2\EC
#define CODEC_CUSTOMIZE_ADDR            (VDE_REG_MAX)
#define CODEC_CUSTOMIZE_VALUE_PERFORMANCE  0x00000001
#define CODEC_CUSTOMIZE_VALUE_LOWPOWER     0x00000002
#define CODEC_CUSTOMIZE_VALUE_DROPFRAME    0x00000004
#define CODEC_CUSTOMIZE_VALUE_MAX          0xffffffff


typedef enum VDE_STATUS
{
    VDE_STATUS_IDLE                 = 0x1,   
    VDE_STATUS_READY_TO_RUN,                // \B5\B1ǰinstance\D2Ѿ\ADִ\D0\D0run, \B5\ABvde\B1\BB\C6\E4\CB\FBinstanceռ\D3\C3
    VDE_STATUS_RUNING,                      // \D5\FD\D4\DA\D4\CB\D0\D0
    VDE_STATUS_GOTFRAME,                    // \D3\D0֡\CA\E4\B3\F6
    VDE_STATUS_JPEG_SLICE_READY     = 0x100, // JPEG \BD\E2\C2\EBһ\B8\F6slice\CD\EA\B3\C9, \B4\CBʱ\B2\BB\C4ܱ\BB\C6\E4\CB\FBinstance\B4\F2\B6ϣ\ACֱ\B5\BDGOTFRAMEʱ\B2ſ\C9\D2Ա\BB\B4\F2\B6\CF
    VDE_STATUS_DIRECTMV_FULL,               // h264 Direct mv buffer\B2\BB\B9\BB\D3\C3,\D0\E8Ҫ\D6\D8\D0\C2\C9\EA\C7\EB\D4\D9\C6\F4\B6\AF\BD\E2\C2\EB    
    VDE_STATUS_STREAM_EMPTY,                // \C2\EB\C1\F7\CF\FB\BA\C4\CD꣬\D0\E8Ҫ\BC\CC\D0\F8\C5\E4\D6\C3\CA\FD\BE\DD\D4\D9\C6\F4\B6\AFVDE, 5202\B2\BB\D4\CA\D0\ED\B3\F6\CFִ\CB\C7\E9\BF\F6     
    VDE_STATUS_ASO_DETECTED,                // \BC\EC\B2⵽h264 ASO, \D0\E8Ҫ\C8\ED\BC\FE\D7\F6\C9ʽ\E2\C2\EB\D4\D9\C6\F4\B6\AFvde, 5202\B2\BB\D4\CA\D0\ED\B3\F6\CFִ\CB\C7\E9\BF\F6
    VDE_STATUS_TIMEOUT              = -1,   // timeout
    VDE_STATUS_STREAM_ERROR         = -2,   // \C2\EB\C1\F7\B3\F6\B4\ED        
    VDE_STATUS_BUS_ERROR            = -3,   // \B7\C3\CE\CAddr\B3\F6\B4\ED, \BF\C9\C4\DC\CA\C7\D2\F2Ϊ\C5\E4\D6õķ\C7\CE\EF\C0\ED\C1\AC\D0\F8\C4ڴ\E6
    VDE_STATUS_DEAD                 = -4,   // vpx\B9\D2\C1ˣ\AC\CE޷\A8\C5\E4\D6\C3\C8κμĴ\E6\C6\F7, video\D6м\E4\BC\FE\D0\E8Ҫ\B9ر\D5\CB\F9\D3\D0instance    
    VDE_STATUS_UNKNOWN_ERROR        = -0x100       // \C6\E4\CB\FB\B4\ED\CE\F3        
} VDE_Status_t;


typedef struct vde_handle 
{    
    // \B6\C1\BCĴ\E6\C6\F7
    unsigned int (*readReg)(struct vde_handle*, VDE_RegNO_t);
    
    // д\BCĴ\E6\C6\F7, ״̬\BCĴ\E6\C6\F7(reg1)\D3\C9\C7\FD\B6\AFͳһ\B9\DC\C0\ED, \B2\BB\C4\DCд, \B7\B5\BB\D8-1\A3\BB
    int (*writeReg)(struct vde_handle*, VDE_RegNO_t, const unsigned int);

    // \C6\F4\B6\AF\BD\E2\C2\EB, \B7\B5\BB\D8-1\A3\AC\B1\EDʾvde״̬\B4\ED\CE󣬲\BB\C4\DC\C6\F4\B6\AF;
    int (*run)(struct vde_handle*);
    
    // \B2\E9ѯVDE״̬\A3\AC\B2\BB\D7\E8\C8\FB\B0汾\A3\ACvde\D5\FD\D4\DA\D4\CB\D0з\B5\BB\D8VDE_STATUS_RUNING
    int (*query)(struct vde_handle*, VDE_Status_t*);    
    
    // \B2\E9ѯVDE״̬, \D7\E8\C8\FB\B0汾, ֱ\B5\BDVDE_STATUS_DEAD\BB\F2\D5\DFVDE\D6жϲ\FA\C9\FA, \B7\B5\BB\D8ֵ\BC\FBVDE_Status_t
    int (*query_timeout)(struct vde_handle*, VDE_Status_t*);    
    
    // \BD\AB״̬תΪidle
    int (*reset)(struct vde_handle*);   
    
} vde_handle_t;

// \BB\F1ȡ\BE\E4\B1\FA. \B2\CE\CA\FD\B4\ED\CE\F3\BB\F2\D5ߴﵽ\D7\EE\B4\F3\B5\C4\D4\CB\D0\D0instance\B8\F6\CA\FD\A3\AC\B7\B5\BB\D8NULL;     
vde_handle_t *vde_getHandle(void);

// \B9رվ\E4\B1\FA    
void vde_freeHandle(struct vde_handle*);

// DEBUG, \B4\F2\BF\AA\C4ڲ\BF\B4\F2ӡ
void vde_enable_log(void);

// DEBUG, \B9ر\D5\C4ڲ\BF\B4\F2ӡ
void vde_disable_log(void);

// DEBUG \BB\F1ȡ\B5\B1ǰ\D4\CB\D0е\C4instance\D0\C5Ϣ\BA\CD\CB\F9\D3мĴ\E6\C6\F7\D0\C5Ϣ, \B5\A5\B4\CE\D3\D0Ч
void vde_dump_info(void);



/**********************************************************
\C9\E8\BC\C6\C1\F7\B3\CC:  vd_h264.so  ----> libvde_core.so ----> vde_drv.ko

\CF\DE\D6\C6\CC\F5\BC\FE\A3\BA \B2\BB\C4ܿ\E7\BD\F8\B3\CCʹ\D3ã\AC\BC\B4\B2\BBͬinstance\B1\D8\D0\EB\CA\C7\D4\DAͬһ\B8\F6\BD\F8\B3\CC\D6С\A3

ʹ\D3÷\BD\B7\A8\A3\BA
    Android.mk\D6\D0\D4\F6\BC\D3 LOCAL_SHARED_LIBRARIES := libvde_core
    \B1\E0\D2\EBʱ\BB\E1\D7Զ\AF\C1\AC\BD\D3libvde_core.so, \BEͿ\C9\D2\D4\D7Զ\AF\BC\D3\D4\D8so, ʹ\D3\C3api\BA\AF\CA\FD, \B2\BB\D0\E8Ҫֱ\BDӵ\F7\D3\C3vde_drv.ko

\B6\EE\CD\E2˵\C3\F7:  reg1(status\BCĴ\E6\C6\F7\A3\A9\B2\BB\C4\DCд

Example code\A3\BA

    int vde_close(void *codec_handle)
    {
        // ...
        vde_freeHandle(codec_handle->vde_handle);
        codec_handle->vde_handle = NULL;
        // ...        
    }
    
    int vde_init(void *codec_handle)
    {
        codec_handle->vde_handle == vde_getHandle();
        if(codec_handle->vde_handle == NULL) return -1;
        
        if(DEBUG)
            vde_enable_log();           
        
        // if you need to know about overload of VDE;
        vde_dump_info();
                
        return 0;
    }
    
    int vde_decode_one_frame(void *codec_handle)
    {
        int rt;        
        unsigned int value;
        int status;
        
        vde_handle_t *vde = codec_handle->vde_handle;                
                      
        vde->reset(vde);      
        
        value = vde->readReg(vde, REG10);                
        value &= 0x2;
        
        rt = vde->writeReg(vde, REG10, value);
        if(rt) goto SOMETHING_WRONG;
        
        rt = vde->run(vde);
        if(rt) goto SOMETHING_WRONG;          
                        
#if USE_QUERY 
        // \CB\C0\B2飬Ч\C2ʵ\CD    
        while(timeout_ms < 10000)        
            rt = vde->query(vde, &status);
            if(rt) return -1;
            
            if(status != VDE_STATUS_RUNING && status != VDE_STATUS_IDLE) 
                break;               
        }
#else                
        // dosomthing else, \D4\D9\C0\B4\B2飬\C4ڲ\BF\D3\D0\C8\CE\CE\F1\B5\F7\B6ȣ\AC\BF\C9\CC\E1\B8\DFcpu\C0\FB\D3\C3\C2\CA
        rt = vde->query_timeout(vde, &status);
        if(rt) goto SOMETHING_WRONG;

#endif
        
        if(status == VDE_STATUS_GOTFRAME) {                
            return 0;
        } else {
            goto SOMETHING_WRONG;
        }           
           
SOMETHING_WRONG:
        
        if(status == VDE_STATUS_DEAD) {
            ACTAL_ERROR("VDE Died");            
            return -1; //fatal error here.
        } else {
            ACTAL_ERROR("something wrong, check your code")
            return -1;
        }       
                   
    }

**********************************************************/
#ifdef __cplusplus
}
#endif

#endif//_VDE_CORE_H_

