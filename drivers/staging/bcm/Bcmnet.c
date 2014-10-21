#include "headers.h"

struct net_device *gblpnetdev;

static INT bcm_open(struct net_device *dev)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);

	if (ad->fw_download_done == false) {
		pr_notice(PFX "%s: link up failed (download in progress)\n",
			  dev->name);
		return -EBUSY;
	}

	if (netif_msg_ifup(ad))
		pr_info(PFX "%s: enabling interface\n", dev->name);

	if (ad->LinkUpStatus) {
		if (netif_msg_link(ad))
			pr_info(PFX "%s: link up\n", dev->name);

		netif_carrier_on(ad->dev);
		netif_start_queue(ad->dev);
	}

	return 0;
}

static INT bcm_close(struct net_device *dev)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);

	if (netif_msg_ifdown(ad))
		pr_info(PFX "%s: disabling interface\n", dev->name);

	netif_carrier_off(dev);
	netif_stop_queue(dev);

	return 0;
}

static u16 bcm_select_queue(struct net_device *dev, struct sk_buff *skb,
			    void *accel_priv, select_queue_fallback_t fallback)
{
	return ClassifyPacket(netdev_priv(dev), skb);
}

/*******************************************************************
* Function    -	bcm_transmit()
*
* Description - This is the main transmit function for our virtual
*		interface(eth0). It handles the ARP packets. It
*		clones this packet and then Queue it to a suitable
*		Queue. Then calls the transmit_packet().
*
* Parameter   -	 skb - Pointer to the socket buffer structure
*		 dev - Pointer to the virtual net device structure
*
*********************************************************************/

static netdev_tx_t bcm_transmit(struct sk_buff *skb, struct net_device *dev)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);
	u16 qindex = skb_get_queue_mapping(skb);


	if (ad->device_removed || !ad->LinkUpStatus)
		goto drop;

	if (ad->TransferMode != IP_PACKET_ONLY_MODE)
		goto drop;

	if (INVALID_QUEUE_INDEX == qindex)
		goto drop;

	if (ad->PackInfo[qindex].uiCurrentPacketsOnHost >=
	    SF_MAX_ALLOWED_PACKETS_TO_BACKUP)
		return NETDEV_TX_BUSY;

	/* Now Enqueue the packet */
	if (netif_msg_tx_queued(ad))
		pr_info(PFX "%s: enqueueing packet to queue %d\n",
			dev->name, qindex);

	spin_lock(&ad->PackInfo[qindex].SFQueueLock);
	ad->PackInfo[qindex].uiCurrentBytesOnHost += skb->len;
	ad->PackInfo[qindex].uiCurrentPacketsOnHost++;

	*((B_UINT32 *) skb->cb + SKB_CB_LATENCY_OFFSET) = jiffies;
	ENQUEUEPACKET(ad->PackInfo[qindex].FirstTxQueue,
		      ad->PackInfo[qindex].LastTxQueue, skb);
	atomic_inc(&ad->TotalPacketCount);
	spin_unlock(&ad->PackInfo[qindex].SFQueueLock);

	/* FIXME - this is racy and incorrect, replace with work queue */
	if (!atomic_read(&ad->TxPktAvail)) {
		atomic_set(&ad->TxPktAvail, 1);
		wake_up(&ad->tx_packet_wait_queue);
	}
	return NETDEV_TX_OK;

 drop:
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}



/**
@ingroup init_functions
Register other driver entry points with the kernel
*/
static const struct net_device_ops bcmNetDevOps = {
	.ndo_open		= bcm_open,
	.ndo_stop		= bcm_close,
	.ndo_start_xmit	        = bcm_transmit,
	.ndo_change_mtu	        = eth_change_mtu,
	.ndo_set_mac_address    = eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_select_queue	= bcm_select_queue,
};

static struct device_type wimax_type = {
	.name	= "wimax",
};

static int bcm_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	cmd->supported		= 0;
	cmd->advertising	= 0;
	cmd->speed		= SPEED_10000;
	cmd->duplex		= DUPLEX_FULL;
	cmd->port		= PORT_TP;
	cmd->phy_address	= 0;
	cmd->transceiver	= XCVR_INTERNAL;
	cmd->autoneg		= AUTONEG_DISABLE;
	cmd->maxtxpkt		= 0;
	cmd->maxrxpkt		= 0;
	return 0;
}

static void bcm_get_drvinfo(struct net_device *dev,
			    struct ethtool_drvinfo *info)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);
	struct bcm_interface_adapter *intf_ad = ad->pvInterfaceAdapter;
	struct usb_device *udev = interface_to_usbdev(intf_ad->interface);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	snprintf(info->fw_version, sizeof(info->fw_version), "%u.%u",
		 ad->uiFlashLayoutMajorVersion,
		 ad->uiFlashLayoutMinorVersion);

	usb_make_path(udev, info->bus_info, sizeof(info->bus_info));
}

static u32 bcm_get_link(struct net_device *dev)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);

	return ad->LinkUpStatus;
}

static u32 bcm_get_msglevel(struct net_device *dev)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);

	return ad->msg_enable;
}

static void bcm_set_msglevel(struct net_device *dev, u32 level)
{
	struct bcm_mini_adapter *ad = GET_BCM_ADAPTER(dev);

	ad->msg_enable = level;
}

static const struct ethtool_ops bcm_ethtool_ops = {
	.get_settings	= bcm_get_settings,
	.get_drvinfo	= bcm_get_drvinfo,
	.get_link	= bcm_get_link,
	.get_msglevel	= bcm_get_msglevel,
	.set_msglevel	= bcm_set_msglevel,
};

int register_networkdev(struct bcm_mini_adapter *ad)
{
	struct net_device *net = ad->dev;
	struct bcm_interface_adapter *intf_ad = ad->pvInterfaceAdapter;
	struct usb_interface *udev = intf_ad->interface;
	struct usb_device *xdev = intf_ad->udev;

	int result;

	net->netdev_ops = &bcmNetDevOps;
	net->ethtool_ops = &bcm_ethtool_ops;
	net->mtu = MTU_SIZE;	/* 1400 Bytes */
	net->tx_queue_len = TX_QLEN;
	net->flags |= IFF_NOARP;

	netif_carrier_off(net);

	SET_NETDEV_DEVTYPE(net, &wimax_type);

	/* Read the MAC Address from EEPROM */
	result = ReadMacAddressFromNVM(ad);
	if (result != STATUS_SUCCESS) {
		dev_err(&udev->dev,
			PFX "Error in Reading the mac Address: %d", result);
		return -EIO;
	}

	result = register_netdev(net);
	if (result)
		return result;

	gblpnetdev = ad->dev;

	if (netif_msg_probe(ad))
		dev_info(&udev->dev, PFX "%s: register usb-%s-%s %pM\n",
			 net->name, xdev->bus->bus_name, xdev->devpath,
			 net->dev_addr);

	return 0;
}

void unregister_networkdev(struct bcm_mini_adapter *ad)
{
	struct net_device *net = ad->dev;
	struct bcm_interface_adapter *intf_ad = ad->pvInterfaceAdapter;
	struct usb_interface *udev = intf_ad->interface;
	struct usb_device *xdev = intf_ad->udev;

	if (netif_msg_probe(ad))
		dev_info(&udev->dev, PFX "%s: unregister usb-%s%s\n",
			 net->name, xdev->bus->bus_name, xdev->devpath);

	unregister_netdev(ad->dev);
}
