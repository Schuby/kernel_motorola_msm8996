/**
  * This file contains the handling of command
  * responses as well as events generated by firmware.
  */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/unaligned.h>
#include <net/cfg80211.h>

#include "cfg.h"
#include "cmd.h"

/**
 *  @brief This function handles disconnect event. it
 *  reports disconnect to upper layer, clean tx/rx packets,
 *  reset link state etc.
 *
 *  @param priv    A pointer to struct lbs_private structure
 *  @return 	   n/a
 */
void lbs_mac_event_disconnected(struct lbs_private *priv)
{
	if (priv->connect_status != LBS_CONNECTED)
		return;

	lbs_deb_enter(LBS_DEB_ASSOC);

	/*
	 * Cisco AP sends EAP failure and de-auth in less than 0.5 ms.
	 * It causes problem in the Supplicant
	 */
	msleep_interruptible(1000);
	lbs_send_disconnect_notification(priv);

	/* report disconnect to upper layer */
	netif_stop_queue(priv->dev);
	netif_carrier_off(priv->dev);

	/* Free Tx and Rx packets */
	kfree_skb(priv->currenttxskb);
	priv->currenttxskb = NULL;
	priv->tx_pending_len = 0;

	priv->connect_status = LBS_DISCONNECTED;

	if (priv->psstate != PS_STATE_FULL_POWER) {
		/* make firmware to exit PS mode */
		lbs_deb_cmd("disconnected, so exit PS mode\n");
		lbs_ps_wakeup(priv, 0);
	}
	lbs_deb_leave(LBS_DEB_ASSOC);
}

static int lbs_ret_reg_access(struct lbs_private *priv,
			       u16 type, struct cmd_ds_command *resp)
{
	int ret = 0;

	lbs_deb_enter(LBS_DEB_CMD);

	switch (type) {
	case CMD_RET(CMD_MAC_REG_ACCESS):
		{
			struct cmd_ds_mac_reg_access *reg = &resp->params.macreg;

			priv->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
			priv->offsetvalue.value = le32_to_cpu(reg->value);
			break;
		}

	case CMD_RET(CMD_BBP_REG_ACCESS):
		{
			struct cmd_ds_bbp_reg_access *reg = &resp->params.bbpreg;

			priv->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
			priv->offsetvalue.value = reg->value;
			break;
		}

	case CMD_RET(CMD_RF_REG_ACCESS):
		{
			struct cmd_ds_rf_reg_access *reg = &resp->params.rfreg;

			priv->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
			priv->offsetvalue.value = reg->value;
			break;
		}

	default:
		ret = -1;
	}

	lbs_deb_leave_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}

static inline int handle_cmd_response(struct lbs_private *priv,
				      struct cmd_header *cmd_response)
{
	struct cmd_ds_command *resp = (struct cmd_ds_command *) cmd_response;
	int ret = 0;
	unsigned long flags;
	uint16_t respcmd = le16_to_cpu(resp->command);

	lbs_deb_enter(LBS_DEB_HOST);

