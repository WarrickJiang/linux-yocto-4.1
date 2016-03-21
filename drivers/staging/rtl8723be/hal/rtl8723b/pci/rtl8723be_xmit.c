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
#define _RTL8723BE_XMIT_C_

//#include <drv_types.h>
#include <rtl8723b_hal.h>

static void _dbg_dump_tx_info(_adapter	*padapter,int frame_tag, u8 *ptxdesc)
{
	u8 bDumpTxPkt;
	u8 bDumpTxDesc = _FALSE;
	rtw_hal_get_def_var(padapter, HAL_DEF_DBG_DUMP_TXPKT, &(bDumpTxPkt));

	if(bDumpTxPkt ==1){//dump txdesc for data frame
		DBG_871X("dump tx_desc for data frame\n");
		if((frame_tag&0x0f) == DATA_FRAMETAG){
			bDumpTxDesc = _TRUE;
		}
	}
	else if(bDumpTxPkt ==2){//dump txdesc for mgnt frame
		DBG_871X("dump tx_desc for mgnt frame\n");
		if((frame_tag&0x0f) == MGNT_FRAMETAG){
			bDumpTxDesc = _TRUE;
		}
	}
	else if(bDumpTxPkt ==3){//dump early info
	}

	if(bDumpTxDesc){
		//	ptxdesc->txdw4 = cpu_to_le32(0x00001006);//RTS Rate=24M
		//	ptxdesc->txdw6 = 0x6666f800;
		DBG_8192C("=====================================\n");
		DBG_8192C("Offset00(0x%08x)\n",*((u32 *)(ptxdesc)));
		DBG_8192C("Offset04(0x%08x)\n",*((u32 *)(ptxdesc+4)));
		DBG_8192C("Offset08(0x%08x)\n",*((u32 *)(ptxdesc+8)));
		DBG_8192C("Offset12(0x%08x)\n",*((u32 *)(ptxdesc+12)));
		DBG_8192C("Offset16(0x%08x)\n",*((u32 *)(ptxdesc+16)));
		DBG_8192C("Offset20(0x%08x)\n",*((u32 *)(ptxdesc+20)));
		DBG_8192C("Offset24(0x%08x)\n",*((u32 *)(ptxdesc+24)));
		DBG_8192C("Offset28(0x%08x)\n",*((u32 *)(ptxdesc+28)));
		DBG_8192C("Offset32(0x%08x)\n",*((u32 *)(ptxdesc+32)));
		DBG_8192C("Offset36(0x%08x)\n",*((u32 *)(ptxdesc+36)));
		DBG_8192C("=====================================\n");
	}

}

s32	rtl8723be_init_xmit_priv(_adapter *padapter)
{
	s32	ret = _SUCCESS;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	_rtw_spinlock_init(&pdvobjpriv->irq_th_lock);

#ifdef PLATFORM_LINUX
	tasklet_init(&pxmitpriv->xmit_tasklet,
	     (void(*)(unsigned long))rtl8723be_xmit_tasklet,
	     (unsigned long)padapter);
#endif

	return ret;
}

void	rtl8723be_free_xmit_priv(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	_rtw_spinlock_free(&pdvobjpriv->irq_th_lock);
}

s32 rtl8723be_enqueue_xmitbuf(struct rtw_tx_ring	*ring, struct xmit_buf *pxmitbuf)
{
	_irqL irqL;
	_queue *ppending_queue = &ring->queue;

_func_enter_;

	//DBG_8192C("+enqueue_xmitbuf\n");

	if(pxmitbuf==NULL)
	{
		return _FAIL;
	}

	//_enter_critical(&ppending_queue->lock, &irqL);

	rtw_list_delete(&pxmitbuf->list);

	rtw_list_insert_tail(&(pxmitbuf->list), get_list_head(ppending_queue));

	ring->qlen++;

	//DBG_8192C("FREE, free_xmitbuf_cnt=%d\n", pxmitpriv->free_xmitbuf_cnt);

	//_exit_critical(&ppending_queue->lock, &irqL);

_func_exit_;

	return _SUCCESS;
}

struct xmit_buf * rtl8723be_dequeue_xmitbuf(struct rtw_tx_ring	*ring)
{
	_irqL irqL;
	_list *plist, *phead;
	struct xmit_buf *pxmitbuf =  NULL;
	_queue *ppending_queue = &ring->queue;

_func_enter_;

	//_enter_critical(&ppending_queue->lock, &irqL);

