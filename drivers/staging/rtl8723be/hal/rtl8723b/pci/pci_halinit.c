/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_HAL_INIT_C_

#include <rtl8723b_hal.h>


// For Two MAC FPGA verify we must disable all MAC/BB/RF setting
#define FPGA_UNKNOWN		0
#define FPGA_2MAC			1
#define FPGA_PHY			2
#define ASIC					3
#define BOARD_TYPE			ASIC

#if BOARD_TYPE == FPGA_2MAC
#else // FPGA_PHY and ASIC
#define FPGA_RF_UNKOWN	0
#define FPGA_RF_8225		1
#define FPGA_RF_0222D		2
#define FPGA_RF				FPGA_RF_0222D
#endif

//-------------------------------------------------------------------
//
//	EEPROM Content Parsing
//
//-------------------------------------------------------------------

static VOID
Hal_ReadPROMContent_BT_8723BE(
	IN PADAPTER 	Adapter
	)
{

#if MP_DRIVER == 1
	if (Adapter->registrypriv.mp_mode == 1)
		EFUSE_ShadowMapUpdate(Adapter, EFUSE_BT, _FALSE);
#endif

}

static VOID
hal_ReadIDs_8723BE(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if( !AutoloadFail )
	{
		// VID, DID
		pHalData->EEPROMVID = EF2Byte( *(u16 *)&PROMContent[EEPROM_VID_8723BE] );
		pHalData->EEPROMDID = EF2Byte( *(u16 *)&PROMContent[EEPROM_DID_8723BE] );
		pHalData->EEPROMSVID = EF2Byte( *(u16 *)&PROMContent[EEPROM_SVID_8723BE] );
		pHalData->EEPROMSMID = EF2Byte( *(u16 *)&PROMContent[EEPROM_SMID_8723BE] );

		// Customer ID, 0x00 and 0xff are reserved for Realtek.
		pHalData->EEPROMCustomerID = *(u8 *)&PROMContent[EEPROM_CustomID_8723B];
		if(pHalData->EEPROMCustomerID == 0xFF)
			pHalData->EEPROMCustomerID = EEPROM_Default_CustomerID_8188E;
		pHalData->EEPROMSubCustomerID = EEPROM_Default_SubCustomerID;
	}
	else
	{
		pHalData->EEPROMVID 		= 0;
		pHalData->EEPROMDID 		= 0;
		pHalData->EEPROMSVID 		= 0;
		pHalData->EEPROMSMID 		= 0;

		// Customer ID, 0x00 and 0xff are reserved for Realtek.
		pHalData->EEPROMCustomerID	= EEPROM_Default_CustomerID;
		pHalData->EEPROMSubCustomerID	= EEPROM_Default_SubCustomerID;
	}

	DBG_871X("VID = 0x%04X, DID = 0x%04X\n", pHalData->EEPROMVID, pHalData->EEPROMDID);
	DBG_871X("SVID = 0x%04X, SMID = 0x%04X\n", pHalData->EEPROMSVID, pHalData->EEPROMSMID);
}

static VOID
hal_ReadMACAddress_8723BE(
	IN	PADAPTER		pAdapter,
	IN	u8				*PROMContent,
	IN	BOOLEAN			AutoLoadFail
	)
{
	u8	sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x88, 0x12, 0xCC};
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(pAdapter);
	if(AutoLoadFail)
	{
		//sMacAddr[5] = (u8)GetRandomNumber(1, 254);
		_rtw_memcpy(pHalData->EEPROMMACAddr, sMacAddr, ETH_ALEN);
	}
	else
	{
		//Read Permanent MAC address
		_rtw_memcpy(pHalData->EEPROMMACAddr, &PROMContent[EEPROM_MAC_ADDR_8723BE], ETH_ALEN);
	}

	DBG_871X("%s MAC Address from EFUSE = "MAC_FMT"\n",__FUNCTION__, MAC_ARG(pHalData->EEPROMMACAddr));
}

static VOID
Hal_ReadEfusePCIeCap8723BE(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN 	AutoloadFail
	)
{
	u8	AspmOscSupport = RT_PCI_ASPM_OSC_IGNORE;
	u16	PCIeCap = 0;

	if(!AutoloadFail)
	{
		PCIeCap = PROMContent[EEPROM_PCIE_DEV_CAP_01] |
			(PROMContent[EEPROM_PCIE_DEV_CAP_02]<<8);

		DBG_871X("Hal_ReadEfusePCIeCap8723BE(): PCIeCap = %#x\n", PCIeCap);

		//
		// <Roger_Notes> We shall take L0S/L1 accept latency into consideration for ASPM Configuration policy, 2013.03.27.
		// L1 accept Latency: 0x8d from PCI CFG space offset 0x75
		// L0S accept Latency: 0x80 from PCI CFG space offset 0x74
		//
		if(PCIeCap == 0x8d80)
			AspmOscSupport |= RT_PCI_ASPM_OSC_ENABLE;
		else
			AspmOscSupport |= RT_PCI_ASPM_OSC_DISABLE;
	}

	rtw_hal_set_def_var(Adapter, HAL_DEF_PCI_ASPM_OSC, (u8 *)&AspmOscSupport);
	DBG_871X("Hal_ReadEfusePCIeCap8723BE(): AspmOscSupport = %d\n", AspmOscSupport);
}

static VOID
hal_CustomizedBehavior_8723BE(
	PADAPTER			Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*pledpriv = &(Adapter->ledpriv);

	pledpriv->LedStrategy = SW_LED_MODE7; //Default LED strategy.
	pHalData->bLedOpenDrain = _TRUE;// Support Open-drain arrangement for controlling the LED. Added by Roger, 2009.10.16.

	switch(pHalData->CustomerID)
	{
		case RT_CID_DEFAULT:
			break;

		case RT_CID_CCX:
			//pMgntInfo->IndicateByDeauth = _TRUE;
			break;

		case RT_CID_WHQL:
			//Adapter->bInHctTest = _TRUE;
			break;

		default:
			//DBG_871X("Unkown hardware Type \n");
			break;
	}
	DBG_871X("hal_CustomizedBehavior_8723BE(): RT Customized ID: 0x%02X\n", pHalData->CustomerID);

#if 0
	if(Adapter->bInHctTest)
	{
		pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
		pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
		pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;
		pMgntInfo->PowerSaveControl.bLeisurePsModeBackup = FALSE;
		pMgntInfo->keepAliveLevel = 0;
	}
#endif
}

static VOID
hal_CustomizeByCustomerID_8723BE(
	IN	PADAPTER		pAdapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	// If the customer ID had been changed by registry, do not cover up by EEPROM.
	if(pHalData->CustomerID == RT_CID_DEFAULT)
	{
		switch(pHalData->EEPROMCustomerID)
		{
			default:
				pHalData->CustomerID = RT_CID_DEFAULT;
				break;

		}
	}
	DBG_871X("MGNT Customer ID: 0x%2x\n", pHalData->CustomerID);

	hal_CustomizedBehavior_8723BE(pAdapter);
}