	switch (respcmd) {
	case CMD_RET(CMD_MAC_REG_ACCESS):
	case CMD_RET(CMD_BBP_REG_ACCESS):
	case CMD_RET(CMD_RF_REG_ACCESS):
		ret = lbs_ret_reg_access(priv, respcmd, resp);
		break;

	case CMD_RET(CMD_802_11_SET_AFC):
	case CMD_RET(CMD_802_11_GET_AFC):
		spin_lock_irqsave(&priv->driver_lock, flags);
		memmove((void *)priv->cur_cmd->callback_arg, &resp->params.afc,
			sizeof(struct cmd_ds_802_11_afc));
		spin_unlock_irqrestore(&priv->driver_lock, flags);

		break;

	case CMD_RET(CMD_802_11_BEACON_STOP):
		break;

	case CMD_RET(CMD_802_11_RSSI):
		ret = lbs_ret_802_11_rssi(priv, resp);
		break;

	case CMD_RET(CMD_802_11_TPC_CFG):
		spin_lock_irqsave(&priv->driver_lock, flags);
		memmove((void *)priv->cur_cmd->callback_arg, &resp->params.tpccfg,
			sizeof(struct cmd_ds_802_11_tpc_cfg));
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		break;

	case CMD_RET(CMD_BT_ACCESS):
		spin_lock_irqsave(&priv->driver_lock, flags);
		if (priv->cur_cmd->callback_arg)
			memcpy((void *)priv->cur_cmd->callback_arg,
			       &resp->params.bt.addr1, 2 * ETH_ALEN);
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		break;
	case CMD_RET(CMD_FWT_ACCESS):
		spin_lock_irqsave(&priv->driver_lock, flags);
		if (priv->cur_cmd->callback_arg)
			memcpy((void *)priv->cur_cmd->callback_arg, &resp->params.fwt,
			       sizeof(resp->params.fwt));
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		break;
	case CMD_RET(CMD_802_11_BEACON_CTRL):
		ret = lbs_ret_802_11_bcn_ctrl(priv, resp);
		break;

	default:
		lbs_pr_err("CMD_RESP: unknown cmd response 0x%04x\n",
			   le16_to_cpu(resp->command));
		break;
	}
	lbs_deb_leave(LBS_DEB_HOST);
	return ret;
}