	if(_rtw_queue_empty(ppending_queue) == _TRUE) {
		pxmitbuf = NULL;
	} else {

		phead = get_list_head(ppending_queue);

		plist = get_next(phead);

		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);

		rtw_list_delete(&(pxmitbuf->list));

		ring->qlen--;
	}

	//_exit_critical(&ppending_queue->lock, &irqL);

_func_exit_;

	return pxmitbuf;
}

static u16 ffaddr2dma(u32 addr)
{
	u16	dma_ctrl;
	switch(addr)
	{
		case VO_QUEUE_INX:
			dma_ctrl = BIT3;
			break;
		case VI_QUEUE_INX:
			dma_ctrl = BIT2;
			break;
		case BE_QUEUE_INX:
			dma_ctrl = BIT1;
			break;
		case BK_QUEUE_INX:
			dma_ctrl = BIT0;
			break;
		case BCN_QUEUE_INX:
			dma_ctrl = BIT4;
			break;
		case MGT_QUEUE_INX:
			dma_ctrl = BIT6;
			break;
		case HIGH_QUEUE_INX:
			dma_ctrl = BIT7;
			break;
		default:
			dma_ctrl = 0;
			break;
	}

	return dma_ctrl;
}

static s32 update_txdesc(struct xmit_frame *pxmitframe, u8 *pmem, s32 sz)
{
	_adapter			*padapter = pxmitframe->padapter;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	dma_addr_t		mapping;
	u8				*ptxdesc =  pmem;

	mapping = pci_map_single(pdvobjpriv->ppcidev, pxmitframe->buf_addr , sz, PCI_DMA_TODEVICE);

	_rtw_memset(ptxdesc, 0, TX_DESC_NEXT_DESC_OFFSET);

	rtl8723b_update_txdesc( pxmitframe, ptxdesc );

	// Don't set value before, because rtl8723b_update_txdesc() will clean ptxdesc.

	//4 offset 0
	SET_TX_DESC_FIRST_SEG_8723B(ptxdesc, 1);
	SET_TX_DESC_LAST_SEG_8723B(ptxdesc, 1);
	//SET_TX_DESC_OWN_8723B(ptxdesc, 1);

	SET_TX_DESC_TX_BUFFER_SIZE_8723B(ptxdesc, sz);
	SET_TX_DESC_TX_BUFFER_ADDRESS_8723B(ptxdesc, mapping);

	// debug
	_dbg_dump_tx_info(padapter,pxmitframe->frame_tag,ptxdesc);

	return 0;
}

static u8 *get_txdesc(_adapter *padapter, u8 queue_index)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;
	u8	*pdesc = NULL;
	int	idx;

	ring = &pxmitpriv->tx_ring[queue_index];

	if (queue_index != BCN_QUEUE_INX) {
		idx = (ring->idx + ring->qlen) % ring->entries;
	} else {
		idx = 0;
	}

	pdesc = (u8 *)&ring->desc[idx];

	if((GET_TX_DESC_OWN_8723B(pdesc)) && (queue_index != BCN_QUEUE_INX)) {
		DBG_8192C("No more TX desc@%d, ring->idx = %d,idx = %d \n", queue_index, ring->idx, idx);
		return NULL;
	}

	return pdesc;
}

s32 rtw_dump_xframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 ret = _SUCCESS;
	s32 inner_ret = _SUCCESS;
       _irqL irqL;
	int	t, sz, w_sz, pull=0;
	u32	ff_hwaddr;
	struct xmit_buf		*pxmitbuf = pxmitframe->pxmitbuf;
	struct pkt_attrib		*pattrib = &pxmitframe->attrib;
	struct xmit_priv		*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv		*pdvobjpriv = adapter_to_dvobj(padapter);
	struct security_priv	*psecuritypriv = &padapter->securitypriv;
	u8					*ptxdesc;

#ifdef CONFIG_CONCURRENT_MODE
	_adapter 	  *pri_adapter;
	struct dvobj_priv *pri_dvobjpriv;
#else
	_adapter 	  * const pri_adapter = padapter;
	struct dvobj_priv * const pri_dvobjpriv = pdvobjpriv;