static VOID
InitAdapterVariablesByPROM_8723BE(
	IN	PADAPTER	Adapter
	)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	//DBG_871X("InitAdapterVariablesByPROM_8723BE()!!\n");

	Hal_InitPGData(Adapter, pHalData->efuse_eeprom_data);
	Hal_EfuseParseIDCode(Adapter, pHalData->efuse_eeprom_data);

	Hal_EfuseParseEEPROMVer_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadIDs_8723BE(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_ReadMACAddress_8723BE(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseTxPowerInfo_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_ReadEfusePCIeCap8723BE(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	//
	// Read Bluetooth co-exist and initialize
	//
	Hal_EfuseParseBTCoexistInfo_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);

	Hal_EfuseParseChnlPlan_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseXtal_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParsePackageType_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseThermalMeter_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	Hal_EfuseParseAntennaDiversity_8723B(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
	hal_CustomizeByCustomerID_8723BE(Adapter);

#ifdef CONFIG_RF_GAIN_OFFSET
	Hal_ReadRFGainOffset(Adapter, pHalData->efuse_eeprom_data, pHalData->bautoload_fail_flag);
#endif	//CONFIG_RF_GAIN_OFFSET

}

static VOID
Hal_ReadPROMContent_8723BE(
	IN PADAPTER 		Adapter
	)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
	u8			eeValue;


	eeValue = rtw_read8(Adapter, REG_9346CR);
	// To check system boot selection.
	pHalData->EepromOrEfuse		= (eeValue & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag	= (eeValue & EEPROM_EN) ? _FALSE : _TRUE;

	DBG_871X("Boot from %s, Autoload %s !\n", (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
				(pHalData->bautoload_fail_flag ? "Fail" : "OK") );

	//pHalData->EEType = IS_BOOT_FROM_EEPROM(Adapter) ? EEPROM_93C46 : EEPROM_BOOT_EFUSE;

	InitAdapterVariablesByPROM_8723BE(Adapter);
}

static void ReadRFType8723B(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

#if DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
#else
	pHalData->rf_chip = RF_6052;
#endif
}

static void ReadAdapterInfo8723BE(PADAPTER Adapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	//DBG_871X("====> ReadAdapterInfo8723BE\n");

	// Read EEPROM size before call any EEPROM function
	Adapter->EepromAddressSize = GetEEPROMSize8723B(Adapter);

	// For debug test now!!!!!
	PHY_RFShadowRefresh(Adapter);

	// Read all content in Efuse/EEPROM.
	Hal_ReadPROMContent_8723BE(Adapter);

	Hal_ReadPROMContent_BT_8723BE(Adapter);

	// We need to define the RF type after all PROM value is recognized.
	ReadRFType8723B(Adapter);

	//DBG_871X("<==== ReadAdapterInfo8723BE\n");
}


void rtl8723be_interface_configure(PADAPTER Adapter)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(Adapter);

_func_enter_;

	////close ASPM for AMD defaultly
	pdvobjpriv->const_amdpci_aspm = 0;

	//// ASPM PS mode.
	//// 0 - Disable ASPM, 1 - Enable ASPM without Clock Req,
	//// 2 - Enable ASPM with Clock Req, 3- Alwyas Enable ASPM with Clock Req,
	//// 4-  Always Enable ASPM without Clock Req.
	//// set defult to rtl8188ee:3 RTL8192E:2
	pdvobjpriv->const_pci_aspm = 0;

	//// Setting for PCI-E device */
	pdvobjpriv->const_devicepci_aspm_setting = 0x03;

	//// Setting for PCI-E bridge */
	pdvobjpriv->const_hostpci_aspm_setting = 0x03;

	//// In Hw/Sw Radio Off situation.
	//// 0 - Default, 1 - From ASPM setting without low Mac Pwr,
	//// 2 - From ASPM setting with low Mac Pwr, 3 - Bus D3
	//// set default to RTL8192CE:0 RTL8192SE:2
	pdvobjpriv->const_hwsw_rfoff_d3 = 0;

	//// This setting works for those device with backdoor ASPM setting such as EPHY setting.
	//// 0: Not support ASPM, 1: Support ASPM, 2: According to chipset.
	pdvobjpriv->const_support_pciaspm = 1;

	pwrpriv->reg_rfoff = 0;
	pwrpriv->rfoff_reason = 0;

	pHalData->bL1OffSupport = _FALSE;
_func_exit_;
}

VOID
DisableInterrupt8723BE (
	IN PADAPTER			Adapter
	)
{
	struct dvobj_priv	*pdvobjpriv= adapter_to_dvobj(Adapter);

	rtw_write32(Adapter, REG_HIMR0_8723B, IMR_DISABLED_8723B);

	rtw_write32(Adapter, REG_HIMR1_8723B, IMR_DISABLED_8723B);	// by tynli

	pdvobjpriv->irq_enabled = 0;
}

VOID
ClearInterrupt8723BE(
	IN PADAPTER			Adapter
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
	u32	tmp = 0;

	tmp = rtw_read32(Adapter, REG_HISR0_8723B);
	rtw_write32(Adapter, REG_HISR0_8723B, tmp);
	pHalData->IntArray[0] = 0;

	tmp = rtw_read32(Adapter, REG_HISR1_8723B);
	rtw_write32(Adapter, REG_HISR1_8723B, tmp);
	pHalData->IntArray[1] = 0;

	tmp = rtw_read32(Adapter, REG_HSISR_8723B);
	rtw_write32(Adapter, REG_HSISR_8723B, tmp);
	pHalData->SysIntArray[0] = 0;
}


VOID
EnableInterrupt8723BE(
	IN PADAPTER			Adapter
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
#ifdef CONFIG_CONCURRENT_MODE
	if ((Adapter->isprimary == _FALSE) && Adapter->pbuddy_adapter){
		Adapter = Adapter->pbuddy_adapter;
		pHalData=GET_HAL_DATA(Adapter);
		pdvobjpriv = adapter_to_dvobj(Adapter);
	}
#endif
	pdvobjpriv->irq_enabled = 1;

	rtw_write32(Adapter, REG_HIMR0_8723B, pHalData->IntrMask[0]&0xFFFFFFFF);

	rtw_write32(Adapter, REG_HIMR1_8723B, pHalData->IntrMask[1]&0xFFFFFFFF);

	rtw_write32(Adapter, REG_HSISR_8723B, pHalData->SysIntrMask[0]&0xFFFFFFFF);
}

BOOLEAN
InterruptRecognized8723BE(
	IN	PADAPTER			Adapter
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
	BOOLEAN			bRecognized = _FALSE;

#ifdef CONFIG_CONCURRENT_MODE
        if ((Adapter->isprimary == _FALSE) && Adapter->pbuddy_adapter){
		Adapter = Adapter->pbuddy_adapter;
		pHalData=GET_HAL_DATA(Adapter);
	}
#endif

	// 2013.11.18 Glayrainx suggests that turn off IMR and
	// restore after cleaning ISR.
	rtw_write32(Adapter, REG_HIMR0_8723B, 0 );
	rtw_write32(Adapter, REG_HIMR1_8723B, 0 );

	pHalData->IntArray[0] = rtw_read32(Adapter, REG_HISR0_8723B);
	pHalData->IntArray[0] &= pHalData->IntrMask[0];
	rtw_write32(Adapter, REG_HISR0_8723B, pHalData->IntArray[0]);

	//For HISR extension. Added by tynli. 2009.10.07.
	pHalData->IntArray[1] = rtw_read32(Adapter, REG_HISR1_8723B);
	pHalData->IntArray[1] &= pHalData->IntrMask[1];
	rtw_write32(Adapter, REG_HISR1_8723B, pHalData->IntArray[1]);

	if (((pHalData->IntArray[0])&pHalData->IntrMask[0])!=0 ||
		((pHalData->IntArray[1])&pHalData->IntrMask[1])!=0)
		bRecognized = _TRUE;

	rtw_write32(Adapter, REG_HIMR0_8723B, pHalData->IntrMask[0]&0xFFFFFFFF);
	rtw_write32(Adapter, REG_HIMR1_8723B, pHalData->IntrMask[1]&0xFFFFFFFF);

	return bRecognized;
}

VOID
UpdateInterruptMask8723BE(
	IN	PADAPTER		Adapter,
	IN	u32		AddMSR, 	u32		AddMSR1,
	IN	u32		RemoveMSR, u32		RemoveMSR1
	)
{
	HAL_DATA_TYPE	*pHalData=GET_HAL_DATA(Adapter);
#ifdef CONFIG_CONCURRENT_MODE
        if ((Adapter->isprimary == _FALSE) && Adapter->pbuddy_adapter){
		Adapter = Adapter->pbuddy_adapter;
		pHalData=GET_HAL_DATA(Adapter);
	}
#endif

	DisableInterrupt8723BE( Adapter );

	if( AddMSR )
	{
		pHalData->IntrMask[0] |= AddMSR;
	}
	if( AddMSR1 )
	{
		pHalData->IntrMask[1] |= AddMSR1;
	}

	if( RemoveMSR )
	{
		pHalData->IntrMask[0] &= (~RemoveMSR);
	}

	if( RemoveMSR1 )
	{
		pHalData->IntrMask[1] &= (~RemoveMSR1);
	}

	EnableInterrupt8723BE( Adapter );
}

static VOID
HwConfigureRTL8723B(
		IN	PADAPTER			Adapter
		)
{

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u32	regRRSR = 0;


	//1 This part need to modified according to the rate set we filtered!!
	//
	// Set RRSR, RATR, and BW_OPMODE registers
	//
	switch(pHalData->CurrentWirelessMode)
	{
	case WIRELESS_MODE_B:
		regRRSR = RATE_ALL_CCK;
		break;
	case WIRELESS_MODE_G:
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_AUTO:
	case WIRELESS_MODE_N_24G:
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	default:
		break;
	}

	// Init value for RRSR.
	rtw_write32(Adapter, REG_RRSR, regRRSR);

	// ARFB table 9 for 11ac 5G 2SS
#if (HAL_MAC_ENABLE == 0)
	rtw_write32(Adapter, REG_ARFR0, 0x00000010);
#endif
	if(IS_NORMAL_CHIP(pHalData->VersionID))
		rtw_write32(Adapter, REG_ARFR0+4, 0xfffff000);
	else
		rtw_write32(Adapter, REG_ARFR0+4, 0x3e0ff000);

	// ARFB table 10 for 11ac 5G 1SS
#if (HAL_MAC_ENABLE == 0)
	rtw_write32(Adapter, REG_ARFR1, 0x00000010);
#endif

	//
	// <Roger_Notes> We should take ARFR settings for Test and MP chip into consideration later
	//
	if(IS_VENDOR_8723B_TEST_CHIP(Adapter))
		PlatformEFIOWrite4Byte(Adapter, REG_ARFR1_8723B+4, 0x000ff000);
	else
		PlatformEFIOWrite4Byte(Adapter, REG_ARFR1_8723B+4, 0x003ff000);

	// Set SLOT time Reg518 0x9

	// 0x420[7] = 0 , enable retry AMPDU in new AMPD not singal MPDU.
	rtw_write16(Adapter,REG_FWHW_TXQ_CTRL, 0x1F00);
	// 0x456 = 0x70, sugguested by Zhilin
	rtw_write8(Adapter,REG_AMPDU_MAX_TIME_8723B, 0x70);


	// Set retry limit
	//3vivi, 20100928, especially for DTM, performance_ext, To avoid asoc too long to another AP more than 4.1 seconds.
	//3we find retry 7 times maybe not enough, so we retry more than 7 times to pass DTM.
	//if(pMgntInfo->bPSPXlinkMode)
	//{
	//	pHalData->ShortRetryLimit = 3;
	//	pHalData->LongRetryLimit = 3;
		// Set retry limit
	//	rtw_write16(Adapter,REG_RL, 0x0303);
	//}
	//else
		rtw_write16(Adapter,REG_RL, 0x0707);

	// Set Data / Response auto rate fallack retry count
	rtw_write32(Adapter, REG_DARFRC, 0x01000000);
	rtw_write32(Adapter, REG_DARFRC+ 4, 0x07060504);
	rtw_write32(Adapter, REG_RARFRC, 0x01000000);
	rtw_write32(Adapter, REG_RARFRC + 4, 0x07060504);

	// Beacon related, for rate adaptive
	// ATIMWND Reg55A  0x2
#if (HAL_MAC_ENABLE == 0)
	rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xff);
#endif

	// 20100211 Joseph: Change original setting of BCN_CTRL(0x550) from
	// 0x1e(0x2c for test chip) ro 0x1f(0x2d for test chip). Set BIT0 of this register disable ATIM
	// function. Since we do not use HIGH_QUEUE anymore, ATIM function is no longer used.
	// Also, enable ATIM function may invoke HW Tx stop operation. This may cause ping failed
	// sometimes in long run test. So just disable it now.
	//PlatformAtomicExchange((pu4Byte)(&pHalData->RegBcnCtrlVal), 0x1d);
	pHalData->RegBcnCtrlVal = 0x1d;
#ifdef CONFIG_CONCURRENT_MODE
	rtw_write16(Adapter, REG_BCN_CTRL, 0x1010);	// For 2 PORT TSF SYNC
#else
	rtw_write8(Adapter, REG_BCN_CTRL, (u8)(pHalData->RegBcnCtrlVal));
#endif

	// BCN_CTRL1 Reg551 0x10

	// TBTT prohibit hold time. Suggested by designer TimChen.
	rtw_write8(Adapter, REG_TBTT_PROHIBIT + 1,0xff); // 8 ms

	// AGGR_BK_TIME Reg51A 0x16

	rtw_write16(Adapter, REG_NAV_PROT_LEN, 0x0040);

	if(!Adapter->registrypriv.wifi_spec)
	{
		//For Rx TP. Suggested by SD1 Richard. Added by tynli. 2010.04.12.
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x03086666);
	}
	else
	{
		//For WiFi WMM. suggested by timchen. Added by tynli.
		rtw_write32(Adapter, REG_FAST_EDCA_CTRL, 0x0);
	}

#if DISABLE_BB_RF

	// 0x50 for 80MHz clock
	rtw_write8(Adapter, REG_USTIME_TSF, 0x50);
	rtw_write8(Adapter, REG_USTIME_EDCA, 0x50);

	// Set Spec SIFS (used in NAV)
	rtw_write16(Adapter,REG_SPEC_SIFS, 0x1010);
	rtw_write16(Adapter,REG_MAC_SPEC_SIFS, 0x1010);

	// Set SIFS for CCK
	rtw_write16(Adapter,REG_SIFS_CTX, 0x1010);

	// Set SIFS for OFDM
	rtw_write16(Adapter,REG_SIFS_TRX, 0x1010);

	// PIFS
	rtw_write8(Adapter, REG_PIFS, 0);

	// Protection Ctrl
	rtw_write16(Adapter, REG_PROT_MODE_CTRL, 0x08ff);

	// BAR settings
	rtw_write32(Adapter, REG_BAR_MODE_CTRL, 0x0001ffff);

	// ACKTO for IOT issue.
	rtw_write8(Adapter, REG_ACKTO, 0x40);
#else
	// Set Spec SIFS Reg428 Reg63A 0x100a

	// Set SIFS for CCK Reg514 0x100a

	// Set SIFS for OFDM Reg516 0x100a

	// Protection Ctrl Reg4C8 0x08ff

	// BAR settings Reg4CC 0x01ffff

	// PIFS Reg512 0x1C

	// ACKTO for IOT issue Reg640 0x80
#endif

	rtw_write8(Adapter, REG_HT_SINGLE_AMPDU_8723B, 0x80);

	rtw_write8(Adapter, REG_RX_PKT_LIMIT, 0x20);

	rtw_write16(Adapter, REG_MAX_AGGR_NUM, 0x1F1F);

//#if (OMNIPEEK_SNIFFER_ENABLED == 1)
	// For sniffer parsing legacy rate bandwidth information
//	PHY_SetBBReg(Adapter, 0x834, BIT26, 0x0);
//#endif

#if (HAL_MAC_ENABLE == 0)
	rtw_write32(Adapter, REG_MAR, 0xffffffff);
	rtw_write32(Adapter, REG_MAR+4, 0xffffffff);
#endif

	//Reject all control frame - default value is 0
	rtw_write16(Adapter,REG_RXFLTMAP1,0x0);
}