int lbs_process_command_response(struct lbs_private *priv, u8 *data, u32 len)
{
	uint16_t respcmd, curcmd;
	struct cmd_header *resp;
	int ret = 0;
	unsigned long flags;
	uint16_t result;

	lbs_deb_enter(LBS_DEB_HOST);

	mutex_lock(&priv->lock);
	spin_lock_irqsave(&priv->driver_lock, flags);

	if (!priv->cur_cmd) {
		lbs_deb_host("CMD_RESP: cur_cmd is NULL\n");
		ret = -1;
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		goto done;
	}

	resp = (void *)data;
	curcmd = le16_to_cpu(priv->cur_cmd->cmdbuf->command);
	respcmd = le16_to_cpu(resp->command);
	result = le16_to_cpu(resp->result);

	lbs_deb_cmd("CMD_RESP: response 0x%04x, seq %d, size %d\n",
		     respcmd, le16_to_cpu(resp->seqnum), len);
	lbs_deb_hex(LBS_DEB_CMD, "CMD_RESP", (void *) resp, len);

	if (resp->seqnum != priv->cur_cmd->cmdbuf->seqnum) {
		lbs_pr_info("Received CMD_RESP with invalid sequence %d (expected %d)\n",
			    le16_to_cpu(resp->seqnum), le16_to_cpu(priv->cur_cmd->cmdbuf->seqnum));
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		ret = -1;
		goto done;
	}
	if (respcmd != CMD_RET(curcmd) &&
	    respcmd != CMD_RET_802_11_ASSOCIATE && curcmd != CMD_802_11_ASSOCIATE) {
		lbs_pr_info("Invalid CMD_RESP %x to command %x!\n", respcmd, curcmd);
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		ret = -1;
		goto done;
	}

	if (resp->result == cpu_to_le16(0x0004)) {
		/* 0x0004 means -EAGAIN. Drop the response, let it time out
		   and be resubmitted */
		lbs_pr_info("Firmware returns DEFER to command %x. Will let it time out...\n",
			    le16_to_cpu(resp->command));
		spin_unlock_irqrestore(&priv->driver_lock, flags);
		ret = -1;
		goto done;
	}

	/* Now we got response from FW, cancel the command timer */
	del_timer(&priv->command_timer);
	priv->cmd_timed_out = 0;

	/* Store the response code to cur_cmd_retcode. */
	priv->cur_cmd_retcode = result;

	if (respcmd == CMD_RET(CMD_802_11_PS_MODE)) {
		struct cmd_ds_802_11_ps_mode *psmode = (void *) &resp[1];
		u16 action = le16_to_cpu(psmode->action);

		lbs_deb_host(
		       "CMD_RESP: PS_MODE cmd reply result 0x%x, action 0x%x\n",
		       result, action);

		if (result) {
			lbs_deb_host("CMD_RESP: PS command failed with 0x%x\n",
				    result);
			/*
			 * We should not re-try enter-ps command in
			 * ad-hoc mode. It takes place in
			 * lbs_execute_next_command().
			 */
			if (priv->wdev->iftype == NL80211_IFTYPE_MONITOR &&
			    action == CMD_SUBCMD_ENTER_PS)
				priv->psmode = LBS802_11POWERMODECAM;
		} else if (action == CMD_SUBCMD_ENTER_PS) {
			priv->needtowakeup = 0;
			priv->psstate = PS_STATE_AWAKE;

			lbs_deb_host("CMD_RESP: ENTER_PS command response\n");
			if (priv->connect_status != LBS_CONNECTED) {
				/*
				 * When Deauth Event received before Enter_PS command
				 * response, We need to wake up the firmware.
				 */
				lbs_deb_host(
				       "disconnected, invoking lbs_ps_wakeup\n");

				spin_unlock_irqrestore(&priv->driver_lock, flags);
				mutex_unlock(&priv->lock);
				lbs_ps_wakeup(priv, 0);
				mutex_lock(&priv->lock);
				spin_lock_irqsave(&priv->driver_lock, flags);
			}
		} else if (action == CMD_SUBCMD_EXIT_PS) {
			priv->needtowakeup = 0;
			priv->psstate = PS_STATE_FULL_POWER;
			lbs_deb_host("CMD_RESP: EXIT_PS command response\n");
		} else {
			lbs_deb_host("CMD_RESP: PS action 0x%X\n", action);
		}

		lbs_complete_command(priv, priv->cur_cmd, result);
		spin_unlock_irqrestore(&priv->driver_lock, flags);

		ret = 0;
		goto done;
	}

	/* If the command is not successful, cleanup and return failure */
	if ((result != 0 || !(respcmd & 0x8000))) {
		lbs_deb_host("CMD_RESP: error 0x%04x in command reply 0x%04x\n",
		       result, respcmd);
		/*
		 * Handling errors here
		 */
		switch (respcmd) {
		case CMD_RET(CMD_GET_HW_SPEC):
		case CMD_RET(CMD_802_11_RESET):
			lbs_deb_host("CMD_RESP: reset failed\n");
			break;

		}
		lbs_complete_command(priv, priv->cur_cmd, result);
		spin_unlock_irqrestore(&priv->driver_lock, flags);

		ret = -1;
		goto done;
	}

	spin_unlock_irqrestore(&priv->driver_lock, flags);

	if (priv->cur_cmd && priv->cur_cmd->callback) {
		ret = priv->cur_cmd->callback(priv, priv->cur_cmd->callback_arg,
				resp);
	} else
		ret = handle_cmd_response(priv, resp);

	spin_lock_irqsave(&priv->driver_lock, flags);

	if (priv->cur_cmd) {
		/* Clean up and Put current command back to cmdfreeq */
		lbs_complete_command(priv, priv->cur_cmd, result);
	}
	spin_unlock_irqrestore(&priv->driver_lock, flags);

done:
	mutex_unlock(&priv->lock);
	lbs_deb_leave_args(LBS_DEB_HOST, "ret %d", ret);
	return ret;
}