#endif


	if ((pxmitframe->frame_tag == DATA_FRAMETAG) &&
	    (pxmitframe->attrib.ether_type != 0x0806) &&
	    (pxmitframe->attrib.ether_type != 0x888e) &&
	    (pxmitframe->attrib.dhcp_pkt != 1))
	{
		rtw_issue_addbareq_cmd(padapter, pxmitframe);
	}

	//mem_addr = pxmitframe->buf_addr;

       RT_TRACE(_module_rtl871x_xmit_c_,_drv_info_,("rtw_dump_xframe()\n"));

	for (t = 0; t < pattrib->nr_frags; t++)
	{
		if (inner_ret != _SUCCESS && ret ==_SUCCESS)
			ret = _FAIL;

		if (t != (pattrib->nr_frags - 1))
		{
			RT_TRACE(_module_rtl871x_xmit_c_,_drv_err_,("pattrib->nr_frags=%d\n", pattrib->nr_frags));

			sz = pxmitpriv->frag_len;
			sz = sz - 4 - (psecuritypriv->sw_encrypt ? 0 : pattrib->icv_len);
		}
		else //no frag
		{
			sz = pattrib->last_txcmdsz;
		}

		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);

		// decide primary adapter
#ifdef CONFIG_CONCURRENT_MODE
		if(padapter->adapter_type > PRIMARY_ADAPTER)
		{
			pri_adapter = padapter->pbuddy_adapter;
			pri_dvobjpriv = adapter_to_dvobj(pri_adapter);
		} else {
			pri_adapter = padapter;
			pri_dvobjpriv = pdvobjpriv;
		}
#endif

		{
			_enter_critical(&(pri_dvobjpriv->irq_th_lock), &irqL);

			ptxdesc = get_txdesc(pri_adapter, ff_hwaddr);
			if (BCN_QUEUE_INX == ff_hwaddr)
				padapter->xmitpriv.beaconDMAing = _TRUE;

			if(ptxdesc == NULL)
			{
				_exit_critical(&pri_dvobjpriv->irq_th_lock, &irqL);
				rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_TX_DESC_NA);
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				DBG_8192C("##### Tx desc unavailable !#####\n");
				break;
			}
			update_txdesc(pxmitframe, ptxdesc, sz);
			if (pxmitbuf->buf_tag != XMITBUF_CMD)
				rtl8723be_enqueue_xmitbuf(&(pri_adapter->xmitpriv.tx_ring[ff_hwaddr]), pxmitbuf);

			pxmitbuf->len = sz;
			w_sz = sz;

			wmb();
			SET_TX_DESC_OWN_8723B(ptxdesc, 1);

			_exit_critical(&pri_dvobjpriv->irq_th_lock, &irqL);

			rtw_write16(pri_adapter, REG_PCIE_CTRL_REG, ffaddr2dma(ff_hwaddr));
			inner_ret = rtw_write_port(padapter, ff_hwaddr, w_sz, (unsigned char*)pxmitbuf);
		}


		rtw_count_tx_stats(padapter, pxmitframe, sz);
		RT_TRACE(_module_rtl871x_xmit_c_,_drv_info_,("rtw_write_port, w_sz=%d\n", w_sz));
		//DBG_8192C("rtw_write_port, w_sz=%d, sz=%d, txdesc_sz=%d, tid=%d\n", w_sz, sz, w_sz-sz, pattrib->priority);

		//mem_addr += w_sz;

		//mem_addr = (u8 *)RND4(((SIZE_PTR)(mem_addr)));

	}

	rtw_free_xmitframe(pxmitpriv, pxmitframe);

	if  (ret != _SUCCESS)
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_UNKNOWN);

	return ret;
}

void rtl8723be_xmitframe_resume(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame *pxmitframe = NULL;
	struct xmit_buf	*pxmitbuf = NULL;
	int res=_SUCCESS, xcnt = 0;

	RT_TRACE(_module_rtl871x_xmit_c_,_drv_info_,("rtl8188ee_xmitframe_resume()\n"));

	while(1)
	{
		if ((padapter->bDriverStopped == _TRUE)||(padapter->bSurpriseRemoved== _TRUE))
		{
			DBG_8192C("rtl8188ee_xmitframe_resume => bDriverStopped or bSurpriseRemoved \n");
			break;
		}

		pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
		if(!pxmitbuf)
		{
			break;
		}

		pxmitframe =  rtw_dequeue_xframe(pxmitpriv, pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);

		if(pxmitframe)
		{
			pxmitframe->pxmitbuf = pxmitbuf;

			pxmitframe->buf_addr = pxmitbuf->pbuf;

			pxmitbuf->priv_data = pxmitframe;

			if((pxmitframe->frame_tag&0x0f) == DATA_FRAMETAG)
			{
				if(pxmitframe->attrib.priority<=15)//TID0~15
				{
					res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
				}

				rtw_os_xmit_complete(padapter, pxmitframe);//always return ndis_packet after rtw_xmitframe_coalesce
			}

			RT_TRACE(_module_rtl871x_xmit_c_,_drv_info_,("rtl8188ee_xmitframe_resume(): rtw_dump_xframe\n"));

			if(res == _SUCCESS)
			{
				rtw_dump_xframe(padapter, pxmitframe);
			}
			else
			{
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				rtw_free_xmitframe(pxmitpriv, pxmitframe);
			}

			xcnt++;
		}
		else
		{
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			break;
		}
	}
}