static u32 _InitPowerOn_8723BE(PADAPTER Adapter)
{
#ifdef CONFIG_BT_COEXIST
	u8 value8;
	u16 value16;
	u32 value32;
#endif
	u32	status = _SUCCESS;
	u8	tmpU1b;
	u8 bMacPwrCtrlOn=_FALSE;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if(bMacPwrCtrlOn == _TRUE)
		return _SUCCESS;

	rtw_write8(Adapter, REG_RSV_CTRL, 0x0);

	//Auto Power Down to CHIP-off State
	tmpU1b = (rtw_read8(Adapter, REG_APS_FSMCO+1) & (~BIT7));
	rtw_write8(Adapter, REG_APS_FSMCO+1, tmpU1b);

	if(!HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723B_card_enable_flow))
		return _FAIL;

	//Enable Power Down Interrupt
	tmpU1b = (rtw_read8(Adapter, REG_APS_FSMCO) | BIT4);
	rtw_write8(Adapter, REG_APS_FSMCO, tmpU1b);

	// Release MAC IO register reset
	tmpU1b = rtw_read8(Adapter, REG_CR);
	tmpU1b = 0xff;
	rtw_write8(Adapter, REG_CR, tmpU1b);
	rtw_mdelay_os(2);

	tmpU1b = rtw_read8(Adapter, REG_HWSEQ_CTRL);
	tmpU1b |= 0x7f;
	rtw_write8(Adapter, REG_HWSEQ_CTRL, tmpU1b);
	rtw_mdelay_os(2);

	//Need remove below furture, suggest by Jackie.
	// if 0xF0[24] =1 (LDO), need to set the 0x7C[6] to 1.
		tmpU1b = rtw_read8(Adapter, REG_SYS_CFG+3);
		if(tmpU1b & BIT0) //LDO mode.
		{
			tmpU1b =rtw_read8(Adapter, 0x7c);
			rtw_write8(Adapter,0x7c,tmpU1b | BIT6);
		}

	// Add for wakeup online
	tmpU1b = rtw_read8(Adapter, REG_SYS_CLKR);
	rtw_write8(Adapter, REG_SYS_CLKR, (tmpU1b|BIT3));
	tmpU1b = rtw_read8(Adapter, REG_GPIO_MUXCFG+1);
	rtw_write8(Adapter, REG_GPIO_MUXCFG+1, (tmpU1b & ~BIT4));

	// Release MAC IO register reset
	// 9.	CR 0x100[7:0]	= 0xFF;
	// 10.	CR 0x101[1]	= 0x01; // Enable SEC block
	rtw_write16(Adapter,REG_CR, 0x2ff);

#ifdef CONFIG_BT_COEXIST
	rtw_btcoex_PowerOnSetting(Adapter);

	// external switch to S1
	// 0x38[11] = 0x1
	// 0x4c[23] = 0x1
	// 0x64[0] = 0
	value16 = rtw_read16(Adapter, REG_PWR_DATA);
	// Switch the control of EESK, EECS to RFC for DPDT or Antenna switch
	value16 |= BIT(11); // BIT_EEPRPAD_RFE_CTRL_EN
	rtw_write16(Adapter, REG_PWR_DATA, value16);
//	DBG_8192C("%s: REG_PWR_DATA(0x%x)=0x%04X\n", __FUNCTION__, REG_PWR_DATA, rtw_read16(padapter, REG_PWR_DATA));

	value32 = rtw_read32(Adapter, REG_LEDCFG0);
	value32 |= BIT(23); // DPDT_SEL_EN, 1 for SW control
	rtw_write32(Adapter, REG_LEDCFG0, value32);
//	DBG_8192C("%s: REG_LEDCFG0(0x%x)=0x%08X\n", __FUNCTION__, REG_LEDCFG0, rtw_read32(padapter, REG_LEDCFG0));

	value8 = rtw_read8(Adapter, REG_PAD_CTRL1_8723B);
	value8 &= ~BIT(0); // BIT_SW_DPDT_SEL_DATA, DPDT_SEL default configuration
	rtw_write8(Adapter, REG_PAD_CTRL1_8723B, value8);
//	DBG_8192C("%s: REG_PAD_CTRL1(0x%x)=0x%02X\n", __FUNCTION__, REG_PAD_CTRL1_8723B, rtw_read8(padapter, REG_PAD_CTRL1_8723B));
#endif // CONFIG_BT_COEXIST

	bMacPwrCtrlOn = _TRUE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);

	return status;
}

static u8
_RQPN_Init_8723B(
	IN  PADAPTER	Adapter,
	OUT u8		*pNPQRQPNVaule,
	OUT u32		*pRQPNValue
	)
{
	u8			i, numNQ = 0, numPubQ=0, numHQ=0, numLQ=0;
	//u8*			numPageArray;

	if(Adapter->registrypriv.wifi_spec) {
		numHQ = WMM_NORMAL_PAGE_NUM_HPQ_8723B;	// 0x30
		numLQ = WMM_NORMAL_PAGE_NUM_LPQ_8723B;	// 0x20
		numNQ = WMM_NORMAL_PAGE_NUM_NPQ_8723B;	// 0x20
	} else {
		numHQ = NORMAL_PAGE_NUM_HPQ_8723B;	// 0x0c
		numLQ = NORMAL_PAGE_NUM_LPQ_8723B;	// 0x02
		numNQ = NORMAL_PAGE_NUM_NPQ_8723B;	// 0x02
	}

	numPubQ = TX_TOTAL_PAGE_NUMBER_8723B - numHQ - numLQ - numNQ;

	*pNPQRQPNVaule = _NPQ(numNQ);
	*pRQPNValue = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;

	if(Adapter->registrypriv.wifi_spec)
		return WMM_NORMAL_TX_PAGE_BOUNDARY_8723B;
	else
		return TX_PAGE_BOUNDARY_8723B;
}

static s32
_LLTWrite_8723B(
	IN	PADAPTER	Adapter,
	IN	u32			address,
	IN	u32			data
	)
{
	s32	status = _SUCCESS;
	s32	count = 0;
	u32	value = _LLT_INIT_ADDR(address) | _LLT_INIT_DATA(data) | _LLT_OP(_LLT_WRITE_ACCESS);

	rtw_write32(Adapter, REG_LLT_INIT, value);

	//polling
	do {
		value = rtw_read32(Adapter, REG_LLT_INIT);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value)) {
			break;
		}

		if (count > POLLING_LLT_THRESHOLD) {
			RT_TRACE(_module_hal_init_c_, _drv_err_, ("Failed to polling write LLT done at address %d!\n", address));
			status = _FAIL;
			break;
		}
	} while (count++);

	return status;
}

static s32
LLT_table_init_8723B(
	IN	PADAPTER	Adapter,
	IN	u8			txpktbuf_bndy,
	IN	u32			RQPN,
	IN	u8			RQPN_NPQ
	)
{
	u16	i, maxPage = 255;
	s32	status = _SUCCESS;

	// 12.	TXRKTBUG_PG_BNDY 0x114[31:0] = 0x27FF00F6	//TXRKTBUG_PG_BNDY
	rtw_write8(Adapter, REG_TRXFF_BNDY, txpktbuf_bndy);
	rtw_write16(Adapter, REG_TRXFF_BNDY+2, RX_DMA_BOUNDARY_8723B);

	// 13.	0x208[15:8] 0x209[7:0] = 0xF6				// Beacon Head for TXDMA
	rtw_write8(Adapter,REG_DWBCN0_CTRL_8723B+1, txpktbuf_bndy);

	// 14.	BCNQ_PGBNDY 0x424[7:0] =  0xF6				//BCNQ_PGBNDY
	// 2009/12/03 Why do we set so large boundary. confilct with document V11.
	rtw_write8(Adapter,REG_BCNQ_BDNY, txpktbuf_bndy);
	rtw_write8(Adapter,REG_MGQ_BDNY, txpktbuf_bndy);

#ifdef CONFIG_CONCURRENT_MODE
	rtw_write8(Adapter, REG_BCNQ1_BDNY, txpktbuf_bndy+8);
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8723B+1, txpktbuf_bndy+8);//BCN1_HEAD
	// BIT1- BIT_SW_BCN_SEL_EN
	rtw_write8(Adapter, REG_DWBCN1_CTRL_8723B+2, rtw_read8(Adapter, REG_DWBCN1_CTRL_8723B+2)|BIT1);
#endif

	// 15.	WMAC_LBK_BF_HD 0x45D[7:0] =  0xF6			//WMAC_LBK_BF_HD
	rtw_write8(Adapter,REG_WMAC_LBK_BF_HD, txpktbuf_bndy);

	// Set Tx/Rx page size (Tx must be 128 Bytes, Rx can be 64,128,256,512,1024 bytes)
	// 16.	PBP [7:0] = 0x11								// TRX page size
	rtw_write8(Adapter,REG_PBP, 0x31);

	// 17.	DRV_INFO_SZ = 0x04
	rtw_write8(Adapter,REG_RX_DRVINFO_SZ, 0x4);

	// 18.	LLT_table_init(Adapter);
	for(i = 0 ; i < (txpktbuf_bndy - 1) ; i++){
		status = _LLTWrite_8723B(Adapter, i , i + 1);
		if(_SUCCESS != status){
			return status;
		}
	}

	// end of list
	status = _LLTWrite_8723B(Adapter, (txpktbuf_bndy - 1), 0xFF);
	if(_SUCCESS != status){
		return status;
	}

	// Make the other pages as ring buffer
	// This ring buffer is used as beacon buffer if we config this MAC as two MAC transfer.
	// Otherwise used as local loopback buffer.
	for(i = txpktbuf_bndy ; i < maxPage ; i++){
		status = _LLTWrite_8723B(Adapter, i, (i + 1));
		if(_SUCCESS != status){
			return status;
		}
	}

	// Let last entry point to the start entry of ring buffer
	status = _LLTWrite_8723B(Adapter, maxPage, txpktbuf_bndy);
	if(_SUCCESS != status)
	{
		return status;
	}

	// Set reserved page for each queue
	// 11.	RQPN 0x200[31:0]	= 0x80BD1C1C				// load RQPN
	rtw_write32(Adapter, REG_RQPN, RQPN);//0x80cb1010);//0x80711010);//0x80cb1010);

	if(RQPN_NPQ != 0)
	{
		rtw_write8(Adapter, REG_RQPN_NPQ, RQPN_NPQ);
	}

	return _SUCCESS;
}