int lbs_process_event(struct lbs_private *priv, u32 event)
{
	int ret = 0;
	struct cmd_header cmd;

	lbs_deb_enter(LBS_DEB_CMD);

	switch (event) {
	case MACREG_INT_CODE_LINK_SENSED:
		lbs_deb_cmd("EVENT: link sensed\n");
		break;

	case MACREG_INT_CODE_DEAUTHENTICATED:
		lbs_deb_cmd("EVENT: deauthenticated\n");
		lbs_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_DISASSOCIATED:
		lbs_deb_cmd("EVENT: disassociated\n");
		lbs_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_LINK_LOST_NO_SCAN:
		lbs_deb_cmd("EVENT: link lost\n");
		lbs_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_PS_SLEEP:
		lbs_deb_cmd("EVENT: ps sleep\n");

		/* handle unexpected PS SLEEP event */
		if (priv->psstate == PS_STATE_FULL_POWER) {
			lbs_deb_cmd(
			       "EVENT: in FULL POWER mode, ignoreing PS_SLEEP\n");
			break;
		}
		priv->psstate = PS_STATE_PRE_SLEEP;

		lbs_ps_confirm_sleep(priv);

		break;

	case MACREG_INT_CODE_HOST_AWAKE:
		lbs_deb_cmd("EVENT: host awake\n");
		if (priv->reset_deep_sleep_wakeup)
			priv->reset_deep_sleep_wakeup(priv);
		priv->is_deep_sleep = 0;
		lbs_cmd_async(priv, CMD_802_11_WAKEUP_CONFIRM, &cmd,
				sizeof(cmd));
		priv->is_host_sleep_activated = 0;
		wake_up_interruptible(&priv->host_sleep_q);
		break;

	case MACREG_INT_CODE_DEEP_SLEEP_AWAKE:
		if (priv->reset_deep_sleep_wakeup)
			priv->reset_deep_sleep_wakeup(priv);
		lbs_deb_cmd("EVENT: ds awake\n");
		priv->is_deep_sleep = 0;
		priv->wakeup_dev_required = 0;
		wake_up_interruptible(&priv->ds_awake_q);
		break;

	case MACREG_INT_CODE_PS_AWAKE:
		lbs_deb_cmd("EVENT: ps awake\n");
		/* handle unexpected PS AWAKE event */
		if (priv->psstate == PS_STATE_FULL_POWER) {
			lbs_deb_cmd(
			       "EVENT: In FULL POWER mode - ignore PS AWAKE\n");
			break;
		}

		priv->psstate = PS_STATE_AWAKE;

		if (priv->needtowakeup) {
			/*
			 * wait for the command processing to finish
			 * before resuming sending
			 * priv->needtowakeup will be set to FALSE
			 * in lbs_ps_wakeup()
			 */
			lbs_deb_cmd("waking up ...\n");
			lbs_ps_wakeup(priv, 0);
		}
		break;

	case MACREG_INT_CODE_MIC_ERR_UNICAST:
		lbs_deb_cmd("EVENT: UNICAST MIC ERROR\n");
		lbs_send_mic_failureevent(priv, event);
		break;

	case MACREG_INT_CODE_MIC_ERR_MULTICAST:
		lbs_deb_cmd("EVENT: MULTICAST MIC ERROR\n");
		lbs_send_mic_failureevent(priv, event);
		break;

	case MACREG_INT_CODE_MIB_CHANGED:
		lbs_deb_cmd("EVENT: MIB CHANGED\n");
		break;
	case MACREG_INT_CODE_INIT_DONE:
		lbs_deb_cmd("EVENT: INIT DONE\n");
		break;
	case MACREG_INT_CODE_ADHOC_BCN_LOST:
		lbs_deb_cmd("EVENT: ADHOC beacon lost\n");
		break;
	case MACREG_INT_CODE_RSSI_LOW:
		lbs_pr_alert("EVENT: rssi low\n");
		break;
	case MACREG_INT_CODE_SNR_LOW:
		lbs_pr_alert("EVENT: snr low\n");
		break;
	case MACREG_INT_CODE_MAX_FAIL:
		lbs_pr_alert("EVENT: max fail\n");
		break;
	case MACREG_INT_CODE_RSSI_HIGH:
		lbs_pr_alert("EVENT: rssi high\n");
		break;
	case MACREG_INT_CODE_SNR_HIGH:
		lbs_pr_alert("EVENT: snr high\n");
		break;

	case MACREG_INT_CODE_MESH_AUTO_STARTED:
		/* Ignore spurious autostart events */
		lbs_pr_info("EVENT: MESH_AUTO_STARTED (ignoring)\n");
		break;

	default:
		lbs_pr_alert("EVENT: unknown event id %d\n", event);
		break;
	}

	lbs_deb_leave_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}