static u8 check_nic_enough_desc(_adapter *padapter, struct pkt_attrib *pattrib)
{
	u32 prio;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;

	switch(pattrib->qsel)
	{
		case 0:
		case 3:
			prio = BE_QUEUE_INX;
			break;
		case 1:
		case 2:
			prio = BK_QUEUE_INX;
			break;
		case 4:
		case 5:
			prio = VI_QUEUE_INX;
			break;
		case 6:
		case 7:
			prio = VO_QUEUE_INX;
			break;
		default:
			prio = BE_QUEUE_INX;
			break;
	}

	ring = &pxmitpriv->tx_ring[prio];

	// for now we reserve two free descriptor as a safety boundary
	// between the tail and the head
	//
	if ((ring->entries - ring->qlen) >= 2) {
		return _TRUE;
	} else {
		//DBG_8192C("do not have enough desc for Tx \n");
		return _FALSE;
	}
}

static s32 xmitframe_direct(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 res = _SUCCESS;

	res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
	if (res == _SUCCESS) {
		rtw_dump_xframe(padapter, pxmitframe);
	}

	return res;
}

/*
 * Return
 *	_TRUE	dump packet directly
 *	_FALSE	enqueue packet
 */
static s32 pre_xmitframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
        _irqL irqL;
	s32 res;
	struct xmit_buf *pxmitbuf = NULL;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

#ifdef CONFIG_CONCURRENT_MODE
	PADAPTER pbuddy_adapter = padapter->pbuddy_adapter;
	struct mlme_priv *pbuddy_mlmepriv = &(pbuddy_adapter->mlmepriv);
#endif // CONFIG_CONCURRENT_MODE

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	if ( (rtw_txframes_sta_ac_pending(padapter, pattrib) > 0) ||
		(check_nic_enough_desc(padapter, pattrib) == _FALSE))
		goto enqueue;


	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY) == _TRUE)
		goto enqueue;

#ifdef CONFIG_CONCURRENT_MODE
	if (check_fwstate(pbuddy_mlmepriv, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE)
		goto enqueue;
#endif // CONFIG_CONCURRENT_MODE
	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	if (pxmitbuf == NULL)
		goto enqueue;

	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	pxmitframe->pxmitbuf = pxmitbuf;
	pxmitframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pxmitframe;

	if (xmitframe_direct(padapter, pxmitframe) != _SUCCESS) {
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
	}

	return _TRUE;

enqueue:
	res = rtw_xmitframe_enqueue(padapter, pxmitframe);
	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	if (res != _SUCCESS) {
		RT_TRACE(_module_xmit_osdep_c_, _drv_err_, ("pre_xmitframe: enqueue xmitframe fail\n"));
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		// Trick, make the statistics correct
		pxmitpriv->tx_pkts--;
		pxmitpriv->tx_drop++;
		return _TRUE;
	}

	return _FALSE;
}

s32 rtl8723be_mgnt_xmit(_adapter *padapter, struct xmit_frame *pmgntframe)
{
	return rtw_dump_xframe(padapter, pmgntframe);
}

/*
 * Return
 *	_TRUE	dump packet directly ok
 *	_FALSE	temporary can't transmit packets to hardware
 */
s32	rtl8723be_hal_xmit(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	return pre_xmitframe(padapter, pxmitframe);
}

s32	rtl8723be_hal_xmitframe_enqueue(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv 	*pxmitpriv = &padapter->xmitpriv;
	s32 err;

	if ((err=rtw_xmitframe_enqueue(padapter, pxmitframe)) != _SUCCESS)
	{
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		// Trick, make the statistics correct
		pxmitpriv->tx_pkts--;
		pxmitpriv->tx_drop++;
	}
	else
	{
		if (check_nic_enough_desc(padapter, &pxmitframe->attrib) == _TRUE) {
#ifdef PLATFORM_LINUX
			tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif
		}
	}

	return err;
}