static  u32
InitMAC_8723B(
	IN	PADAPTER	Adapter
)
{
	u8	tmpU1b;
	u16	tmpU2b;
	struct recv_priv	*precvpriv = &Adapter->recvpriv;
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	DBG_871X("=======>InitMAC_8723B()\n");

	if( _InitPowerOn_8723BE(Adapter) == _FAIL ) {
		DBG_871X("_InitPowerOn8723BE fail\n");
		return _FAIL;
	}

	//if(!pHalData->bMACFuncEnable)
	{
		//System init
		// 18.	LLT_table_init(Adapter);
		u32	RQPN = 0x80EB0808;
		u8	RQPN_NPQ = 0;
		u8	TX_PAGE_BOUNDARY =
				_RQPN_Init_8723B(Adapter, &RQPN_NPQ, &RQPN);

		if(LLT_table_init_8723B(Adapter, TX_PAGE_BOUNDARY, RQPN, RQPN_NPQ) == _FAIL)
			return _FAIL;
	}

	// Enable Host Interrupt
	rtw_write32(Adapter,REG_HISR0_8723B, 0xffffffff);
	rtw_write32(Adapter,REG_HISR1_8723B, 0xffffffff);
	// Enable FW Beamformer Interrupt.
	rtw_write8(Adapter, REG_FWIMR+3, (rtw_read8(Adapter, REG_FWIMR+3) | BIT6));

	tmpU2b = rtw_read16(Adapter,REG_TRXDMA_CTRL);
	tmpU2b &= 0xf;
	if(Adapter->registrypriv.wifi_spec)
		tmpU2b |= 0xF9B1;
	else
		tmpU2b |= 0xF5B1;
	rtw_write16(Adapter,REG_TRXDMA_CTRL, tmpU2b);


	// Reported Tx status from HW for rate adaptive.
	// 2009/12/03 MH This should be realtive to power on step 14. But in document V11
	// still not contain the description.!!!
	rtw_write8(Adapter,REG_FWHW_TXQ_CTRL+1, 0x1F);

	// Set RCR register
	rtw_write32(Adapter,REG_RCR, pHalData->ReceiveConfig);
	rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);
	// Set TCR register
	rtw_write32(Adapter,REG_TCR, pHalData->TransmitConfig);

	//
	// Set TX/RX descriptor physical address(from OS API).
	//
	rtw_write32(Adapter, REG_BCNQ_DESA, (u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_MGQ_DESA, (u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VOQ_DESA, (u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_VIQ_DESA, (u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BEQ_DESA, (u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_BKQ_DESA, (u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_HQ_DESA, (u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma & DMA_BIT_MASK(32));
	rtw_write32(Adapter, REG_RX_DESA, (u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma & DMA_BIT_MASK(32));

#ifdef CONFIG_64BIT_DMA
	// 2009/10/28 MH For DMA 64 bits. We need to assign the high 32 bit address
	// for NIC HW to transmit data to correct path.
	rtw_write32(Adapter, REG_BCNQ_DESA+4,
		((u64)pxmitpriv->tx_ring[BCN_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_MGQ_DESA+4,
		((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_VOQ_DESA+4,
		((u64)pxmitpriv->tx_ring[VO_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_VIQ_DESA+4,
		((u64)pxmitpriv->tx_ring[VI_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_BEQ_DESA+4,
		((u64)pxmitpriv->tx_ring[BE_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_BKQ_DESA+4,
		((u64)pxmitpriv->tx_ring[BK_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter,REG_HQ_DESA+4,
		((u64)pxmitpriv->tx_ring[HIGH_QUEUE_INX].dma)>>32);
	rtw_write32(Adapter, REG_RX_DESA+4,
		((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma)>>32);

	// 2009/10/28 MH If RX descriptor address is not equal to zero. We will enable
	// DMA 64 bit functuion.
	// Note: We never saw thd consition which the descripto address are divided into
	// 4G down and 4G upper seperate area.
	if (((u64)precvpriv->rx_ring[RX_MPDU_QUEUE].dma)>>32 != 0)
	{
		//DBG_871X("RX_DESC_HA=%08lx\n", ((u64)priv->rx_ring_dma[RX_MPDU_QUEUE])>>32);
		DBG_871X("Enable DMA64 bit\n");

		// Check if other descriptor address is zero and abnormally be in 4G lower area.
		if (((u64)pxmitpriv->tx_ring[MGT_QUEUE_INX].dma)>>32)
		{
			DBG_871X("MGNT_QUEUE HA=0\n");
		}

		PlatformEnableDMA64(Adapter);
	}
	else
	{
		DBG_871X("Enable DMA32 bit\n");
	}
#endif

	tmpU1b = rtw_read8(Adapter,REG_PCIE_CTRL_REG+3);
	rtw_write8(Adapter,REG_PCIE_CTRL_REG+3, (tmpU1b|0x77));

	// 20100318 Joseph: Reset interrupt migration setting when initialization. Suggested by SD1.
	rtw_write32(Adapter, REG_INT_MIG, 0);
	pHalData->bInterruptMigration = _FALSE;

	//2009.10.19. Reset H2C protection register. by tynli.
	rtw_write32(Adapter, REG_MCUTST_1, 0x0);

#if MP_DRIVER == 1
	if (Adapter->registrypriv.mp_mode == 1) {
		rtw_write32(Adapter, REG_MACID, 0x87654321);
		rtw_write32(Adapter, 0x0700, 0x87654321);
	}
#endif

#if (DISABLE_BB_RF == 1)
	rtw_write16(Adapter, REG_PCIE_MULTIFET_CTRL_8723B, 0xF450);
#endif
	// ending , Mask until BB secondary cca is ready.
	rtw_write8(Adapter, REG_SECONDARY_CCA_CTRL_8723B, 0x3);

	// Release RX DMA
	tmpU1b = rtw_read8(Adapter, REG_RXDMA_CONTROL_8723B);
	rtw_write8(Adapter, REG_RXDMA_CONTROL_8723B, tmpU1b&(~BIT2));

	DBG_871X("InitMAC_8723B() <====\n");

	return _SUCCESS;
}

//
//	Description:
//		ePHY Read operation on RTL8723BE
//
//	Created by Roger, 2013.05.08.
//
u2Byte
hal_MDIORead_8723BE(
	IN	PADAPTER 	Adapter,
	IN 	u1Byte		Addr
)
{
	u2Byte ret = 0;
	u1Byte tmpU1b = 0, count = 0;

	rtw_write8(Adapter, REG_MDIO_CTL_8723B, Addr|BIT6);
	tmpU1b = rtw_read8(Adapter, REG_MDIO_CTL_8723B) & BIT6 ;
	count = 0;
	while(tmpU1b && count < 20)
	{
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_MDIO_CTL_8723B) & BIT6;
		count++;
	}
	if(0 == tmpU1b)
	{
		ret = rtw_read16(Adapter, REG_MDIO_RDATA_8723B);
	}

	return ret;
	}

//
//	Description:
//		ePHY Write operation on RTL8723BE
//
//	Created by Roger, 2013.05.08.
//
VOID
hal_MDIOWrite_8723BE(
	IN	PADAPTER 	Adapter,
	IN 	u1Byte		Addr,
	IN	u2Byte		Data
)
{
	u1Byte tmpU1b = 0, count = 0;

	rtw_write16(Adapter, REG_MDIO_WDATA_8723B, Data);
	rtw_write8(Adapter, REG_MDIO_CTL_8723B, Addr|BIT5);
	tmpU1b = rtw_read8(Adapter, REG_MDIO_CTL_8723B) & BIT5 ;
	count = 0;
	while(tmpU1b && count < 20)
	{
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_MDIO_CTL_8723B) & BIT5;
		count++;
	}
}

//
//	Description:
//		PCI configuration space read operation on RTL8723BE
//
//	Created by Roger, 2013.05.08.
//
u1Byte
hal_DBIRead_8723BE(
	IN	PADAPTER 	Adapter,
	IN 	u2Byte		Addr
)
{
	u2Byte ReadAddr = Addr & 0xfffc;
	u1Byte ret = 0, tmpU1b = 0, count = 0;

	rtw_write16(Adapter, REG_DBI_ADDR_8723B, ReadAddr);
	rtw_write8(Adapter, REG_DBI_FLAG_8723B, 0x2);
	tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_8723B);
	count = 0;
	while(tmpU1b && count < 20)
	{
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_8723B);
		count++;
	}
	if(0 == tmpU1b)
	{
		ReadAddr = REG_DBI_RDATA_8723B + Addr%4;
		ret = rtw_read8(Adapter, ReadAddr);
	}

	return ret;
}

//
//	Description:
//		PCI configuration space write operation on RTL8723BE
//
//	Created by Roger, 2013.05.08.
//
VOID
hal_DBIWrite_8723BE(
	IN	PADAPTER 	Adapter,
	IN 	u2Byte		Addr,
	IN	u1Byte		Data
)
	{
	u1Byte tmpU1b = 0, count = 0;
	u2Byte WriteAddr = 0, Remainder = Addr%4;


	// Write DBI 1Byte Data
	WriteAddr = REG_DBI_WDATA_8723B + Remainder;
	rtw_write8(Adapter, WriteAddr, Data);

	// Write DBI 2Byte Address & Write Enable
	WriteAddr = (Addr & 0xfffc) | (BIT0 << (Remainder + 12));
	rtw_write16(Adapter, REG_DBI_ADDR_8723B, WriteAddr);

	// Write DBI Write Flag
		rtw_write8(Adapter, REG_DBI_FLAG_8723B, 0x1);

	tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_8723B);
	count = 0;
	while(tmpU1b && count < 20)
	{
		rtw_udelay_os(10);
		tmpU1b = rtw_read8(Adapter, REG_DBI_FLAG_8723B);
		count++;
	}
}

VOID
EnableAspmBackDoor_8723BE(IN	PADAPTER Adapter)
{

	u1Byte tmp1byte = 0;
	u2Byte tmp2byte = 0;


	//
	// <Roger_Notes> Overwrite following ePHY parameter for some platform compatibility issue,
	// especially when CLKReq is enabled, 2012.11.09.
	//
	tmp2byte = hal_MDIORead_8723BE(Adapter, 0x0b);
	if(tmp2byte!=0x70)
		hal_MDIOWrite_8723BE(Adapter, 0x0b, 0x70);

	// Configuration Space offset 0x70f BIT7 is used to control L0S
	tmp1byte = hal_DBIRead_8723BE(Adapter, 0x70f);
	hal_DBIWrite_8723BE(Adapter, 0x70f,tmp1byte|BIT7);

	// Configuration Space offset 0x719 Bit3 is for L1 BIT4 is for clock request
	tmp1byte = hal_DBIRead_8723BE(Adapter, 0x719);
	hal_DBIWrite_8723BE(Adapter, 0x719,tmp1byte|BIT3|BIT4);

}

VOID
EnableL1Off_8723BE(IN	PADAPTER Adapter)
{
	u8	tmpU1b  = 0;
	HAL_DATA_TYPE	*pHalData	= GET_HAL_DATA(Adapter);

	if(pHalData->bL1OffSupport == _FALSE)
		return;

	hal_MDIOWrite_8723BE(Adapter, 0x1b, (hal_MDIORead_8723BE(Adapter, 0x1b)|BIT4));

	tmpU1b  = hal_DBIRead_8723BE(Adapter, 0x718);
	hal_DBIWrite_8723BE(Adapter, 0x718, tmpU1b|BIT5);

	tmpU1b  = hal_DBIRead_8723BE(Adapter, 0x160);
	hal_DBIWrite_8723BE(Adapter, 0x160, tmpU1b|0xf);
}

//
// Description: Check if PCIe DMA Tx/Rx hang.
// The flow is provided by SD1 Alan. Added by tynli. 2013.05.03.
//
BOOLEAN
CheckPcieDMAHang8723BE(
	IN	PADAPTER	Adapter
)
{
	u1Byte	u1bTmp;

	//write reg 0x350 Bit[26]=1. Enable debug port.
	u1bTmp = PlatformEFIORead1Byte(Adapter, REG_DBI_CTRL+3);
	if(!(u1bTmp&BIT2))
	{
		PlatformEFIOWrite1Byte(Adapter, REG_DBI_CTRL+3, (u1bTmp|BIT2));
		rtw_mdelay_os(100); // Suggested by DD Justin_tsai.
	}

	//read reg 0x350 Bit[25] if 1 : RX hang
	//read reg 0x350 Bit[24] if 1 : TX hang
	u1bTmp = PlatformEFIORead1Byte(Adapter, REG_DBI_CTRL+3);
	if((u1bTmp&BIT0) || (u1bTmp&BIT1))
	{
		RT_TRACE(_module_hci_hal_init_c_, _drv_notice_, ("CheckPcieDMAHang8723BE(): TRUE! Reset PCIE DMA!\n"));
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//
// Description: Reset Pcie interface DMA.
//
VOID
ResetPcieInterfaceDMA8723BE(
	IN PADAPTER		Adapter,
	IN BOOLEAN		bInMACPowerOn
)
{
	u1Byte	u1Tmp;
	BOOLEAN	bReleaseMACRxPause;
	u1Byte	BackUpPcieDMAPause;

	RT_TRACE(_module_hci_hal_init_c_, _drv_notice_, ("ResetPcieInterfaceDMA8723BE()\n"));

	// Revise Note: Follow the document "PCIe RX DMA Hang Reset Flow_v03" released by SD1 Alan.
	// 2013.05.07, by tynli.

	//1. disable register write lock
	//	write 0x1C bit[1:0] = 2'h0
	//	write 0xCC bit[2] = 1'b1
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RSV_CTRL_8723B);
	u1Tmp &= ~(BIT1|BIT0);
	PlatformEFIOWrite1Byte(Adapter, REG_RSV_CTRL_8723B, u1Tmp);
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_PMC_DBG_CTRL2_8723B);
	u1Tmp |= BIT2;
	PlatformEFIOWrite1Byte(Adapter, REG_PMC_DBG_CTRL2_8723B, u1Tmp);

	//2. Check and pause TRX DMA
	//write 0x284 bit[18] = 1'b1
	//write 0x301 = 0xFF
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RXDMA_CONTROL_8723B);
	if(u1Tmp&BIT2)
	{
		// Already pause before the function for another purpose.
		bReleaseMACRxPause = FALSE;
	}
	else
	{
		PlatformEFIOWrite1Byte(Adapter, REG_RXDMA_CONTROL_8723B, (u1Tmp|BIT2));
		bReleaseMACRxPause = TRUE;
	}
	BackUpPcieDMAPause = PlatformEFIORead1Byte(Adapter,	REG_PCIE_CTRL_REG+1);
	if(BackUpPcieDMAPause != 0xFF)
	{
		PlatformEFIOWrite1Byte(Adapter, 	REG_PCIE_CTRL_REG+1, 0xFF);
	}

	if(bInMACPowerOn)
	{
		// 3. reset TRX function
		// write 0x100 = 0x00
		PlatformEFIOWrite1Byte(Adapter, REG_CR, 0);
	}

	// 4. Reset PCIe DMA
	//	write 0x003 bit[0] = 0
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_SYS_FUNC_EN+1);
	u1Tmp &= ~(BIT0);
	PlatformEFIOWrite1Byte(Adapter, REG_SYS_FUNC_EN+1, u1Tmp);

	// 5. Enable PCIe DMA
	//	write 0x003 bit[0] = 1
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_SYS_FUNC_EN+1);
	u1Tmp |= BIT0;
	PlatformEFIOWrite1Byte(Adapter, REG_SYS_FUNC_EN+1, u1Tmp);

	if(bInMACPowerOn)
	{
		// 6. enable TRX function
		// write 0x100 = 0xFF
		PlatformEFIOWrite1Byte(Adapter, REG_CR, 0xFF);

		// We should init LLT & RQPN and prepare Tx/Rx descrptor address later
		// because MAC function is reset.
	}

	// 7. Restore PCIe autoload down bit
	// write 0xF8 bit[17] = 1'b1
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_MAC_PHY_CTRL_NORMAL+2);
	u1Tmp |= BIT1;
	PlatformEFIOWrite1Byte(Adapter, REG_MAC_PHY_CTRL_NORMAL+2, u1Tmp);

	// In MAC power on state, BB and RF maybe in ON state, if we release TRx DMA here
	// it will cause packets to be started to Tx/Rx, so we release Tx/Rx DMA later.
	if(!bInMACPowerOn)
	{
		// 8. release TRX DMA
		//write 0x284 bit[18] = 1'b0
		//write 0x301 = 0x00
		if(bReleaseMACRxPause)
		{
			u1Tmp = PlatformEFIORead1Byte(Adapter, REG_RXDMA_CONTROL_8723B);
			PlatformEFIOWrite1Byte(Adapter, REG_RXDMA_CONTROL_8723B, (u1Tmp&~BIT2));
		}
		PlatformEFIOWrite1Byte(Adapter, 	REG_PCIE_CTRL_REG+1, BackUpPcieDMAPause);
	}

	//9. lock system register
	//	 write 0xCC bit[2] = 1'b0
	u1Tmp = PlatformEFIORead1Byte(Adapter, REG_PMC_DBG_CTRL2_8723B);
	u1Tmp &= ~(BIT2);
	PlatformEFIOWrite1Byte(Adapter, REG_PMC_DBG_CTRL2_8723B, u1Tmp);

}
VOID
PHY_InitAntennaSelection8723B(
	IN PADAPTER Adapter
)
{
	u8	tmpU1b;
	u32	tmpU4b;

	// TODO: <20130114, Kordan> The following setting is only for DPDT and Fixed board type.
	// TODO:  A better solution is configure it according EFUSE during the run-time.

	//PHY_SetMacReg(Adapter, 0x64, BIT20, 0x0);		   //0x66[4]=0
	tmpU1b = rtw_read8(Adapter, 0x66);
	rtw_write8(Adapter, 0x66, tmpU1b & ~( 1 << 4 ));
	//PHY_SetMacReg(Adapter, 0x64, BIT24, 0x0);		   //0x66[8]=0
	tmpU1b = rtw_read8(Adapter, 0x66);
	rtw_write8(Adapter, 0x66, tmpU1b & ~( 1 << 8 ));
	//PHY_SetMacReg(Adapter, 0x40, BIT4, 0x0);		   //0x40[4]=0
	tmpU1b = rtw_read8(Adapter, 0x40);
	rtw_write8(Adapter, 0x40, tmpU1b & ~( 1 << 4 ));
	//PHY_SetMacReg(Adapter, 0x40, BIT3, 0x1);		   //0x40[3]=1
	tmpU1b = rtw_read8(Adapter, 0x40);
	rtw_write8(Adapter, 0x40, tmpU1b | ( 1 << 3 ));
	//PHY_SetMacReg(Adapter, 0x4C, BIT24, 0x1);   	   //0x4C[24:23]=10
	tmpU4b = rtw_read32(Adapter, 0x4C);
	rtw_write32(Adapter, 0x4C, tmpU4b | ( 1 << 24 ));
	//PHY_SetMacReg(Adapter, 0x4C, BIT23, 0x0);   	   //0x4C[24:23]=10
	tmpU4b = rtw_read32(Adapter, 0x4C);
	rtw_write32(Adapter, 0x4C, tmpU4b & ~( 1 << 23 ));

	PHY_SetBBReg(Adapter, 0x944, BIT1|BIT0, 0x3);     //0x944[1:0]=11
	PHY_SetBBReg(Adapter, 0x930, bMaskByte0, 0x77);   //0x930[7:0]=77
	//PHY_SetMacReg(Adapter, 0x38, BIT11, 0x1); 	       //0x38[11]=1
	tmpU4b = rtw_read32(Adapter, 0x38);
	rtw_write32(Adapter, 0x38, tmpU4b | ( 1 << 11 ));
}

static u32 rtl8723b_hal_init(PADAPTER Adapter)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);
	u32	rtStatus = _SUCCESS;
	u8	tmpU1b, u1bRegCR;
	u32 u4Tmp;
	u32	i;
	BOOLEAN bSupportRemoteWakeUp, is2T2R;
	u32	NavUpper = WiFiNavUpperUs;
	PDM_ODM_T 			pDM_Odm = &pHalData->odmpriv;

_func_enter_;

	//
	// No I/O if device has been surprise removed
	//
	if (Adapter->bSurpriseRemoved)
	{
		DBG_8192C("rtl8723be_hal_init(): bSurpriseRemoved!\n");
		return _SUCCESS;
	}

	Adapter->init_adpt_in_progress = _TRUE;
	DBG_8192C("=======>rtl8723be_hal_init()\n");

	u1bRegCR = rtw_read8(Adapter, REG_CR);
	DBG_871X(" power-on :REG_CR 0x100=0x%02x.\n", u1bRegCR);
	if(/*(tmpU1b&BIT3) && */ (u1bRegCR != 0 && u1bRegCR != 0xEA))
	{
		//pHalData->bMACFuncEnable = TRUE;
		DBG_871X(" MAC has already power on.\n");
	}
	else
	{
		//pHalData->bMACFuncEnable = FALSE;
		// Set FwPSState to ALL_ON mode to prevent from the I/O be return because of 32k
		// state which is set before sleep under wowlan mode. 2012.01.04. by tynli.
		//pHalData->FwPSState = FW_PS_STATE_ALL_ON_88E;
	}

	if(CheckPcieDMAHang8723BE(Adapter))
	{
		//ResetPcieInterfaceDMA8723BE(Adapter, pHalData->bMACFuncEnable);
		ResetPcieInterfaceDMA8723BE(Adapter, (u1bRegCR != 0 && u1bRegCR != 0xEA));
		// Release pHalData->bMACFuncEnable here because it will reset MAC in ResetPcieInterfaceDMA
		// function such that we need to allow LLT to be initialized in later flow.
		//pHalData->bMACFuncEnable = FALSE;
	}

	//
	// 1. MAC Initialize
	//
	rtStatus = InitMAC_8723B(Adapter);
	if(rtStatus != _SUCCESS)
	{
		DBG_8192C("Init MAC failed\n");
		return rtStatus;
	}

	// <Kordan> InitHalDm should be put ahead of FirmwareDownload. (HWConfig flow: FW->MAC->-BB->RF)
	rtl8723b_InitHalDm(Adapter);

	/*
		To inform FW about:
		bit0: "0" for no antenna inverse; "1" for antenna inverse
		bit1: "0" for internal switch; "1" for external switch
		bit2: "0" for one antenna; "1" for two antenna
	*/
	tmpU1b = 0;
	if (pHalData->EEPROMBluetoothAntNum == Ant_x2)
	{
		tmpU1b |= BIT(2); /* two antenna case */
	}
	rtw_write8(Adapter, 0x384, tmpU1b);

#if HAL_FW_ENABLE
	//if ((Adapter->registrypriv.mp_mode != 1) /*|| IS_HARDWARE_TYPE_8821(Adapter)*/)
	{
		tmpU1b = rtw_read8(Adapter, REG_SYS_CFG);
		rtw_write8(Adapter, REG_SYS_CFG, tmpU1b & 0x7F);

		rtStatus = rtl8723b_FirmwareDownload(Adapter, _FALSE);

		if(rtStatus != _SUCCESS)
		{
			DBG_8192C("FwLoad failed\n");
			rtStatus = _SUCCESS;
			Adapter->bFWReady = _FALSE;
			pHalData->fw_ractrl = _FALSE;
		}
		else
		{
			DBG_8192C("FwLoad SUCCESSFULLY!!!\n");
			Adapter->bFWReady = _TRUE;
			pHalData->fw_ractrl = _TRUE;
		}
		rtl8723b_InitializeFirmwareVars(Adapter);
	}
#endif

	if(pHalData->AMPDUBurstMode)
	{
		rtw_write8(Adapter,REG_AMPDU_BURST_MODE_8723B,  0x7F);
	}

	pHalData->CurrentChannel = 0;//set 0 to trigger switch correct channel

	// We should call the function before MAC/BB configuration.
	PHY_InitAntennaSelection8723B(Adapter);

	//
	// 2. Initialize MAC/PHY Config by MACPHY_reg.txt
	//
#if (HAL_MAC_ENABLE == 1)
	DBG_871X("8723B MAC Config Start!\n");
	rtStatus = PHY_MACConfig8723B(Adapter);
	if (rtStatus != _SUCCESS)
	{
		DBG_871X("8723B MAC Config failed\n");
		return rtStatus;
	}
	DBG_871X("8723B MAC Config Finished!\n");

	// without this statement, RCR doesn't take effect so that it will show 'validate_recv_ctrl_frame fail'.
	rtw_write32(Adapter,REG_RCR, rtw_read32(Adapter, REG_RCR)&~(RCR_ADF) );
#endif	// #if (HAL_MAC_ENABLE == 1)

	//
	// 3. Initialize BB After MAC Config PHY_reg.txt, AGC_Tab.txt
	//
#if (HAL_BB_ENABLE == 1)
	DBG_871X("BB Config Start!\n");
	rtStatus = PHY_BBConfig8723B(Adapter);
	if (rtStatus!= _SUCCESS)
	{
		DBG_871X("BB Config failed\n");
		return rtStatus;
	}

	if( Adapter->registrypriv.wifi_spec==1 )  {
		ODM_SetBBReg(pDM_Odm, 0xc4c, bMaskDWord, 0x00fe0301);
	}

	DBG_871X("BB Config Finished!\n");
#endif	// #if (HAL_BB_ENABLE == 1)

	//
	// 4. Initiailze RF RAIO_A.txt RF RAIO_B.txt
	//
#if (HAL_RF_ENABLE == 1)
	DBG_871X("RF Config started!\n");
	rtStatus = PHY_RFConfig8723B(Adapter);
	if(rtStatus != _SUCCESS)
	{
		DBG_871X("RF Config failed\n");
		return rtStatus;
	}
	DBG_871X("RF Config Finished!\n");

        pHalData->RfRegChnlVal[0] = PHY_QueryRFReg(Adapter, 0, RF_CHNLBW, bRFRegOffsetMask);
        pHalData->RfRegChnlVal[1] = PHY_QueryRFReg(Adapter, 1, RF_CHNLBW, bRFRegOffsetMask);
        // Defalut BW shoule be 20MH at first for the value. because HAL bandwidh defalut = 20MH.
        pHalData->RfRegChnlVal[0] &= 0xFFF03FF;
        pHalData->RfRegChnlVal[0] |= (BIT10 | BIT11);

#endif	// #if (HAL_RF_ENABLE == 1)

	//3 Set Hardware(MAC default setting.)
	HwConfigureRTL8723B(Adapter);

	rtw_hal_set_chnl_bw(Adapter, Adapter->registrypriv.channel,
		CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

	// We should set pHalData->bMACFuncEnable to TURE after LLT initialize and Fw download
	// and before GPIO detect. by tynli. 2011.05.27.
	//pHalData->bMACFuncEnable = _TRUE;

	//3Security related
	//-----------------------------------------------------------------------------
	// Set up security related. 070106, by rcnjko:
	// 1. Clear all H/W keys.
	//-----------------------------------------------------------------------------
	invalidate_cam_all(Adapter);

	// Joseph debug: MAC_SEC_EN need to be set
	rtw_write8(Adapter, REG_CR+1, (rtw_read8(Adapter, REG_CR+1)|BIT1));

	//2=======================================================
	// RF Power Save
	//2=======================================================
	// Fix the bug that Hw/Sw radio off before S3/S4, the RF off action will not be executed
	// in MgntActSet_RF_State() after wake up, because the value of pHalData->eRFPowerState
	// is the same as eRfOff, we should change it to eRfOn after we config RF parameters.
	// Added by tynli. 2010.03.30.
	pwrpriv->rf_pwrstate = rf_on;

	//if(pPSC->bGpioRfSw)
	if(0)
	{
		tmpU1b = rtw_read8(Adapter, 0x60);
		pwrpriv->rfoff_reason |= (tmpU1b & BIT1) ? 0 : RF_CHANGE_BY_HW;
	}

	pwrpriv->rfoff_reason |= (pwrpriv->reg_rfoff) ? RF_CHANGE_BY_SW : 0;

	if(pwrpriv->rfoff_reason&RF_CHANGE_BY_HW)
		pwrpriv->b_hw_radio_off = _TRUE;

	if(pwrpriv->rfoff_reason > RF_CHANGE_BY_PS)
	{ // H/W or S/W RF OFF before sleep.
		DBG_8192C("InitializeAdapter8188EE(): Turn off RF for RfOffReason(%d) ----------\n", pwrpriv->rfoff_reason);
		//MgntActSet_RF_State(Adapter, rf_off, pwrpriv->rfoff_reason, _TRUE);
	}
	else
	{
		pwrpriv->rf_pwrstate = rf_on;
		pwrpriv->rfoff_reason = 0;

		DBG_8192C("InitializeAdapter8723BE(): Turn on  ----------\n");

		// LED control
		rtw_led_control(Adapter, LED_CTL_POWER_ON);

	}

	rtl8723b_InitAntenna_Selection(Adapter);

	// Fix the bug that when the system enters S3/S4 then tirgger HW radio off, after system
	// wakes up, the scan OID will be set from upper layer, but we still in RF OFF state and scan
	// list is empty, such that the system might consider the NIC is in RF off state and will wait
	// for several seconds (during this time the scan OID will not be set from upper layer anymore)
	// even though we have already HW RF ON, so we tell the upper layer our RF state here.
	// Added by tynli. 2010.04.01.
	//DrvIFIndicateCurrentPhyStatus(Adapter);

	if(Adapter->registrypriv.hw_wps_pbc)
	{
		tmpU1b = rtw_read8(Adapter, GPIO_IO_SEL);
		tmpU1b &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
		rtw_write8(Adapter, GPIO_IO_SEL, tmpU1b);	//enable GPIO[2] as input mode
	}

	//
	// Execute TX power tracking later
	//

	// We must set MAC address after firmware download. HW do not support MAC addr
	// autoload now.
	hal_init_macaddr(Adapter);

	EnableAspmBackDoor_8723BE(Adapter);
	EnableL1Off_8723BE(Adapter);

	// Init BT hw config.

	//rtl8723b_InitHalDm(Adapter);

	Adapter->init_adpt_in_progress = _FALSE;

#if defined(CONFIG_CONCURRENT_MODE) || defined(CONFIG_TX_MCAST2UNI)

#ifdef CONFIG_CHECK_AC_LIFETIME
	// Enable lifetime check for the four ACs
	rtw_write8(Adapter, REG_LIFETIME_CTRL, 0x0F);
#endif	// CONFIG_CHECK_AC_LIFETIME

#ifdef CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x0400);	// unit: 256us. 256ms
#else	// CONFIG_TX_MCAST2UNI
	rtw_write16(Adapter, REG_PKT_VO_VI_LIFE_TIME, 0x3000);	// unit: 256us. 3s
	rtw_write16(Adapter, REG_PKT_BE_BK_LIFE_TIME, 0x3000);	// unit: 256us. 3s
#endif	// CONFIG_TX_MCAST2UNI
#endif	// CONFIG_CONCURRENT_MODE || CONFIG_TX_MCAST2UNI


	//enable tx DMA to drop the redundate data of packet
	rtw_write16(Adapter,REG_TXDMA_OFFSET_CHK, (rtw_read16(Adapter,REG_TXDMA_OFFSET_CHK) | DROP_DATA_EN));

	pHalData->RegBcnCtrlVal = rtw_read8(Adapter, REG_BCN_CTRL);
	pHalData->RegTxPause = rtw_read8(Adapter, REG_TXPAUSE);
	pHalData->RegFwHwTxQCtrl = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL+2);
	pHalData->RegReg542 = rtw_read8(Adapter, REG_TBTT_PROHIBIT+2);

	//EnableInterrupt8188EE(Adapter);

#if (MP_DRIVER == 1)
	if (Adapter->registrypriv.mp_mode == 1)
	{
		Adapter->mppriv.channel = pHalData->CurrentChannel;
		MPT_InitializeAdapter(Adapter, Adapter->mppriv.channel);
	}
	else
#endif
	{
		struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(Adapter);

		pwrctrlpriv->rf_pwrstate = rf_on;

		if(pwrctrlpriv->rf_pwrstate == rf_on)
		{
			struct pwrctrl_priv *pwrpriv;
			u32 start_time;
			u8 restore_iqk_rst;
			u8 b2Ant;
			u8 h2cCmdBuf;

			pwrpriv = adapter_to_pwrctl(Adapter);

			PHY_LCCalibrate_8723B(&pHalData->odmpriv);

			/* Inform WiFi FW that it is the beginning of IQK */
			h2cCmdBuf = 1;
			FillH2CCmd8723B(Adapter, H2C_8723B_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			start_time = rtw_get_current_time();
			do {
				if (rtw_read8(Adapter, 0x1e7) & 0x01)
					break;

				rtw_msleep_os(50);
			} while (rtw_get_passing_time_ms(start_time) <= 400);

#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_IQKNotify(Adapter, _TRUE);
#endif
			restore_iqk_rst = (pwrpriv->bips_processing == _TRUE)?_TRUE:_FALSE;
			b2Ant = pHalData->EEPROMBluetoothAntNum==Ant_x2?_TRUE:_FALSE;
			PHY_IQCalibrate_8723B(Adapter, _FALSE, restore_iqk_rst, b2Ant, pHalData->ant_path);
			pHalData->bIQKInitialized = _TRUE;
#ifdef CONFIG_BT_COEXIST
			rtw_btcoex_IQKNotify(Adapter, _FALSE);
#endif

			/* Inform WiFi FW that it is the finish of IQK */
			h2cCmdBuf = 0;
			FillH2CCmd8723B(Adapter, H2C_8723B_BT_WLAN_CALIBRATION, 1, &h2cCmdBuf);

			ODM_TXPowerTrackingCheck(&pHalData->odmpriv);
		}
	}

#ifdef CONFIG_BT_COEXIST
	// Init BT hw config.
	rtw_btcoex_HAL_Initialize(Adapter, _FALSE);
#else
	rtw_btcoex_HAL_Initialize(Adapter, _TRUE);
#endif

	is2T2R = IS_2T2R(pHalData->VersionID);

	#if(defined(CONFIG_ANT_DETECTION))
	ODM_SingleDualAntennaDefaultSetting(pDM_Odm);
	#endif

/*
	tmpU1b = EFUSE_Read1Byte(Adapter, 0x1FA);

	if(!(tmpU1b & BIT0))
	{
		PHY_SetRFReg(Adapter, ODM_RF_PATH_A, 0x15, 0x0F, 0x05);
		DBG_871X("PA BIAS path A\n");
	}

	if(!(tmpU1b & BIT1) && is2T2R)
	{
		PHY_SetRFReg(Adapter, ODM_RF_PATH_B, 0x15, 0x0F, 0x05);
		DBG_871X("PA BIAS path B\n");
	}
*/

	rtw_hal_set_hwreg(Adapter, HW_VAR_NAV_UPPER, ((u8 *)&NavUpper));


/*{
	DBG_8192C("===== Start Dump Reg =====");
	for(i = 0 ; i <= 0xeff ; i+=4)
	{
#if 1
		u32 tmp;

		if(i%16==0)
			printk("\n%04x: ",i);

		tmp = rtw_read32(Adapter, i);

		printk("%02X %02X %02X %02X ",
				( tmp >> 0 ) & 0xFF,
				( tmp >> 8 ) & 0xFF,
				( tmp >> 16 ) & 0xFF,
				( tmp >> 24 ) & 0xFF
				);
#else
		if(i%16==0)
			DBG_8192C("\n%04x: ",i);
		DBG_8192C("0x%08x ",rtw_read32(Adapter, i));
#endif
	}
	DBG_8192C("\n ===== End Dump Reg =====\n");
}*/

_func_exit_;

	return rtStatus;
}

//
// 2009/10/13 MH Acoording to documetn form Scott/Alfred....
// This is based on version 8.1.
//




VOID
hal_poweroff_8723BE(
	IN	PADAPTER			Adapter
)
{
	u8	u1bTmp;
	u8 bMacPwrCtrlOn = _FALSE;

	rtw_hal_get_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
	if(bMacPwrCtrlOn == _FALSE)
		return  ;

	DBG_871X("=====>%s\n",__FUNCTION__);

	//pHalData->bMACFuncEnable = _FALSE;

	// Run LPS WL RFOFF flow
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723B_enter_lps_flow);



	// 0x1F[7:0] = 0		// turn off RF
	//rtw_write8(Adapter, REG_RF_CTRL_8812, 0x00);

	//	==== Reset digital sequence   ======
	if((rtw_read8(Adapter, REG_MCUFWDL)&BIT7) && Adapter->bFWReady) //8051 RAM code
	{
		_8051Reset8723(Adapter);
	}

	// Reset MCU. Suggested by Filen. 2011.01.26. by tynli.
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN+1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN+1, (u1bTmp&(~BIT2)));

	// MCUFWDL 0x80[1:0]=0				// reset MCU ready status
	rtw_write8(Adapter, REG_MCUFWDL, 0x00);

	// HW card disable configuration.
	HalPwrSeqCmdParsing(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8723B_card_disable_flow);

	// Reset MCU IO Wrapper
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL+1);
	rtw_write8(Adapter, REG_RSV_CTRL+1, (u1bTmp&(~BIT0)));
	u1bTmp = rtw_read8(Adapter, REG_RSV_CTRL+1);
	rtw_write8(Adapter, REG_RSV_CTRL+1, u1bTmp|BIT0);

	// RSV_CTRL 0x1C[7:0] = 0x0E			// lock ISO/CLK/Power control register
	rtw_write8(Adapter, REG_RSV_CTRL, 0x0e);

	Adapter->bFWReady = _FALSE;
	bMacPwrCtrlOn = _FALSE;
	rtw_hal_set_hwreg(Adapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
}

VOID
PowerOffAdapter8723B(
	IN	PADAPTER			Adapter
	)
{
	hal_poweroff_8723BE(Adapter);
}

static u32 rtl8723be_hal_deinit(PADAPTER Adapter)
{
	u8	u1bTmp = 0;
	u32 	count = 0;
	u8	bSupportRemoteWakeUp = _FALSE;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv		*pwrpriv = adapter_to_pwrctl(Adapter);

_func_enter_;

	DBG_871X("====> %s() \n", __FUNCTION__);

	if (Adapter->bHaltInProgress == _TRUE)
	{
		DBG_8192C("====> Abort rtl8723be_hal_deinit()\n");
		return _FAIL;
	}

	Adapter->bHaltInProgress = _TRUE;

	//
	// No I/O if device has been surprise removed
	//
	if (Adapter->bSurpriseRemoved)
	{
		Adapter->bHaltInProgress = _FALSE;
		return _SUCCESS;
	}

	RT_SET_PS_LEVEL(pwrpriv, RT_RF_OFF_LEVL_HALT_NIC);

	// Without supporting WoWLAN or the driver is in awake (D0) state, we should
	if(!bSupportRemoteWakeUp )//||!pMgntInfo->bPwrSaveState)
	{
		// 2009/10/13 MH For power off test.
		PowerOffAdapter8723B(Adapter);
	}
#if 0
	else
	{
		s32	rtStatus = _SUCCESS;

		//--------------------------------------------------------
		//3 <1> Prepare for configuring wowlan related infomations
		//--------------------------------------------------------

		// Clear Fw WoWLAN event.
		rtw_write8(Adapter, REG_MCUTST_WOWLAN, 0x0);

#if (USE_SPECIFIC_FW_TO_SUPPORT_WOWLAN == 1)
		SetFwRelatedForWoWLAN8812A(Adapter, _TRUE);
#endif

		SetWoWLANCAMEntry8812(Adapter);
		PlatformEFIOWrite1Byte(Adapter, REG_WKFMCAM_NUM_8812, pPSC->WoLPatternNum);
		RT_TRACE(COMP_INIT, DBG_LOUD, ("Number of WoL pattern filled in CAM = %d\n", pPSC->WoLPatternNum));

		// Dynamically adjust Tx packet boundary for download reserved page packet.
		rtStatus = DynamicRQPN8812AE(Adapter, 0xE0, 0x3, 0x80c20d0d); // reserve 30 pages for rsvd page
		if(rtStatus == RT_STATUS_SUCCESS)
		{
			pHalData->bReInitLLTTable = _TRUE;
		}

		//--------------------------------------------------------
		//3 <2> Set Fw releted H2C cmd.
		//--------------------------------------------------------

		// Set WoWLAN related security information.
		SetFwGlobalInfoCmd_8812(Adapter);

		HalDownloadRSVDPage8812E(Adapter, _TRUE);


		// Just enable AOAC related functions when we connect to AP.
		if(pMgntInfo->mAssoc && pMgntInfo->OpMode == RT_OP_MODE_INFRASTRUCTURE)
		{
			// Set Join Bss Rpt H2C cmd and download RSVD page.
			// Fw will check media status to send null packet and perform WoWLAN LPS.
			Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_AID, 0);
			MacIdIndicateSpecificMediaStatus(Adapter, MAC_ID_STATIC_FOR_DEFAULT_PORT, RT_MEDIA_CONNECT);

			HalSetFWWoWlanMode8812(Adapter, _TRUE);
			// Enable Fw Keep alive mechanism.
			HalSetFwKeepAliveCmd8812(Adapter, _TRUE);

			//Enable disconnect decision control.
			SetFwDisconnectDecisionCtrlCmd_8812(Adapter, _TRUE);
		}

		//--------------------------------------------------------
		//3 <3> Hw Configutations
		//--------------------------------------------------------

		//Wait untill Rx DMA Finished before host sleep. FW Pause Rx DMA may happens when received packet doing dma.  //YJ,add,111101
		 rtw_write8(Adapter, REG_RXDMA_CONTROL_8812, BIT2);

		u1bTmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812);
		count=0;
		while(!(u1bTmp & BIT(1)) &&(count++ < 100))
		{
			rtw_udelay_os(10); // 10 us
			u1bTmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812);
		}
		DBG_871X("HaltAdapter8812E(): 222 Wait untill Rx DMA Finished before host sleep. count=%d\n", count);

		rtw_write8(Adapter, REG_APS_FSMCO+1, 0x0);

		PlatformClearPciPMEStatus(Adapter);

		// tynli_test for normal chip wowlan. 2010.01.26. Suggested by Sd1 Isaac and designer Alfred.
		rtw_write8(Adapter, REG_SYS_CLKR, (rtw_read8(Adapter, REG_SYS_CLKR)|BIT3));

		//prevent 8051 to be reset by PERST#
		rtw_write8(Adapter, REG_RSV_CTRL, 0x20);
		rtw_write8(Adapter, REG_RSV_CTRL, 0x60);
	}

	// For wowlan+LPS+32k.
	if(bSupportRemoteWakeUp && Adapter->bEnterPnpSleep)
	{
		BOOLEAN		bEnterFwLPS = TRUE;
		u1Byte		QueueID;
		PRT_TCB 	pTcb;


		// Set the WoWLAN related function control enable.
		// It should be the last H2C cmd in the WoWLAN flow. 2012.02.10. by tynli.
		SetFwRemoteWakeCtrlCmd_8723B(Adapter, 1);

		// Stop Pcie Interface Tx DMA.
		PlatformEFIOWrite1Byte(Adapter, REG_PCIE_CTRL_REG_8723B+1, 0xff);
		RT_TRACE(COMP_POWER, DBG_LOUD, ("Stop PCIE Tx DMA.\n"));

		// Wait for TxDMA idle.
		count = 0;
		do
		{
			u1bTmp = PlatformEFIORead1Byte(Adapter, REG_PCIE_CTRL_REG_8723B);
			PlatformSleepUs(10); // 10 us
			count++;
		}while((u1bTmp != 0) && (count < 100));
		RT_TRACE(COMP_INIT, DBG_LOUD, ("HaltAdapter8188EE(): Wait untill Tx DMA Finished before host sleep. count=%d\n", count));

		// Set Fw to enter LPS mode.
		if(	pMgntInfo->mAssoc &&
			pMgntInfo->OpMode == RT_OP_MODE_INFRASTRUCTURE &&
			(pPSC->WoWLANLPSLevel > 0))
		{
			RT_TRACE(COMP_POWER, DBG_LOUD, ("FwLPS: Sleep!!\n"));
			Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_FW_LPS_ACTION, (pu1Byte)(&bEnterFwLPS));
			pPSC->bEnterLPSDuringSuspend = TRUE;
		}

		PlatformAcquireSpinLock(Adapter, RT_TX_SPINLOCK);
		// guangan, 2009/08/28, return TCB in busy queue to idle queue when resume from S3/S4.
		for(QueueID = 0; QueueID < MAX_TX_QUEUE; QueueID++)
		{
			// 2004.08.11, revised by rcnjko.
			while(!RTIsListEmpty(&Adapter->TcbBusyQueue[QueueID]))
			{
				pTcb=(PRT_TCB)RTRemoveHeadList(&Adapter->TcbBusyQueue[QueueID]);
				ReturnTCB(Adapter, pTcb, RT_STATUS_SUCCESS);
			}
		}

		PlatformReleaseSpinLock(Adapter, RT_TX_SPINLOCK);

		if(pHalData->HwROFEnable)
		{
			u1bTmp = PlatformEFIORead1Byte(Adapter, REG_HSISR_8812+3);
			PlatformEFIOWrite1Byte(Adapter, REG_HSISR_8812+3, u1bTmp|BIT1);
		}
	}
#endif

	DBG_871X("%s() <====\n",__FUNCTION__);

	Adapter->bHaltInProgress = _FALSE;

_func_exit_;

	return _SUCCESS;
}

void SetHwReg8723BE(PADAPTER Adapter, u8 variable, u8* val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

_func_enter_;

	switch(variable)
	{
		case HW_VAR_SET_RPWM:
#ifdef CONFIG_LPS_LCLK
			{
				u8	ps_state = *((u8 *)val);
				//rpwm value only use BIT0(clock bit) ,BIT6(Ack bit), and BIT7(Toggle bit) for 88e.
				//BIT0 value - 1: 32k, 0:40MHz.
				//BIT6 value - 1: report cpwm value after success set, 0:do not report.
				//BIT7 value - Toggle bit change.
				//modify by Thomas. 2012/4/2.
				ps_state = ps_state & 0xC1;
				//DBG_871X("##### Change RPWM value to = %x for switch clk #####\n",ps_state);
				rtw_write8(Adapter, REG_PCIE_HRPWM, ps_state);
			}
#endif
			break;
		case HW_VAR_AMPDU_MAX_TIME:
			{
				u8	maxRate = *(u8 *)val;

				rtw_write8(Adapter, REG_AMPDU_MAX_TIME_8723B, 0x70);
			}
			break;
		case HW_VAR_PCIE_STOP_TX_DMA:
			rtw_write16(Adapter, REG_PCIE_CTRL_REG, 0xff00);
			break;
		case HW_VAR_DM_IN_LPS:	// [NOTE] copy from 8723bs
			rtl8723b_hal_dm_in_lps(Adapter);
			break;
		default:
			SetHwReg8723B(Adapter, variable, val);
			break;
	}

_func_exit_;
}

void GetHwReg8723BE(PADAPTER Adapter, u8 variable, u8* val)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	//DM_ODM_T 		*podmpriv = &pHalData->odmpriv;
_func_enter_;

	switch(variable)
	{
		default:
			GetHwReg8723B(Adapter,variable,val);
			break;
	}

_func_exit_;
}

//
//	Description:
//		Change default setting of specified variable.
//
u8
SetHalDefVar8723BE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch(eVariable)
	{
		case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:
			pHalData->bSupportBackDoor = *((PBOOLEAN)pValue);
			break;
		default:
			SetHalDefVar8723B(Adapter,eVariable,pValue);
			break;
	}

	return bResult;
}

//
//	Description:
//		Query setting of specified variable.
//
u8
GetHalDefVar8723BE(
	IN	PADAPTER				Adapter,
	IN	HAL_DEF_VARIABLE		eVariable,
	IN	PVOID					pValue
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			bResult = _SUCCESS;

	switch(eVariable)
	{
		case HAL_DEF_PCI_SUUPORT_L1_BACKDOOR:
			*((PBOOLEAN)pValue) = pHalData->bSupportBackDoor;
			break;

		case HAL_DEF_PCI_AMD_L1_SUPPORT:
			*((PBOOLEAN)pValue) = _TRUE;// Support L1 patch on AMD platform in default, added by Roger, 2012.04.30.
			break;

#ifdef CONFIG_ANTENNA_DIVERSITY
		case HAL_DEF_IS_SUPPORT_ANT_DIV:
			*((u8*)pValue) = (pHalData->AntDivCfg==0) ? _FALSE : _TRUE;
			break;
#endif

#ifdef CONFIG_ANTENNA_DIVERSITY
		case HAL_DEF_CURRENT_ANTENNA:
			*((u8*)pValue) = pHalData->CurAntenna;
			break;
#endif

		case HAL_DEF_DRVINFO_SZ:
			*((u32*)pValue) = DRVINFO_SZ;
			break;

		default:
			GetHalDefVar8723B(Adapter,eVariable,pValue);
			break;
	}

	return bResult;
}

static void rtl8723be_init_default_value(_adapter * padapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	rtl8723b_init_default_value(padapter);

	pHalData->CurrentWirelessMode = WIRELESS_MODE_AUTO;
	pHalData->bDefaultAntenna = 1;

	//
	// Set TCR-Transmit Control Register. The value is set in InitializeAdapter8190Pci()
	//
	pHalData->TransmitConfig = CFENDFORM | BIT12 | BIT13;

	//
	// Set RCR-Receive Control Register . The value is set in InitializeAdapter8190Pci().
	//
	pHalData->ReceiveConfig = (\
								/*RCR_APPFCS			|*/
								RCR_APP_MIC			|
								RCR_APP_ICV			|
								RCR_APP_PHYST_RXFF	|
								/*RCR_NONQOS_VHT	|*/
								RCR_HTC_LOC_CTRL	|
								RCR_AMF				|
								/*RCR_ACF				|*/
								/*RCR_ADF				|*/ /* Note: This bit controls the PS-Poll packet filter. */
								RCR_AICV			|
								/*RCR_ACRC32			|*/
								RCR_AB				|
								RCR_AM				|
								RCR_APM				|
								0);

	//
	// Set Interrupt Mask Register
	//
	pHalData->IntrMaskDefault[0]	= (u32)(			\
								IMR_PSTIMEOUT_8723B		|
								//IMR_GTINT4_8723B			|
								IMR_GTINT3_8723B			|
								IMR_TXBCN0ERR_8723B		|
								IMR_TXBCN0OK_8723B		|
								IMR_BCNDMAINT0_8723B	|
								IMR_HSISR_IND_ON_INT_8723B	|
								IMR_C2HCMD_8723B		|
								//IMR_CPWM_8723B			|
								IMR_HIGHDOK_8723B		|
								IMR_MGNTDOK_8723B		|
								IMR_BKDOK_8723B			|
								IMR_BEDOK_8723B			|
								IMR_VIDOK_8723B			|
								IMR_VODOK_8723B			|
								IMR_RDU_8723B			|
								IMR_ROK_8723B			|
								0);
	pHalData->IntrMaskDefault[1] 	= (u32)(\
								IMR_RXFOVW_8723B		|
								IMR_TXFOVW_8723B		|
								0);

	// 2012/03/27 hpfan Add for win8 DTM DPC ISR test
	pHalData->IntrMaskReg[0]	=	(u32)(	\
								IMR_RDU_8723B		|
								IMR_PSTIMEOUT_8723B	|
								0);
	pHalData->IntrMaskReg[1]	=	(u32)(	\
								IMR_C2HCMD_8723B		|
								0);

	//if (padapter->bUseThreadHandleInterrupt)
	//{
	//	pHalData->IntrMask[0] =pHalData->IntrMaskReg[0];
	//	pHalData->IntrMask[1] =pHalData->IntrMaskReg[1];
	//}
	//else
	{
		pHalData->IntrMask[0]= pHalData->IntrMaskDefault[0];
		pHalData->IntrMask[1]= pHalData->IntrMaskDefault[1];
	}


	if(1)//IS_HARDWARE_TYPE_8723BE(padapter))
	{

		pHalData->SysIntrMask[0] = (u32)(			\
								HSIMR_PDN_INT_EN		|
								HSIMR_RON_INT_EN		|
								0);
	}
}

void rtl8723be_set_hal_ops(_adapter * padapter)
{
	struct hal_ops	*pHalFunc = &padapter->HalFunc;

_func_enter_;

	rtl8723b_set_hal_ops(pHalFunc);

	pHalFunc->hal_power_on = &_InitPowerOn_8723BE;
	pHalFunc->hal_power_off = &hal_poweroff_8723BE;
	pHalFunc->hal_init = &rtl8723b_hal_init;
	pHalFunc->hal_deinit = &rtl8723be_hal_deinit;

	pHalFunc->inirp_init = &rtl8723be_init_desc_ring;
	pHalFunc->inirp_deinit = &rtl8723be_free_desc_ring;
	pHalFunc->irp_reset = &rtl8723be_reset_desc_ring;

	pHalFunc->init_xmit_priv = &rtl8723be_init_xmit_priv;
	pHalFunc->free_xmit_priv = &rtl8723be_free_xmit_priv;

	pHalFunc->init_recv_priv = &rtl8723be_init_recv_priv;
	pHalFunc->free_recv_priv = &rtl8723be_free_recv_priv;
#ifdef CONFIG_SW_LED
	pHalFunc->InitSwLeds = &rtl8723be_InitSwLeds;
	pHalFunc->DeInitSwLeds = &rtl8723be_DeInitSwLeds;
#else //case of hw led or no led
	pHalFunc->InitSwLeds = NULL;
	pHalFunc->DeInitSwLeds = NULL;
#endif//CONFIG_SW_LED

	pHalFunc->init_default_value = &rtl8723be_init_default_value;
	pHalFunc->intf_chip_configure = &rtl8723be_interface_configure;
	pHalFunc->read_adapter_info = &ReadAdapterInfo8723BE;

	pHalFunc->enable_interrupt = &EnableInterrupt8723BE;
	pHalFunc->disable_interrupt = &DisableInterrupt8723BE;
	pHalFunc->interrupt_handler = &rtl8723be_interrupt;

	pHalFunc->SetHwRegHandler = &SetHwReg8723BE;
	pHalFunc->GetHwRegHandler = &GetHwReg8723BE;
	pHalFunc->GetHalDefVarHandler = &GetHalDefVar8723BE;
	pHalFunc->SetHalDefVarHandler = &SetHalDefVar8723BE;

	pHalFunc->hal_xmit = &rtl8723be_hal_xmit;
	pHalFunc->mgnt_xmit = &rtl8723be_mgnt_xmit;
	pHalFunc->hal_xmitframe_enqueue = &rtl8723be_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	pHalFunc->hostap_mgnt_xmit_entry = &rtl8723be_hostap_mgnt_xmit_entry;
#endif

#ifdef CONFIG_XMIT_THREAD_MODE
	pHalFunc->xmit_thread_handler = &rtl8723be_xmit_buf_handler;
#endif

_func_exit_;

}
