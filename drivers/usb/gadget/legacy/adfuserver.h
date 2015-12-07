/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 Actions Semi Inc.
*/
/******************************************************************************/

/******************************************************************************/
#ifndef __MBR_INFO_H__
#define __MBR_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTITION   	12
#define HDCP_KEY_SIZE		308 	//bytes
#define SERIAL_NO_SIZE		16   	//bytes
#define PARTITION_TBL_SIZE	(MAX_PARTITION * sizeof(partition_info_t))


#define RECOVERY_ACCESS      0
#define MISC_ACCESS       1
#define ROOTFS_ACCESS       2
#define ANDROID_DATA_ACCESS 3
#define ANDROID_CACHE_ACCESS 4

#define SNAPSHOT_ACCESS		5
#define WHD_ACCESS			6

#define CONFIG_RW_ACCESS 	7


#define UDISK_ACCESS        10

typedef struct
{
    unsigned char   flash_ptn;                  //flash partition number
    unsigned char   partition_num;              //ÿ\B8\F6\B7\D6\C7\F8\B6\D4Ӧ\C6\E4\CB\F9\D4ڵ\C4flash partition\B5ı\E0\BA\C5
    unsigned short  reserved;                   //reserved\A3\BA\BD\AB\CD\D8չ\B3ɸ÷\D6\C7\F8\B5\C4\CA\F4\D0\D4
    unsigned int    partition_cap;              //\B6\D4Ӧ\B7\D6\C7\F8\B5Ĵ\F3С
}__attribute__ ((packed)) partition_info_t;


typedef struct
{
    unsigned char   flash_ptn;                  //flash partition number
    unsigned char   partition_num;              //ÿ\B8\F6\B7\D6\C7\F8\B6\D4Ӧ\C6\E4\CB\F9\D4ڵ\C4flash partition\B5ı\E0\BA\C5
    unsigned short  phy_info;                   //reserved\A3\BA\BD\AB\CD\D8չ\B3ɸ÷\D6\C7\F8\B5\C4\CA\F4\D0\D4
    unsigned int    partition_cap;              //\B6\D4Ӧ\B7\D6\C7\F8\B5Ĵ\F3С
}__attribute__ ((packed)) CapInfo_t;


/*
 * don't re-order
 */
typedef struct
{
    partition_info_t partition_info[MAX_PARTITION];            //\B7\D6\C7\F8\D0\C5Ϣ\B1\ED
    unsigned char HdcpKey[HDCP_KEY_SIZE];
    unsigned char SerialNo[SERIAL_NO_SIZE];
    unsigned char reserved[0x400 - PARTITION_TBL_SIZE - HDCP_KEY_SIZE - SERIAL_NO_SIZE];     //mbr_info_t\A3\AC\B4\F3СΪ1k\A3\ACΪ\D2Ժ\F3\C0\A9չ
}__attribute__ ((packed)) mbr_info_t;



/********************************************************************
\B7\D6\C7\F8\B7\BDʽ\A3\ACԭ\D4\F2\C9Ϸ\D6\C7\F8\B0\B4˳\D0\F2\C5\C5\C1У\BA
            flash_ptn       partition_num       partition_cap\B5ĵ\A5λ
mbrc:           0                   0               block
vmlinux         0                   1               M
rootfs          1                   0               M
configfs        1                   1               M
others          2                   0~n             M

\D2\D4partition_numΪ0xff\B1\ED\C3\F7\D7\EE\BA\F3һ\B8\F6\B7\D6\C7\F8
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
