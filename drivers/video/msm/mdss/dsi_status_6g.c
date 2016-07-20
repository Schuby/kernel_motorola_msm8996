/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>

#include "mdss_dsi.h"
#include "mdss_mdp.h"

/*
 * mdss_check_te_status() - Check the status of panel for TE based ESD.
 * @ctrl_pdata   : dsi controller data
 * @pstatus_data : dsi status data
 * @interval     : duration in milliseconds for panel TE wait
 *
 * This function is called when the TE signal from the panel doesn't arrive
 * after 'interval' milliseconds. If the TE IRQ is not ready, the workqueue
 * gets re-scheduled. Otherwise, report the panel to be dead due to ESD attack.
 */
static bool mdss_check_te_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		struct dsi_status_data *pstatus_data, uint32_t interval)
{
	bool ret;

	atomic_set(&ctrl_pdata->te_irq_ready, 0);
	reinit_completion(&ctrl_pdata->te_irq_comp);
	enable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	/* Define TE interrupt timeout value as 3x(1/fps) */
	ret = wait_for_completion_timeout(&ctrl_pdata->te_irq_comp,
			msecs_to_jiffies(interval));
	disable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	pr_debug("%s: Panel TE check done with ret = %d\n", __func__, ret);
	return ret;
}

/*
 * mdss_check_dsi_ctrl_status() - Check MDP5 DSI controller status periodically.
 * @work     : dsi controller status data
 * @interval : duration in milliseconds to schedule work queue
 *
 * This function calls check_status API on DSI controller to send the BTA
 * command. If DSI controller fails to acknowledge the BTA command, it sends
 * the PANEL_ALIVE=0 status to HAL layer.
 */
void mdss_check_dsi_ctrl_status(struct work_struct *work, uint32_t interval)
{
	struct dsi_status_data *pstatus_data = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mipi_panel_info *mipi = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_overlay_private *mdp5_data = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret = 0;

	pstatus_data = container_of(to_delayed_work(work),
		struct dsi_status_data, check_status);
	if (!pstatus_data || !(pstatus_data->mfd)) {
		pr_err("%s: mfd not available\n", __func__);
		return;
	}

	pdata = dev_get_platdata(&pstatus_data->mfd->pdev->dev);
	if (!pdata) {
		pr_err("%s: Panel data not available\n", __func__);
		return;
	}
	mipi = &pdata->panel_info.mipi;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	if (!ctrl_pdata || (!ctrl_pdata->check_status &&
		(ctrl_pdata->status_mode != ESD_TE))) {
		pr_err("%s: DSI ctrl or status_check callback not available\n",
								__func__);
		return;
	}

	if (!pdata->panel_info.esd_rdy) {
		pr_debug("%s: unblank not complete, reschedule check status\n",
			__func__);
		schedule_delayed_work(&pstatus_data->check_status,
				msecs_to_jiffies(interval));
		return;
	}

	mdp5_data = mfd_to_mdp5_data(pstatus_data->mfd);
	ctl = mfd_to_ctl(pstatus_data->mfd);

	if (!ctl) {
		pr_err("%s: Display is off\n", __func__);
		return;
	}

	if (ctrl_pdata->status_mode == ESD_TE) {
		uint32_t fps = mdss_panel_get_framerate(&pdata->panel_info,
							FPS_RESOLUTION_HZ);
		uint32_t timeout = ((1000 / fps) + 1) *
					MDSS_STATUS_TE_WAIT_MAX;

		if (mdss_check_te_status(ctrl_pdata, pstatus_data, timeout))
			goto sim;
		else
			goto status_dead;
	}

	/*
	 * TODO: Because mdss_dsi_cmd_mdp_busy has made sure DMA to
	 * be idle in mdss_dsi_cmdlist_commit, it is not necessary
	 * to acquire ov_lock in case of video mode. Removing this
	 * lock to fix issues so that ESD thread would not block other
	 * overlay operations. Need refine this lock for command mode
	 *
	 * If Burst mode is enabled then we dont have to acquire ov_lock as
	 * command and data arbitration is possible in h/w
	 */

	if ((mipi->mode == DSI_CMD_MODE) && !ctrl_pdata->burst_mode_enabled)
		mutex_lock(&mdp5_data->ov_lock);
	mutex_lock(&ctl->mfd->param_lock);

	if (mdss_panel_is_power_off(pstatus_data->mfd->panel_power_state) ||
			pstatus_data->mfd->shutdown_pending) {
		mutex_unlock(&ctl->mfd->param_lock);
		if ((mipi->mode == DSI_CMD_MODE) &&
		    !ctrl_pdata->burst_mode_enabled)
			mutex_unlock(&mdp5_data->ov_lock);
		pr_err("%s: DSI turning off, avoiding panel status check\n",
							__func__);
		return;
	}

	/*
	 * For the command mode panels, we return pan display
	 * IOCTL on vsync interrupt. So, after vsync interrupt comes
	 * and when DMA_P is in progress, if the panel stops responding
	 * and if we trigger BTA before DMA_P finishes, then the DSI
	 * FIFO will not be cleared since the DSI data bus control
	 * doesn't come back to the host after BTA. This may cause the
	 * display reset not to be proper. Hence, wait for DMA_P done
	 * for command mode panels before triggering BTA.
	 */
	if (ctl->ops.wait_pingpong && !ctrl_pdata->burst_mode_enabled)
		ctl->ops.wait_pingpong(ctl, NULL);

	pr_debug("%s: DSI ctrl wait for ping pong done\n", __func__);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	ret = ctrl_pdata->check_status(ctrl_pdata);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&ctl->mfd->param_lock);
	if ((mipi->mode == DSI_CMD_MODE) && !ctrl_pdata->burst_mode_enabled)
		mutex_unlock(&mdp5_data->ov_lock);

	if ((pstatus_data->mfd->panel_power_state == MDSS_PANEL_POWER_ON)) {
		if (ret > 0)
			schedule_delayed_work(&pstatus_data->check_status,
				msecs_to_jiffies(interval));
		else
			goto status_dead;
	}
sim:
	if (pdata->panel_info.panel_force_dead) {
		pr_debug("force_dead=%d\n", pdata->panel_info.panel_force_dead);
		pdata->panel_info.panel_force_dead--;
		if (!pdata->panel_info.panel_force_dead)
			goto status_dead;
	}

	return;

status_dead:
	mdss_fb_report_panel_dead(pstatus_data->mfd);
}