#ifdef  CONFIG_HOSTAPD_MLME

static void rtl8723be_hostap_mgnt_xmit_cb(struct urb *urb)
{
#ifdef PLATFORM_LINUX
	struct sk_buff *skb = (struct sk_buff *)urb->context;

	//DBG_8192C("%s\n", __FUNCTION__);

	rtw_skb_free(skb);
#endif
}

s32 rtl8723be_hostap_mgnt_xmit_entry(_adapter *padapter, _pkt *pkt)
{
#ifdef PLATFORM_LINUX
	u16 fc;
	int rc, len, pipe;
	unsigned int bmcst, tid, qsel;
	struct sk_buff *skb, *pxmit_skb;
	struct urb *urb;
	unsigned char *pxmitbuf;
	struct tx_desc *ptxdesc;
	struct rtw_ieee80211_hdr *tx_hdr;
	struct hostapd_priv *phostapdpriv = padapter->phostapdpriv;
	struct net_device *pnetdev = padapter->pnetdev;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pdvobj = adapter_to_dvobj(padapter);


	//DBG_8192C("%s\n", __FUNCTION__);

	skb = pkt;

	len = skb->len;
	tx_hdr = (struct rtw_ieee80211_hdr *)(skb->data);
	fc = le16_to_cpu(tx_hdr->frame_ctl);
	bmcst = IS_MCAST(tx_hdr->addr1);

	if ((fc & RTW_IEEE80211_FCTL_FTYPE) != RTW_IEEE80211_FTYPE_MGMT)
		goto _exit;

	pxmit_skb = rtw_skb_alloc(len + TXDESC_SIZE);

	if(!pxmit_skb)
		goto _exit;

	pxmitbuf = pxmit_skb->data;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		goto _exit;
	}

	// ----- fill tx desc -----
	ptxdesc = (struct tx_desc *)pxmitbuf;
	_rtw_memset(ptxdesc, 0, sizeof(*ptxdesc));

	//offset 0
	ptxdesc->txdw0 |= cpu_to_le32(len&0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE+OFFSET_SZ)<<OFFSET_SHT)&0x00ff0000);//default = 32 bytes for TX Desc
	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

	if(bmcst)
	{
		ptxdesc->txdw0 |= cpu_to_le32(BIT(24));
	}

	//offset 4
	ptxdesc->txdw1 |= cpu_to_le32(0x00);//MAC_ID

	ptxdesc->txdw1 |= cpu_to_le32((0x12<<QSEL_SHT)&0x00001f00);

	ptxdesc->txdw1 |= cpu_to_le32((0x06<< 16) & 0x000f0000);//b mode

	//offset 8

	//offset 12
	ptxdesc->txdw3 |= cpu_to_le32((le16_to_cpu(tx_hdr->seq_ctl)<<16)&0xffff0000);

	//offset 16
	ptxdesc->txdw4 |= cpu_to_le32(BIT(8));//driver uses rate

	//offset 20

	rtl8188e_cal_txdesc_chksum(ptxdesc);
	// ----- end of fill tx desc -----

	//
	skb_put(pxmit_skb, len + TXDESC_SIZE);
	pxmitbuf = pxmitbuf + TXDESC_SIZE;
	_rtw_memcpy(pxmitbuf, skb->data, len);

	//DBG_8192C("mgnt_xmit, len=%x\n", pxmit_skb->len);


	// ----- prepare urb for submit -----

	//translate DMA FIFO addr to pipehandle
	//pipe = ffaddr2pipehdl(pdvobj, MGT_QUEUE_INX);
	pipe = usb_sndbulkpipe(pdvobj->pusbdev, pHalData->Queue2EPNum[(u8)MGT_QUEUE_INX]&0x0f);

	usb_fill_bulk_urb(urb, pdvobj->pusbdev, pipe,
			  pxmit_skb->data, pxmit_skb->len, rtl8723be_hostap_mgnt_xmit_cb, pxmit_skb);

	urb->transfer_flags |= URB_ZERO_PACKET;
	usb_anchor_urb(urb, &phostapdpriv->anchored);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		usb_unanchor_urb(urb);
		kfree_skb(skb);
	}
	usb_free_urb(urb);


_exit:

	rtw_skb_free(skb);

#endif

	return 0;

}
#endif
