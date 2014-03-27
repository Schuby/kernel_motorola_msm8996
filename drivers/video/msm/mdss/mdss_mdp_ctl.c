/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sort.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"

static void mdss_mdp_xlog_mixer_reg(struct mdss_mdp_ctl *ctl);
static inline u64 fudge_factor(u64 val, u32 numer, u32 denom)
{
	u64 result = (val * (u64)numer);
	do_div(result, denom);
	return result;
}

static inline u64 apply_fudge_factor(u64 val,
	struct mdss_fudge_factor *factor)
{
	return fudge_factor(val, factor->numer, factor->denom);
}

static DEFINE_MUTEX(mdss_mdp_ctl_lock);

static int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer);
static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer);

static inline u32 mdss_mdp_get_pclk_rate(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	return (ctl->intf_type == MDSS_INTF_DSI) ?
		pinfo->mipi.dsi_pclk_rate :
		pinfo->clk_rate;
}

static inline u32 mdss_mdp_clk_fudge_factor(struct mdss_mdp_mixer *mixer,
						u32 rate)
{
	struct mdss_panel_info *pinfo = &mixer->ctl->panel_data->panel_info;

	rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	/*
	 * If the panel is video mode and its back porch period is
	 * small, the workaround of increasing mdp clk is needed to
	 * avoid underrun.
	 */
	if (mixer->ctl->is_video_mode && pinfo &&
		(pinfo->lcdc.v_back_porch < MDP_MIN_VBP))
		rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	return rate;
}

struct mdss_mdp_prefill_params {
	u32 smp_bytes;
	u32 xres;
	u32 src_w;
	u32 dst_w;
	u32 src_h;
	u32 dst_h;
	u32 dst_y;
	u32 bpp;
	bool is_yuv;
	bool is_caf;
	bool is_fbc;
	bool is_bwc;
	bool is_tile;
	bool is_hflip;
	bool is_cmd;
};

static inline bool mdss_mdp_perf_is_caf(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/*
	 * CAF mode filter is enabled when format is yuv and
	 * upscaling. Post processing had the decision to use CAF
	 * under these conditions.
	 */
	return ((mdata->mdp_rev >= MDSS_MDP_HW_REV_102) &&
		pipe->src_fmt->is_yuv && ((pipe->src.h >> pipe->vert_deci) <=
			pipe->dst.h));
}

static inline u32 mdss_mdp_calc_y_scaler_bytes(struct mdss_mdp_prefill_params
	*params, struct mdss_prefill_data *prefill)
{
	u32 y_scaler_bytes = 0, y_scaler_lines = 0;

	if (params->is_yuv) {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = (params->is_caf) ?
				prefill->y_scaler_lines_caf :
				prefill->y_scaler_lines_bilinear;
			/*
			 * y is src_width, u is src_width/2 and v is
			 * src_width/2, so the total is scaler_lines *
			 * src_w * 2
			 */
			y_scaler_bytes = y_scaler_lines * params->src_w * 2;
		}
	} else {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = prefill->y_scaler_lines_bilinear;
			y_scaler_bytes = y_scaler_lines * params->src_w *
				params->bpp;
		}
	}

	return y_scaler_bytes;
}

static inline u32 mdss_mdp_calc_latency_buf_bytes(struct mdss_mdp_prefill_params
	*params, struct mdss_prefill_data *prefill)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 latency_lines, latency_buf_bytes;

	if (params->is_yuv) {
		if (params->is_bwc) {
			latency_lines = 4;
			latency_buf_bytes = params->src_w * params->bpp *
				latency_lines;
		} else {
			latency_lines = 2;
			latency_buf_bytes = ALIGN(params->src_w * params->bpp *
				latency_lines, mdata->smp_mb_size) * 2;
		}
	} else {
		if (params->is_tile) {
			latency_lines = 8;
			latency_buf_bytes = params->src_w * params->bpp *
				latency_lines;
		} else if (params->is_bwc) {
			latency_lines = 4;
			latency_buf_bytes = params->src_w * params->bpp *
				latency_lines;
		} else {
			latency_lines = 2;
			latency_buf_bytes = ALIGN(params->src_w * params->bpp *
				latency_lines, mdata->smp_mb_size);
		}
	}

	return latency_buf_bytes;
}

static inline u32 mdss_mdp_calc_scaling_w_h(u32 val, u32 src_h, u32 dst_h,
	u32 src_w, u32 dst_w)
{
	if (dst_h)
		val = mult_frac(val, src_h, dst_h);
	if (dst_w)
		val = mult_frac(val, src_w, dst_w);

	return val;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_video(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 latency_buf_bytes;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes;
	u32 pp_bytes = 0, pp_lines = 0;
	u32 post_scaler_bytes;
	u32 fbc_bytes = 0;

	prefill_bytes = prefill->ot_bytes;

	latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(params, prefill);
	prefill_bytes += latency_buf_bytes;
	pr_debug("latency_buf_bytes bw_calc=%d actual=%d\n", latency_buf_bytes,
		params->smp_bytes);

	if (params->is_yuv)
		y_buf_bytes = prefill->y_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);

	prefill_bytes += y_buf_bytes + y_scaler_bytes;

	post_scaler_bytes = prefill->post_scaler_pixels * params->bpp;
	post_scaler_bytes = mdss_mdp_calc_scaling_w_h(post_scaler_bytes,
		params->src_h, params->dst_h, params->src_w, params->dst_w);
	prefill_bytes += post_scaler_bytes;

	if (params->xres)
		pp_lines = DIV_ROUND_UP(prefill->pp_pixels, params->xres);
	if (params->xres && params->dst_h && (params->dst_y <= pp_lines))
		pp_bytes = ((params->src_w * params->bpp * prefill->pp_pixels /
				params->xres) * params->src_h) / params->dst_h;
	prefill_bytes += pp_bytes;

	if (params->is_fbc) {
		fbc_bytes = prefill->fbc_lines * params->bpp;
		fbc_bytes = mdss_mdp_calc_scaling_w_h(fbc_bytes, params->src_h,
			params->dst_h, params->src_w, params->dst_w);
	}
	prefill_bytes += fbc_bytes;

	pr_debug("ot=%d y_buf=%d pp_lines=%d pp=%d post_sc=%d fbc_bytes=%d\n",
		prefill->ot_bytes, y_buf_bytes, pp_lines, pp_bytes,
		post_scaler_bytes, fbc_bytes);

	return prefill_bytes;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_cmd(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 ot_bytes = 0;
	u32 latency_lines, latency_buf_bytes;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;
	u32 post_scaler_bytes = 0;

	/* y_scaler_bytes are same for the first or non first line */
	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes = y_scaler_bytes;

	/* 1st line if fbc is not enabled and 2nd line if fbc is enabled */
	if (((params->dst_y == 0) && !params->is_fbc) ||
		((params->dst_y <= 1) && params->is_fbc)) {
		if (params->is_bwc || params->is_tile)
			latency_lines = 4;
		else if (!params->is_caf && params->is_hflip)
			latency_lines = 1;
		else
			latency_lines = 0;
		latency_buf_bytes = params->src_w * params->bpp * latency_lines;
		prefill_bytes += latency_buf_bytes;

		fbc_cmd_lines++;
		if (params->is_fbc)
			fbc_cmd_lines++;
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	} else {
		ot_bytes = prefill->ot_bytes;
		prefill_bytes += ot_bytes;

		latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(params,
			prefill);
		prefill_bytes += latency_buf_bytes;

		if (params->is_yuv)
			y_buf_bytes = prefill->y_buf_bytes;
		prefill_bytes += y_buf_bytes;

		post_scaler_bytes = prefill->post_scaler_pixels * params->bpp;
		post_scaler_bytes = mdss_mdp_calc_scaling_w_h(post_scaler_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += post_scaler_bytes;
	}

	pr_debug("ot=%d bwc=%d smp=%d y_buf=%d fbc=%d\n", ot_bytes,
		params->is_bwc, latency_buf_bytes, y_buf_bytes, fbc_cmd_bytes);

	return prefill_bytes;
}

u32 mdss_mdp_perf_calc_pipe_prefill_single(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 latency_lines, latency_buf_bytes;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;

	if (params->is_bwc || params->is_tile)
		/* can start processing after receiving 4 lines */
		latency_lines = 4;
	else if (!params->is_caf && params->is_hflip)
		/* need oneline before reading backwards */
		latency_lines = 1;
	else
		latency_lines = 0;
	latency_buf_bytes = params->src_w * params->bpp * latency_lines;
	prefill_bytes = latency_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes += y_scaler_bytes;

	if (params->is_cmd)
		fbc_cmd_lines++;
	if (params->is_fbc)
		fbc_cmd_lines++;

	if (fbc_cmd_lines) {
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	}

	return prefill_bytes;
}

static inline bool mdss_mdp_is_single_pipe_per_mixer(
	struct mdss_mdp_mixer *mixer)
{
	bool is_single = true;
	int cnt = 0;
	int i;

	for (i = 0; i < MDSS_MDP_MAX_STAGE; i++) {
		struct mdss_mdp_pipe *pipe = mixer->stage_pipe[i];
		if (pipe) {
			cnt++;
			if (cnt > 1) {
				is_single = false;
				break;
			}
		}
	}
	return is_single;
}

/**
 * mdss_mdp_perf_calc_pipe() - calculate performance numbers required by pipe
 * @pipe:	Source pipe struct containing updated pipe params
 * @perf:	Structure containing values that should be updated for
 *		performance tuning
 * @apply_fudge:	Boolean to determine if mdp clock fudge is applicable
 *
 * Function calculates the minimum required performance calculations in order
 * to avoid MDP underflow. The calculations are based on the way MDP
 * fetches (bandwidth requirement) and processes data through MDP pipeline
 * (MDP clock requirement) based on frame size and scaling requirements.
 */
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_perf_params *perf, struct mdss_rect *roi,
	bool apply_fudge)
{
	struct mdss_mdp_mixer *mixer;
	int fps = DEFAULT_FRAME_RATE;
	u32 quota, rate, v_total, src_h, xres = 0, h_total = 0;
	struct mdss_rect src, dst;
	bool is_fbc = false;
	struct mdss_mdp_prefill_params prefill_params;

	if (!pipe || !perf || !pipe->mixer_left)
		return -EINVAL;

	mixer = pipe->mixer_left;

	dst = pipe->dst;
	src = pipe->src;

	if (mixer->rotator_mode) {
		v_total = pipe->flags & MDP_ROT_90 ? pipe->dst.w : pipe->dst.h;
	} else if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		struct mdss_panel_info *pinfo;

		pinfo = &mixer->ctl->panel_data->panel_info;
		fps = mdss_panel_get_framerate(pinfo);
		v_total = mdss_panel_get_vtotal(pinfo);
		xres = pinfo->xres;
		is_fbc = pinfo->fbc.enabled;
		h_total = mdss_panel_get_htotal(pinfo, false);
	} else {
		v_total = mixer->height;
		xres = mixer->width;
		h_total = mixer->width;
	}

	if (roi && !mixer->ctl->is_video_mode && !pipe->src_split_req)
		mdss_mdp_crop_rect(&src, &dst, roi);

	pr_debug("v_total=%d, xres=%d fps=%d\n", v_total, xres, fps);

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	src_h = src.h >> pipe->vert_deci;

	quota = fps * src.w * src_h;

	pr_debug("src(w,h)(%d,%d) dst(w,h)(%d,%d) dst_y=%d bpp=%d yuv=%d\n",
		 pipe->src.w, src_h, pipe->dst.w, pipe->dst.h, pipe->dst.y,
		 pipe->src_fmt->bpp, pipe->src_fmt->is_yuv);

	if (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		/*
		 * with decimation, chroma is not downsampled, this means we
		 * need to allocate bw for extra lines that will be fetched
		 */
		if (pipe->vert_deci)
			quota *= 2;
		else
			quota = (quota * 3) / 2;
	else
		quota *= pipe->src_fmt->bpp;

	rate = dst.w;
	if (src_h > dst.h)
		rate = (rate * src_h) / dst.h;

	rate *= v_total * fps;
	if (mixer->rotator_mode) {
		rate /= 4; /* block mode fetch at 4 pix/clk */
		quota *= 2; /* bus read + write */
	} else {
		quota = mult_frac(quota, v_total, dst.h);
		if (!mixer->ctl->is_video_mode)
			quota = mult_frac(quota, h_total, xres);
	}
	perf->bw_overlap = quota;

	if (apply_fudge)
		perf->mdp_clk_rate = mdss_mdp_clk_fudge_factor(mixer, rate);
	else
		perf->mdp_clk_rate = rate;

	if (mixer->ctl->intf_num == MDSS_MDP_NO_INTF) {
		perf->prefill_bytes = 0;
		return 0;
	}

	prefill_params.smp_bytes = mdss_mdp_smp_get_size(pipe);
	prefill_params.xres = xres;
	prefill_params.src_w = src.w;
	prefill_params.src_h = src_h;
	prefill_params.dst_w = dst.w;
	prefill_params.dst_h = dst.h;
	prefill_params.dst_y = dst.y;
	prefill_params.bpp = pipe->src_fmt->bpp;
	prefill_params.is_yuv = pipe->src_fmt->is_yuv;
	prefill_params.is_caf = mdss_mdp_perf_is_caf(pipe);
	prefill_params.is_fbc = is_fbc;
	prefill_params.is_bwc = pipe->bwc_mode;
	prefill_params.is_tile = pipe->src_fmt->tile;
	prefill_params.is_hflip = pipe->flags & MDP_FLIP_LR;
	prefill_params.is_cmd = !mixer->ctl->is_video_mode;

	if (mdss_mdp_is_single_pipe_per_mixer(mixer))
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_single(&prefill_params);
	else if (!prefill_params.is_cmd)
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_video(&prefill_params);
	else
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_cmd(&prefill_params);

	pr_debug("mixer=%d pnum=%d clk_rate=%u bw_overlap=%llu prefill=%d\n",
		 mixer->num, pipe->num, perf->mdp_clk_rate, perf->bw_overlap,
		 perf->prefill_bytes);

	return 0;
}

static inline int mdss_mdp_perf_is_overlap(u32 y00, u32 y01, u32 y10, u32 y11)
{
	return (y10 < y00 && y11 >= y01) || (y10 >= y00 && y10 < y01);
}

static inline int cmpu32(const void *a, const void *b)
{
	return (*(u32 *)a < *(u32 *)b) ? -1 : 0;
}

static void mdss_mdp_perf_calc_mixer(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **pipe_list, int num_pipes)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_panel_info *pinfo = NULL;
	int fps = DEFAULT_FRAME_RATE;
	u32 v_total = 0;
	int i;
	u32 max_clk_rate = 0;
	u64 bw_overlap_max = 0;
	u64 bw_overlap[MAX_PIPES_PER_LM] = { 0 };
	u32 v_region[MAX_PIPES_PER_LM * 2] = { 0 };
	u32 prefill_bytes = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool apply_fudge = true;

	BUG_ON(num_pipes > MAX_PIPES_PER_LM);

	memset(perf, 0, sizeof(*perf));

	if (!mixer->rotator_mode) {
		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
			pinfo = &mixer->ctl->panel_data->panel_info;
			fps = mdss_panel_get_framerate(pinfo);
			v_total = mdss_panel_get_vtotal(pinfo);

			if (pinfo->type == WRITEBACK_PANEL)
				pinfo = NULL;
		} else {
			v_total = mixer->height;
		}

		perf->mdp_clk_rate = mixer->width * v_total * fps;
		perf->mdp_clk_rate =
			mdss_mdp_clk_fudge_factor(mixer, perf->mdp_clk_rate);

		if (!pinfo)	/* perf for bus writeback */
			perf->bw_overlap =
				fps * mixer->width * mixer->height * 3;
	}

	memset(bw_overlap, 0, sizeof(u64) * MAX_PIPES_PER_LM);
	memset(v_region, 0, sizeof(u32) * MAX_PIPES_PER_LM * 2);

	/*
	* Apply this logic only for 8x26 to reduce clock rate
	* for single video playback use case
	*/
	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_101)
		 && mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		u32 npipes = 0;
		for (i = 0; i < num_pipes; i++) {
			pipe = pipe_list[i];
			if (pipe) {
				if (npipes) {
					apply_fudge = true;
					break;
				}
				npipes++;
				apply_fudge = !(pipe->src_fmt->is_yuv)
					|| !(pipe->flags
					& MDP_SOURCE_ROTATED_90);
			}
		}
	}

	for (i = 0; i < num_pipes; i++) {
		struct mdss_mdp_perf_params tmp;
		pipe = pipe_list[i];
		if (pipe == NULL)
			continue;

		/*
		 * if is pipe used across two LMs in source split configuration
		 * then it is staged on both LMs. In such cases skip BW calc
		 * for such pipe on right LM to prevent adding BW twice.
		 */
		if (pipe->src_split_req && mixer->is_right_mixer)
			continue;

		if (mdss_mdp_perf_calc_pipe(pipe, &tmp, &mixer->roi,
			apply_fudge))
			continue;

		prefill_bytes += tmp.prefill_bytes;
		bw_overlap[i] = tmp.bw_overlap;
		v_region[2*i] = pipe->dst.y;
		v_region[2*i + 1] = pipe->dst.y + pipe->dst.h;
		if (tmp.mdp_clk_rate > max_clk_rate)
			max_clk_rate = tmp.mdp_clk_rate;
	}

	/*
	 * Sort the v_region array so the total display area can be
	 * divided into individual regions. Check how many pipes fetch
	 * data for each region and sum them up, then the worst case
	 * of all regions is ib request.
	 */
	sort(v_region, num_pipes * 2, sizeof(u32), cmpu32, NULL);
	for (i = 1; i < num_pipes * 2; i++) {
		int j;
		u64 bw_max_region = 0;
		u32 y0, y1;
		pr_debug("v_region[%d]%d\n", i, v_region[i]);
		if (v_region[i] == v_region[i-1])
			continue;
		y0 = v_region[i-1];
		y1 = v_region[i];
		for (j = 0; j < num_pipes; j++) {
			if (!bw_overlap[j])
				continue;
			pipe = pipe_list[j];
			if (mdss_mdp_perf_is_overlap(y0, y1, pipe->dst.y,
				(pipe->dst.y + pipe->dst.h)))
				bw_max_region += bw_overlap[j];
			pr_debug("v[%d](%d,%d)pipe[%d](%d,%d)bw(%llu %llu)\n",
				i, y0, y1, j, pipe->dst.y,
				pipe->dst.y + pipe->dst.h, bw_overlap[j],
				bw_max_region);
		}
		bw_overlap_max = max(bw_overlap_max, bw_max_region);
	}

	perf->bw_overlap += bw_overlap_max;
	perf->prefill_bytes += prefill_bytes;

	if (max_clk_rate > perf->mdp_clk_rate)
		perf->mdp_clk_rate = max_clk_rate;

	pr_debug("final mixer=%d video=%d clk_rate=%u bw=%llu prefill=%d\n",
		mixer->num, mixer->ctl->is_video_mode, perf->mdp_clk_rate,
		perf->bw_overlap, perf->prefill_bytes);

}

static u32 mdss_mdp_get_vbp_factor(struct mdss_mdp_ctl *ctl)
{
	u32 fps, v_total, vbp, vbp_fac;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return 0;

	pinfo = &ctl->panel_data->panel_info;
	fps = mdss_panel_get_framerate(pinfo);
	v_total = mdss_panel_get_vtotal(pinfo);
	vbp = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	vbp_fac = (vbp) ? fps * v_total / vbp : 0;
	pr_debug("vbp_fac=%d vbp=%d v_total=%d\n", vbp_fac, vbp, v_total);

	return vbp_fac;
}

static u32 mdss_mdp_get_vbp_factor_max(struct mdss_mdp_ctl *ctl)
{
	u32 vbp_max = 0;
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;
		u32 vbp_fac;

		if (ctl->power_on) {
			vbp_fac = mdss_mdp_get_vbp_factor(ctl);
			vbp_max = max(vbp_max, vbp_fac);
		}
	}

	return vbp_max;
}

static void __mdss_mdp_perf_calc_ctl_helper(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt)
{
	struct mdss_mdp_perf_params tmp;

	memset(perf, 0, sizeof(*perf));

	if (left_cnt && ctl->mixer_left) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_left, &tmp,
				left_plist, left_cnt);
		perf->bw_overlap += tmp.bw_overlap;
		perf->prefill_bytes += tmp.prefill_bytes;
		perf->mdp_clk_rate = tmp.mdp_clk_rate;
	}

	if (right_cnt && ctl->mixer_right) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_right, &tmp,
				right_plist, right_cnt);
		perf->bw_overlap += tmp.bw_overlap;
		perf->prefill_bytes += tmp.prefill_bytes;
		if (tmp.mdp_clk_rate > perf->mdp_clk_rate)
			perf->mdp_clk_rate = tmp.mdp_clk_rate;

		if (ctl->intf_type) {
			u32 clk_rate = mdss_mdp_get_pclk_rate(ctl);
			/* minimum clock rate due to inefficiency in 3dmux */
			clk_rate = mult_frac(clk_rate >> 1, 9, 8);
			if (clk_rate > perf->mdp_clk_rate)
				perf->mdp_clk_rate = clk_rate;
		}
	}

	/* request minimum bandwidth to have bus clock on when display is on */
	if (perf->bw_overlap == 0)
		perf->bw_overlap = SZ_16M;

	if (ctl->intf_type != MDSS_MDP_NO_INTF) {
		u32 vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);

		perf->bw_prefill = perf->prefill_bytes;
		/*
		 * Prefill bandwidth equals the amount of data (number
		 * of prefill_bytes) divided by the the amount time
		 * available (blanking period). It is equivalent that
		 * prefill bytes times a factor in unit Hz, which is
		 * the reciprocal of time.
		 */
		perf->bw_prefill *= vbp_fac;
	}

	perf->bw_ctl = max(perf->bw_prefill, perf->bw_overlap);
}

int mdss_mdp_perf_bw_check(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_perf_params perf;
	u32 bw, threshold;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	__mdss_mdp_perf_calc_ctl_helper(ctl, &perf,
			left_plist, left_cnt, right_plist, right_cnt);

	/* convert bandwidth to kb */
	bw = DIV_ROUND_UP_ULL(perf.bw_ctl, 1000);
	pr_debug("calculated bandwidth=%uk\n", bw);

	threshold = ctl->is_video_mode ? mdata->max_bw_low : mdata->max_bw_high;
	if (bw > threshold) {
		pr_debug("exceeds bandwidth: %ukb > %ukb\n", bw, threshold);
		return -E2BIG;
	}

	return 0;
}

int mdss_mdp_perf_bw_check_pipe(struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	u32 vbp_fac, threshold;
	u64 prefill_bw, pipe_bw;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);
	prefill_bw = perf->prefill_bytes * vbp_fac;
	pipe_bw = max(prefill_bw, perf->bw_overlap);
	pr_debug("prefill=%llu, vbp_fac=%u, overlap=%llu\n",
			prefill_bw, vbp_fac, perf->bw_overlap);

	/* convert bandwidth to kb */
	pipe_bw = DIV_ROUND_UP_ULL(pipe_bw, 1000);

	threshold = mdata->max_bw_per_pipe;
	pr_debug("bw=%llu threshold=%u\n", pipe_bw, threshold);

	if (threshold && pipe_bw > threshold) {
		pr_debug("pipe exceeds bandwidth: %llukb > %ukb\n", pipe_bw,
				threshold);
		return -E2BIG;
	}

	return 0;
}

static void mdss_mdp_perf_calc_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf)
{
	struct mdss_mdp_pipe **left_plist, **right_plist;

	left_plist = ctl->mixer_left ? ctl->mixer_left->stage_pipe : NULL;
	right_plist = ctl->mixer_right ? ctl->mixer_right->stage_pipe : NULL;

	__mdss_mdp_perf_calc_ctl_helper(ctl, perf,
			left_plist, (left_plist ? MAX_PIPES_PER_LM : 0),
			right_plist, (right_plist ? MAX_PIPES_PER_LM : 0));

	if (ctl->is_video_mode) {
		if (perf->bw_overlap > perf->bw_prefill)
			perf->bw_ctl = apply_fudge_factor(perf->bw_ctl,
				&mdss_res->ib_factor_overlap);
		else
			perf->bw_ctl = apply_fudge_factor(perf->bw_ctl,
				&mdss_res->ib_factor);
	}
	pr_debug("ctl=%d clk_rate=%u\n", ctl->num, perf->mdp_clk_rate);
	pr_debug("bw_overlap=%llu bw_prefill=%llu prefill_bytes=%d\n",
		 perf->bw_overlap, perf->bw_prefill, perf->prefill_bytes);
}

static void set_status(u32 *value, bool status, u32 bit_num)
{
	if (status)
		*value |= BIT(bit_num);
	else
		*value &= ~BIT(bit_num);
}

/**
 * @ mdss_mdp_ctl_perf_set_transaction_status() -
 *                             Set the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * This function is called to set the status bit in the perf_transaction_status
 * according to the operation that it is on-going for the command mode
 * panels, where:
 *
 * PERF_SW_COMMIT_STATE:
 *           1 - If SW operation has been commited and bw
 *               has been requested (HW transaction have not started yet).
 *           0 - If there is no SW operation pending
 * PERF_HW_MDP_STATE:
 *           1 - If HW transaction is on-going
 *           0 - If there is no HW transaction on going (ping-pong interrupt
 *               has finished)
 * Only if both states are zero there are no pending operations and
 * BW could be released.
 * State can be queried calling "mdss_mdp_ctl_perf_get_transaction_status"
 */
void mdss_mdp_ctl_perf_set_transaction_status(struct mdss_mdp_ctl *ctl,
	enum mdss_mdp_perf_state_type component, bool new_status)
{
	u32  previous_transaction;
	bool previous_status;
	unsigned long flags;

	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	spin_lock_irqsave(&ctl->spin_lock, flags);

	previous_transaction = ctl->perf_transaction_status;
	previous_status = previous_transaction & BIT(component) ?
		PERF_STATUS_BUSY : PERF_STATUS_DONE;

	/*
	 * If we set "done" state when previous state was not "busy",
	 * we want to print a warning since maybe there is a state
	 * that we are not considering
	 */
	WARN((PERF_STATUS_DONE == new_status) &&
		(PERF_STATUS_BUSY != previous_status),
		"unexpected previous state for component: %d\n", component);

	set_status(&ctl->perf_transaction_status, new_status,
		(u32)component);

	pr_debug("component:%d previous_transaction:%d transaction_status:%d\n",
		component, previous_transaction, ctl->perf_transaction_status);
	pr_debug("new_status:%d prev_status:%d\n",
		new_status, previous_status);

	spin_unlock_irqrestore(&ctl->spin_lock, flags);
}

/**
 * @ mdss_mdp_ctl_perf_get_transaction_status() -
 *                             Get the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * Return:
 * The status of the transactions for the command mode panels,
 * note that the bandwidth can be released only if all transaction
 * status bits are zero.
 */
u32 mdss_mdp_ctl_perf_get_transaction_status(struct mdss_mdp_ctl *ctl)
{
	unsigned long flags;
	u32 transaction_status;

	/*
	 * If Video Mode or not valid data to determine the status, return busy
	 * status, so the bandwidth cannot be freed by the caller
	 */
	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL)) {
		return PERF_STATUS_BUSY;
	}

	spin_lock_irqsave(&ctl->spin_lock, flags);
	transaction_status = ctl->perf_transaction_status;
	spin_unlock_irqrestore(&ctl->spin_lock, flags);

	return transaction_status;
}

static inline void mdss_mdp_ctl_perf_update_bus(struct mdss_mdp_ctl *ctl)
{
	u64 bw_sum_of_intfs = 0;
	u64 bus_ab_quota, bus_ib_quota;
	struct mdss_data_type *mdata;
	int i;

	if (!ctl || !ctl->mdata)
		return;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl;
		ctl = mdata->ctl_off + i;
		if (ctl->power_on) {
			bw_sum_of_intfs += ctl->cur_perf.bw_ctl;
			pr_debug("c=%d bw=%llu\n", ctl->num,
				ctl->cur_perf.bw_ctl);
		}
	}
	bus_ib_quota = max(bw_sum_of_intfs, mdata->perf_tune.min_bus_vote);
	bus_ab_quota = apply_fudge_factor(bus_ib_quota,
		&mdss_res->ab_factor);
	mdss_mdp_bus_scale_set_quota(bus_ab_quota, bus_ib_quota);
	pr_debug("ab=%llu ib=%llu\n", bus_ab_quota, bus_ib_quota);
}

/**
 * @mdss_mdp_ctl_perf_release_bw() - request zero bandwidth
 * @ctl - pointer to a ctl
 *
 * Function checks a state variable for the ctl, if all pending commit
 * requests are done, meaning no more bandwidth is needed, release
 * bandwidth request.
 */
void mdss_mdp_ctl_perf_release_bw(struct mdss_mdp_ctl *ctl)
{
	int transaction_status;
	struct mdss_data_type *mdata;
	int i;

	/* only do this for command panel */
	if (!ctl || !ctl->mdata || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = ctl->mdata;
	/*
	 * If video interface present, cmd panel bandwidth cannot be
	 * released.
	 */
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;

		if (ctl->power_on && ctl->is_video_mode)
			goto exit;
	}

	transaction_status = mdss_mdp_ctl_perf_get_transaction_status(ctl);
	pr_debug("transaction_status=0x%x\n", transaction_status);

	/*Release the bandwidth only if there are no transactions pending*/
	if (!transaction_status) {
		ctl->cur_perf.bw_ctl = 0;
		ctl->new_perf.bw_ctl = 0;
		pr_debug("Release BW ctl=%d\n", ctl->num);
		mdss_mdp_ctl_perf_update_bus(ctl);
	}
exit:
	mutex_unlock(&mdss_mdp_ctl_lock);
}

static int mdss_mdp_select_clk_lvl(struct mdss_mdp_ctl *ctl,
			u32 clk_rate)
{
	int i;
	struct mdss_data_type *mdata;

	if (!ctl)
		return -ENODEV;

	mdata = ctl->mdata;

	for (i = 0; i < mdata->nclk_lvl; i++) {
		if (clk_rate > mdata->clock_levels[i]) {
			continue;
		} else {
			clk_rate = mdata->clock_levels[i];
			break;
		}
	}

	return clk_rate;
}

static void mdss_mdp_ctl_perf_update(struct mdss_mdp_ctl *ctl,
		int params_changed)
{
	struct mdss_mdp_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	struct mdss_data_type *mdata;
	bool is_bw_released;

	if (!ctl || !ctl->mdata)
		return;

	mutex_lock(&mdss_mdp_ctl_lock);

	mdata = ctl->mdata;
	old = &ctl->cur_perf;
	new = &ctl->new_perf;

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);

	if (ctl->power_on) {
		if (is_bw_released || params_changed)
			mdss_mdp_perf_calc_ctl(ctl, new);
		/*
		 * if params have just changed delay the update until
		 * later once the hw configuration has been flushed to
		 * MDP
		 */
		if ((params_changed && (new->bw_ctl > old->bw_ctl)) ||
		    (!params_changed && (new->bw_ctl < old->bw_ctl))) {
			pr_debug("c=%d p=%d new_bw=%llu,old_bw=%llu\n",
				ctl->num, params_changed, new->bw_ctl,
				old->bw_ctl);
			old->bw_ctl = new->bw_ctl;
			update_bus = 1;
		}

		if ((params_changed && (new->mdp_clk_rate > old->mdp_clk_rate))
		    || (!params_changed && (new->mdp_clk_rate <
					    old->mdp_clk_rate))) {
			old->mdp_clk_rate = new->mdp_clk_rate;
			update_clk = 1;
		}
	} else {
		memset(old, 0, sizeof(*old));
		memset(new, 0, sizeof(*new));
		update_bus = 1;
		update_clk = 1;
	}

	if (update_bus)
		mdss_mdp_ctl_perf_update_bus(ctl);

	if (update_clk) {
		u32 clk_rate = 0;
		int i;

		for (i = 0; i < mdata->nctl; i++) {
			struct mdss_mdp_ctl *ctl;
			ctl = mdata->ctl_off + i;
			if (ctl->power_on)
				clk_rate = max(ctl->cur_perf.mdp_clk_rate,
					       clk_rate);
		}

		clk_rate  = mdss_mdp_select_clk_lvl(ctl, clk_rate);
		mdss_mdp_set_clk_rate(clk_rate);
		pr_debug("update clk rate = %d HZ\n", clk_rate);
	}

	mutex_unlock(&mdss_mdp_ctl_lock);
}

static struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(struct mdss_data_type *mdata,
					       u32 off)
{
	struct mdss_mdp_ctl *ctl = NULL;
	u32 cnum;
	u32 nctl = mdata->nctl;

	mutex_lock(&mdss_mdp_ctl_lock);
	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED)
		nctl++;

	for (cnum = off; cnum < nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (ctl->ref_cnt == 0) {
			ctl->ref_cnt++;
			ctl->mdata = mdata;
			mutex_init(&ctl->lock);
			spin_lock_init(&ctl->spin_lock);
			BLOCKING_INIT_NOTIFIER_HEAD(&ctl->notifier_head);
			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
		ctl = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return ctl;
}

static int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	pr_debug("free ctl_num=%d ref_cnt=%d\n", ctl->num, ctl->ref_cnt);

	if (!ctl->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	if (ctl->mixer_left) {
		mdss_mdp_mixer_free(ctl->mixer_left);
		ctl->mixer_left = NULL;
	}
	if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}
	mutex_lock(&mdss_mdp_ctl_lock);
	ctl->ref_cnt--;
	ctl->intf_num = MDSS_MDP_NO_INTF;
	ctl->intf_type = MDSS_MDP_NO_INTF;
	ctl->is_secure = false;
	ctl->power_on = false;
	ctl->start_fnc = NULL;
	ctl->stop_fnc = NULL;
	ctl->prepare_fnc = NULL;
	ctl->display_fnc = NULL;
	ctl->wait_fnc = NULL;
	ctl->read_line_cnt_fnc = NULL;
	ctl->add_vsync_handler = NULL;
	ctl->remove_vsync_handler = NULL;
	ctl->config_fps_fnc = NULL;
	ctl->panel_data = NULL;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

/**
 * mdss_mdp_mixer_alloc() - allocate mdp mixer.
 * @ctl: mdp controller.
 * @type: specifying type of mixer requested. interface or writeback.
 * @mux: specifies if mixer allocation is for split_fb cases.
 * @rotator: specifies if the mixer requested for rotator operations.
 *
 * This function is called to request allocation of mdp mixer
 * during mdp controller path setup.
 *
 * Return: mdp mixer structure that is allocated.
 *	   NULL if mixer allocation fails.
 */
static struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(
		struct mdss_mdp_ctl *ctl, u32 type, int mux, int rotator)
{
	struct mdss_mdp_mixer *mixer = NULL, *alt_mixer = NULL;
	u32 nmixers_intf;
	u32 nmixers_wb;
	u32 i;
	u32 nmixers;
	struct mdss_mdp_mixer *mixer_pool = NULL;

	if (!ctl || !ctl->mdata)
		return NULL;

	mutex_lock(&mdss_mdp_ctl_lock);
	nmixers_intf = ctl->mdata->nmixers_intf;
	nmixers_wb = ctl->mdata->nmixers_wb;

	switch (type) {
	case MDSS_MDP_MIXER_TYPE_INTF:
		mixer_pool = ctl->mdata->mixer_intf;
		nmixers = nmixers_intf;

		/*
		 * try to reserve first layer mixer for write back if
		 * assertive display needs to be supported through wfd
		 */
		if (ctl->mdata->has_wb_ad && ctl->intf_num &&
			((ctl->panel_data->panel_info.type != MIPI_CMD_PANEL) ||
			!mux)) {
			alt_mixer = mixer_pool;
			mixer_pool++;
			nmixers--;
		}
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mixer_pool = ctl->mdata->mixer_wb;
		nmixers = nmixers_wb;
		if ((ctl->mdata->wfd_mode == MDSS_MDP_WFD_DEDICATED) && rotator)
			mixer_pool = mixer_pool + nmixers;
		break;

	default:
		nmixers = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	/* early mdp revision only supports mux of dual pipe on mixers 0 and 1,
	 * need to ensure that these pipes are readily available by using
	 * mixer 2 if available and mux is not required */
	if (!mux && (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_100) &&
			(type == MDSS_MDP_MIXER_TYPE_INTF) &&
			(nmixers >= MDSS_MDP_INTF_LAYERMIXER2) &&
			(mixer_pool[MDSS_MDP_INTF_LAYERMIXER2].ref_cnt == 0))
		mixer_pool += MDSS_MDP_INTF_LAYERMIXER2;

	/*Allocate virtual wb mixer if no dedicated wfd wb blk is present*/
	if ((ctl->mdata->wfd_mode == MDSS_MDP_WFD_SHARED) &&
			(type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
		nmixers += 1;

	for (i = 0; i < nmixers; i++) {
		mixer = mixer_pool + i;
		if (mixer->ref_cnt == 0) {
			mixer->ref_cnt++;
			mixer->params_changed++;
			mixer->ctl = ctl;
			pr_debug("alloc mixer num %d for ctl=%d\n",
				 mixer->num, ctl->num);
			break;
		}
		mixer = NULL;
	}

	if (!mixer && alt_mixer && (alt_mixer->ref_cnt == 0))
		mixer = alt_mixer;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return mixer;
}

static int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer)
{
	if (!mixer)
		return -ENODEV;

	pr_debug("free mixer_num=%d ref_cnt=%d\n", mixer->num, mixer->ref_cnt);

	if (!mixer->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	mixer->ref_cnt--;
	mixer->is_right_mixer = false;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_mixer *mixer = NULL;
	u32 offset = mdss_res->nctl - mdss_res->nmixers_wb;

	ctl = mdss_mdp_ctl_alloc(mdss_res, offset);
	if (!ctl) {
		pr_debug("unable to allocate wb ctl\n");
		return NULL;
	}

	mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK,
							false, rotator);
	if (!mixer) {
		pr_debug("unable to allocate wb mixer\n");
		goto error;
	}

	mixer->rotator_mode = rotator;

	switch (mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT0_MODE :
			       MDSS_MDP_CTL_OP_WB0_MODE);
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT1_MODE :
			       MDSS_MDP_CTL_OP_WB1_MODE);
		break;
	default:
		pr_err("invalid layer mixer=%d\n", mixer->num);
		goto error;
	}

	ctl->mixer_left = mixer;

	ctl->start_fnc = mdss_mdp_writeback_start;
	ctl->power_on = true;
	ctl->wb_type = (rotator ? MDSS_MDP_WB_CTL_TYPE_BLOCK :
			MDSS_MDP_WB_CTL_TYPE_LINE);
	mixer->ctl = ctl;

	if (ctl->start_fnc)
		ctl->start_fnc(ctl);

	return mixer;
error:
	if (mixer)
		mdss_mdp_mixer_free(mixer);
	if (ctl)
		mdss_mdp_ctl_free(ctl);

	return NULL;
}

int mdss_mdp_wb_mixer_destroy(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_ctl *ctl;

	if (!mixer || !mixer->ctl) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	ctl = mixer->ctl;
	mixer->rotator_mode = 0;

	pr_debug("destroy ctl=%d mixer=%d\n", ctl->num, mixer->num);

	if (ctl->stop_fnc)
		ctl->stop_fnc(ctl);

	mdss_mdp_ctl_free(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0);

	return 0;
}

int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctl->panel_data->panel_info.cont_splash_enabled = 0;

	switch (ctl->panel_data->panel_info.type) {
	case MIPI_VIDEO_PANEL:
	case EDP_PANEL:
		return mdss_mdp_video_reconfigure_splash_done(ctl, handoff);
	case MIPI_CMD_PANEL:
		return mdss_mdp_cmd_reconfigure_splash_done(ctl, handoff);
	default:
		return 0;
	}
}

static inline int mdss_mdp_set_split_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_ctl *split_ctl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl || !split_ctl || !mdata)
		return -ENODEV;

	/* setup split ctl mixer as right mixer of original ctl so that
	 * original ctl can work the same way as dual pipe solution */
	ctl->mixer_right = split_ctl->mixer_left;

	if ((mdata->mdp_rev >= MDSS_MDP_HW_REV_103) && ctl->is_video_mode)
		ctl->split_flush_en = true;

	return 0;
}

static int mdss_mdp_ctl_fbc_enable(int enable,
		struct mdss_mdp_mixer *mixer, struct mdss_panel_info *pdata)
{
	struct fbc_panel_info *fbc;
	u32 mode = 0, budget_ctl = 0, lossy_mode = 0;

	if (!pdata) {
		pr_err("Invalid pdata\n");
		return -EINVAL;
	}

	fbc = &pdata->fbc;

	if (!fbc || !fbc->enabled) {
		pr_err("Invalid FBC structure\n");
		return -EINVAL;
	}

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0 ||
			mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		pr_debug("Mixer supports FBC.\n");
	} else {
		pr_debug("Mixer doesn't support FBC.\n");
		return -EINVAL;
	}

	if (enable) {
		mode = ((pdata->xres) << 16) | ((fbc->comp_mode) << 8) |
			((fbc->qerr_enable) << 7) | ((fbc->cd_bias) << 4) |
			((fbc->pat_enable) << 3) | ((fbc->vlc_enable) << 2) |
			((fbc->bflc_enable) << 1) | enable;

		budget_ctl = ((fbc->line_x_budget) << 12) |
			((fbc->block_x_budget) << 8) | fbc->block_budget;

		lossy_mode = ((fbc->lossless_mode_thd) << 16) |
			((fbc->lossy_mode_thd) << 8) |
			((fbc->lossy_rgb_thd) << 4) | fbc->lossy_mode_idx;
	}

	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_MODE, mode);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_BUDGET_CTL, budget_ctl);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_LOSSY_MODE, lossy_mode);

	return 0;
}

int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *split_ctl;
	u32 width, height;
	int split_fb;

	if (!ctl || !ctl->panel_data) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	split_ctl = mdss_mdp_get_split_ctl(ctl);

	width = ctl->panel_data->panel_info.xres;
	height = ctl->panel_data->panel_info.yres;

	split_fb = (ctl->mfd->split_fb_left &&
		    ctl->mfd->split_fb_right &&
		    (ctl->mfd->split_fb_left <= MAX_MIXER_WIDTH) &&
		    (ctl->mfd->split_fb_right <= MAX_MIXER_WIDTH)) ? 1 : 0;
	pr_debug("max=%d xres=%d left=%d right=%d\n", MAX_MIXER_WIDTH,
		 width, ctl->mfd->split_fb_left, ctl->mfd->split_fb_right);

	if ((split_ctl && (width > MAX_MIXER_WIDTH)) ||
			(width > (2 * MAX_MIXER_WIDTH))) {
		pr_err("Unsupported panel resolution: %dx%d\n", width, height);
		return -ENOTSUPP;
	}

	ctl->width = width;
	ctl->height = height;
	ctl->roi = (struct mdss_rect) {0, 0, width, height};

	if (!ctl->mixer_left) {
		ctl->mixer_left =
			mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			 ((width > MAX_MIXER_WIDTH) || split_fb), 0);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			return -ENOMEM;
		} else if (ctl->mixer_left->num >= 1 &&
			(ctl->panel_data->panel_info.type == MIPI_CMD_PANEL)) {
			pr_err("use only DSPP0 and DSPP1 with cmd split\n");
			return -EPERM;
		}
	}

	if (split_fb)
		width = ctl->mfd->split_fb_left;
	else if (width > MAX_MIXER_WIDTH)
		width /= 2;

	ctl->mixer_left->width = width;
	ctl->mixer_left->height = height;
	ctl->mixer_left->roi = (struct mdss_rect) {0, 0, width, height};

	if (split_ctl) {
		pr_debug("split display detected\n");
		return 0;
	}

	if (split_fb)
		width = ctl->mfd->split_fb_right;

	if (width < ctl->width) {
		if (ctl->mixer_right == NULL) {
			ctl->mixer_right = mdss_mdp_mixer_alloc(ctl,
					MDSS_MDP_MIXER_TYPE_INTF, true, 0);
			if (!ctl->mixer_right) {
				pr_err("unable to allocate right mixer\n");
				if (ctl->mixer_left)
					mdss_mdp_mixer_free(ctl->mixer_left);
				return -ENOMEM;
			}
		}
		ctl->mixer_right->is_right_mixer = true;
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
		ctl->mixer_right->roi = (struct mdss_rect)
						{0, 0, width, height};
	} else if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}

	if (ctl->mixer_right) {
		ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
			       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	} else {
		ctl->opmode &= ~(MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				  MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT);
	}

	return 0;
}

static int mdss_mdp_ctl_setup_wfd(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_mixer *mixer;
	int mixer_type;

	/* if WB2 is supported, try to allocate it first */
	if (mdata->wfd_mode == MDSS_MDP_WFD_INTERFACE)
		mixer_type = MDSS_MDP_MIXER_TYPE_INTF;
	else
		mixer_type = MDSS_MDP_MIXER_TYPE_WRITEBACK;

	mixer = mdss_mdp_mixer_alloc(ctl, mixer_type, false, 0);
	if (!mixer && mixer_type == MDSS_MDP_MIXER_TYPE_INTF)
		mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK,
				false, 0);

	if (!mixer) {
		pr_err("Unable to allocate writeback mixer\n");
		return -ENOMEM;
	}

	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF ||
			(mdata->wfd_mode == MDSS_MDP_WFD_DEDICATED)) {
		ctl->opmode = MDSS_MDP_CTL_OP_WFD_MODE;
	} else {
		switch (mixer->num) {
		case MDSS_MDP_WB_LAYERMIXER0:
			ctl->opmode = MDSS_MDP_CTL_OP_WB0_MODE;
			break;
		case MDSS_MDP_WB_LAYERMIXER1:
			ctl->opmode = MDSS_MDP_CTL_OP_WB1_MODE;
			break;
		default:
			pr_err("Incorrect writeback config num=%d\n",
					mixer->num);
			mdss_mdp_mixer_free(mixer);
			return -EINVAL;
		}
		ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_LINE;
	}
	ctl->mixer_left = mixer;

	return 0;
}

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
				       struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	ctl = mdss_mdp_ctl_alloc(mdata, MDSS_MDP_CTL0);
	if (!ctl) {
		pr_err("unable to allocate ctl\n");
		return ERR_PTR(-ENOMEM);
	}

	ctl->mfd = mfd;
	ctl->panel_data = pdata;
	ctl->is_video_mode = false;

	switch (pdata->panel_info.type) {
	case EDP_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF0;
		ctl->intf_type = MDSS_INTF_EDP;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->start_fnc = mdss_mdp_cmd_start;
		break;
	case DTV_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF3;
		ctl->intf_type = MDSS_INTF_HDMI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		ret = mdss_mdp_limited_lut_igc_config(ctl);
		if (ret)
			pr_err("Unable to config IGC LUT data");
		break;
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->start_fnc = mdss_mdp_writeback_start;
		ret = mdss_mdp_ctl_setup_wfd(ctl);
		if (ret)
			goto ctl_init_fail;
		break;
	default:
		pr_err("unsupported panel type (%d)\n", pdata->panel_info.type);
		ret = -EINVAL;
		goto ctl_init_fail;
	}

	ctl->opmode |= (ctl->intf_num << 4);

	if (ctl->intf_num == MDSS_MDP_NO_INTF) {
		ctl->dst_format = pdata->panel_info.out_format;
	} else {
		struct mdp_dither_cfg_data dither = {
			.block = mfd->index + MDP_LOGICAL_BLOCK_DISP_0,
			.flags = MDP_PP_OPS_DISABLE,
		};

		switch (pdata->panel_info.bpp) {
		case 18:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666;
			dither.flags = MDP_PP_OPS_ENABLE | MDP_PP_OPS_WRITE;
			dither.g_y_depth = 2;
			dither.r_cr_depth = 2;
			dither.b_cb_depth = 2;
			break;
		case 24:
		default:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB888;
			break;
		}
		mdss_mdp_dither_config(&dither, NULL);
	}

	return ctl;
ctl_init_fail:
	mdss_mdp_ctl_free(ctl);

	return ERR_PTR(ret);
}

int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_mixer *mixer;

	if (!ctl || !pdata)
		return -ENODEV;

	if (pdata->panel_info.xres > MAX_MIXER_WIDTH) {
		pr_err("Unsupported second panel resolution: %dx%d\n",
				pdata->panel_info.xres, pdata->panel_info.yres);
		return -ENOTSUPP;
	}

	if (ctl->mixer_right) {
		pr_err("right mixer already setup for ctl=%d\n", ctl->num);
		return -EPERM;
	}

	sctl = mdss_mdp_ctl_init(pdata, ctl->mfd);
	if (!sctl) {
		pr_err("unable to setup split display\n");
		return -ENODEV;
	}

	sctl->width = pdata->panel_info.xres;
	sctl->height = pdata->panel_info.yres;

	ctl->mixer_left = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			false, 0);
	if (!ctl->mixer_left) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	mixer = mdss_mdp_mixer_alloc(sctl, MDSS_MDP_MIXER_TYPE_INTF, false, 0);
	if (!mixer) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	mixer->is_right_mixer = true;
	mixer->width = sctl->width;
	mixer->height = sctl->height;
	mixer->roi = (struct mdss_rect)
				{0, 0, mixer->width, mixer->height};
	sctl->mixer_left = mixer;

	return mdss_mdp_set_split_ctl(ctl, sctl);
}

static void mdss_mdp_ctl_split_display_enable(int enable,
	struct mdss_mdp_ctl *main_ctl, struct mdss_mdp_ctl *slave_ctl)
{
	u32 upper = 0, lower = 0;

	pr_debug("split main ctl=%d intf=%d slave ctl=%d intf=%d\n",
			main_ctl->num, main_ctl->intf_num,
			slave_ctl->num, slave_ctl->intf_num);
	if (enable) {
		if (main_ctl->opmode & MDSS_MDP_CTL_OP_CMD_MODE) {
			upper |= BIT(1);
			lower |= BIT(1);

			/* interface controlling sw trigger */
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				upper |= BIT(4);
			else
				upper |= BIT(8);
		} else { /* video mode */
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				lower |= BIT(4);
			else
				lower |= BIT(8);
		}
	}
	writel_relaxed(upper, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_UPPER_PIPE_CTRL);
	writel_relaxed(lower, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_LOWER_PIPE_CTRL);
	writel_relaxed(enable, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_EN);

	if (main_ctl->split_flush_en)
		writel_relaxed(enable ? 0x1 : 0x0,
			main_ctl->mdata->mdp_base +
			MMSS_MDP_MDP_SSPP_SPARE_0);
}

int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int rc;

	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CLOSE, NULL);
	WARN(rc, "unable to close panel for intf=%d\n", ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl) {
		pr_debug("destroying split display ctl=%d\n", sctl->num);
		mdss_mdp_ctl_free(sctl);
	}

	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg)
{
	struct mdss_panel_data *pdata;
	int rc = 0;

	if (!ctl || !ctl->panel_data)
		return -ENODEV;

	pdata = ctl->panel_data;

	pr_debug("sending ctl=%d event=%d\n", ctl->num, event);

	do {
		if (pdata->event_handler)
			rc = pdata->event_handler(pdata, event, arg);
		pdata = pdata->next;
	} while (rc == 0 && pdata);

	return rc;
}

static void mdss_mdp_ctl_restore_sub(struct mdss_mdp_ctl *ctl)
{
	u32 temp;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	temp = readl_relaxed(ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	writel_relaxed(temp, ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

/*
 * mdss_mdp_ctl_restore() - restore mdp ctl path
 * @ctl: mdp controller.
 *
 * This function is called whenever MDP comes out of a power collapse as
 * a result of a screen update when DSI ULPS mode is enabled. It restores
 * the MDP controller's software state to the hardware registers.
 */
void mdss_mdp_ctl_restore(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;

	sctl = mdss_mdp_get_split_ctl(ctl);
	mdss_mdp_ctl_restore_sub(ctl);
	if (sctl) {
		mdss_mdp_ctl_restore_sub(sctl);
		mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
	}
}

static int mdss_mdp_ctl_start_sub(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp;
	int ret = 0;
	int i, nmixers;

	pr_debug("ctl_num=%d\n", ctl->num);

	/*
	 * Need start_fnc in 2 cases:
	 * (1) handoff
	 * (2) continuous splash finished.
	 */
	if (handoff || !ctl->panel_data->panel_info.cont_splash_enabled) {
		if (ctl->start_fnc)
			ret = ctl->start_fnc(ctl);
		else
			pr_warn("no start function for ctl=%d type=%d\n",
					ctl->num,
					ctl->panel_data->panel_info.type);

		if (ret) {
			pr_err("unable to start intf\n");
			return ret;
		}
	}

	if (!ctl->panel_data->panel_info.cont_splash_enabled) {
		nmixers = MDSS_MDP_INTF_MAX_LAYERMIXER +
			MDSS_MDP_WB_MAX_LAYERMIXER;
		for (i = 0; i < nmixers; i++)
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(i), 0);
	}

	mixer = ctl->mixer_left;
	mdss_mdp_pp_resume(ctl, mixer->num);
	mixer->params_changed++;

	temp = readl_relaxed(ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	writel_relaxed(temp, ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);

	outsize = (mixer->height << 16) | mixer->width;
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (ctl->panel_data->panel_info.fbc.enabled) {
		ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
				&ctl->panel_data->panel_info);
	}

	return ret;
}

int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int ret = 0;

	if (ctl->power_on) {
		pr_debug("%d: panel already on!\n", __LINE__);
		return 0;
	}

	ret = mdss_mdp_ctl_setup(ctl);
	if (ret)
		return ret;

	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	/*
	 * keep power_on false during handoff to avoid unexpected
	 * operations to overlay.
	 */
	if (!handoff)
		ctl->power_on = true;

	memset(&ctl->cur_perf, 0, sizeof(ctl->cur_perf));

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_RESET, NULL);
	if (ret) {
		pr_err("panel power on failed ctl=%d\n", ctl->num);
		goto error;
	}

	ret = mdss_mdp_ctl_start_sub(ctl, handoff);
	if (ret == 0) {
		if (sctl) { /* split display is available */
			ret = mdss_mdp_ctl_start_sub(sctl, handoff);
			if (!ret)
				mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		} else if (ctl->mixer_right) {
			struct mdss_mdp_mixer *mixer = ctl->mixer_right;
			u32 out;

			mdss_mdp_pp_resume(ctl, mixer->num);
			mixer->params_changed++;
			out = (mixer->height << 16) | mixer->width;
			mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, out);
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
		}
	}
	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
error:
	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 off;

	if (!ctl->power_on) {
		pr_debug("%s %d already off!\n", __func__, __LINE__);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);

	pr_debug("ctl_num=%d\n", ctl->num);

	mutex_lock(&ctl->lock);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);

	if (ctl->stop_fnc)
		ret = ctl->stop_fnc(ctl);
	else
		pr_warn("no stop func for ctl=%d\n", ctl->num);

	if (sctl && sctl->stop_fnc) {
		ret = sctl->stop_fnc(sctl);

		mdss_mdp_ctl_split_display_enable(0, ctl, sctl);
	}

	if (ret) {
		pr_warn("error powering off intf ctl=%d\n", ctl->num);
	} else {
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, 0);
		if (sctl)
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP, 0);

		if (ctl->mixer_left) {
			off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_left);
			mdss_mdp_ctl_write(ctl, off, 0);
		}

		if (ctl->mixer_right) {
			off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_right);
			mdss_mdp_ctl_write(ctl, off, 0);
		}

		ctl->power_on = false;
		ctl->play_cnt = 0;
		mdss_mdp_ctl_perf_update(ctl, 0);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
}

/*
 * mdss_mdp_ctl_reset() - reset mdp ctl path.
 * @ctl: mdp controller.
 * this function called when underflow happen,
 * it will reset mdp ctl path and poll for its completion
 *
 * Note: called within atomic context.
 */
int mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl)
{
	u32 status = 1;
	int cnt = 20;

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_SW_RESET, 1);

	/*
	 * it takes around 30us to have mdp finish resetting its ctl path
	 * poll every 50us so that reset should be completed at 1st poll
	 */

	do {
		udelay(50);
		status = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_SW_RESET);
		status &= 0x01;
		pr_debug("status=%x\n", status);
		cnt--;
		if (cnt == 0) {
			pr_err("timeout\n");
			return -EAGAIN;
		}
	} while (status);

	return 0;
}

void mdss_mdp_set_roi(struct mdss_mdp_ctl *ctl,
		struct mdp_display_commit *data)
{
	struct mdss_rect temp_roi, mixer_roi;

	temp_roi.x =  data->roi.x;
	temp_roi.y =  data->roi.y;
	temp_roi.w =  data->roi.w;
	temp_roi.h =  data->roi.h;

	/*
	 * No Partial Update for:
	 * 1) dual DSI panels
	 * 2) non-cmd mode panels
	*/
	if (!temp_roi.w || !temp_roi.h || ctl->mixer_right ||
			(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL) ||
			!ctl->panel_data->panel_info.partial_update_enabled) {
		temp_roi = (struct mdss_rect)
				{0, 0, ctl->mixer_left->width,
					ctl->mixer_left->height};
	}

	ctl->roi_changed = 0;
	if (!mdss_rect_cmp(&temp_roi, &ctl->roi)) {
		ctl->roi = temp_roi;
		ctl->roi_changed++;

		mixer_roi = ctl->mixer_left->roi;
		if ((mixer_roi.w != temp_roi.w) ||
			(mixer_roi.h != temp_roi.h)) {
			ctl->mixer_left->roi = temp_roi;
			ctl->mixer_left->params_changed++;
		}
	}
	pr_debug("ROI requested: [%d, %d, %d, %d]\n",
			ctl->roi.x, ctl->roi.y, ctl->roi.w, ctl->roi.h);
}

static inline u32 mdss_mdp_mpq_pipe_num_map(u32 pipe_num)
{
	u32 mpq_num;
	if (pipe_num == MDSS_MDP_SSPP_VIG3)
		mpq_num = MDSS_MDP_SSPP_VIG2 + 1;
	else if (pipe_num == MDSS_MDP_SSPP_RGB0)
		mpq_num = MDSS_MDP_SSPP_VIG2 + 2;
	else if (pipe_num == MDSS_MDP_SSPP_RGB1)
		mpq_num = MDSS_MDP_SSPP_VIG2 + 3;
	else
		mpq_num = pipe_num;
	return mpq_num;
}

static void mdss_mdp_mixer_setup(struct mdss_mdp_ctl *master_ctl,
	int mixer_mux)
{
	int i;
	int stage, screen_state, outsize;
	u32 off, blend_op, blend_stage, mpq_num;
	u32 mixercfg = 0, mixer_op_mode = 0, bg_alpha_enable = 0;
	u32 fg_alpha = 0, bg_alpha = 0;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer = mdss_mdp_mixer_get(master_ctl,
		mixer_mux);

	if (!mixer)
		return;

	ctl = mixer->ctl;
	if (!ctl)
		return;

	mixer->params_changed = 0;
	/* check if mixer setup for rotator is needed */
	if (mixer->rotator_mode) {
		off = __mdss_mdp_ctl_get_mixer_off(mixer);
		mdss_mdp_ctl_write(mixer->ctl, off, 0);
		return;
	}

	trace_mdp_mixer_update(mixer->num);
	pr_debug("setup mixer=%d\n", mixer->num);
	screen_state = ctl->force_screen_state;

	outsize = (mixer->roi.h << 16) | mixer->roi.w;
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (screen_state == MDSS_SCREEN_FORCE_BLANK) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
		goto update_mixer;
	}

	pipe = mixer->stage_pipe[MDSS_MDP_STAGE_BASE * MAX_PIPES_PER_STAGE];
	if (pipe == NULL) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
	} else {
		if (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_200) {
			mpq_num = mdss_mdp_mpq_pipe_num_map(pipe->num);
			mixercfg = 1 << (3 * mpq_num);
		} else if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3) {
			/* Add 2 to account for Cursor & Border bits */
			mixercfg = 1 << ((3 * pipe->num)+2);
		} else {
			mixercfg = 1 << (3 * pipe->num);
		}
		if (pipe->src_fmt->alpha_enable)
			bg_alpha_enable = 1;
	}

	i = MDSS_MDP_STAGE_0 * MAX_PIPES_PER_STAGE;
	for (; i < MAX_PIPES_PER_LM; i++) {
		pipe = mixer->stage_pipe[i];
		if (pipe == NULL)
			continue;

		stage = i / MAX_PIPES_PER_STAGE;
		if (stage != pipe->mixer_stage) {
			mixer->stage_pipe[i] = NULL;
			continue;
		}

		blend_stage = stage - MDSS_MDP_STAGE_0;
		off = MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
			    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
		fg_alpha = pipe->alpha;
		bg_alpha = 0xFF - pipe->alpha;
		/* keep fg alpha */
		mixer_op_mode |= 1 << (blend_stage + 1);

		switch (pipe->blend_op) {
		case BLEND_OP_OPAQUE:

			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);

			pr_debug("pnum=%d stg=%d op=OPAQUE\n", pipe->num,
					stage);
			break;

		case BLEND_OP_PREMULTIPLIED:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
						MDSS_MDP_BLEND_BG_MOD_ALPHA |
						MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=PREMULTIPLIED\n", pipe->num,
					stage);
			break;

		case BLEND_OP_COVERAGE:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_PIXEL |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
					       MDSS_MDP_BLEND_FG_MOD_ALPHA |
					       MDSS_MDP_BLEND_FG_INV_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=COVERAGE\n", pipe->num,
					stage);
			break;

		default:
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			pr_debug("pnum=%d stg=%d op=NONE\n", pipe->num,
					stage);
			break;
		}

		if (!pipe->src_fmt->alpha_enable && bg_alpha_enable)
			mixer_op_mode = 0;

		if (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_200) {
			mpq_num = mdss_mdp_mpq_pipe_num_map(pipe->num);
			mixercfg |= stage << (3 * mpq_num);
		} else if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3) {
			/* Add 2 to account for Cursor & Border bits */
			mixercfg |= stage << ((3 * pipe->num)+2);
		} else {
			mixercfg |= stage << (3 * pipe->num);
		}

		trace_mdp_sspp_change(pipe);

		pr_debug("stg=%d op=%x fg_alpha=%x bg_alpha=%x\n", stage,
					blend_op, fg_alpha, bg_alpha);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA,
				   fg_alpha);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA,
				   bg_alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg |= MDSS_MDP_LM_CURSOR_OUT;

update_mixer:
	pr_debug("mixer=%d mixer_cfg=%x\n", mixer->num, mixercfg);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3)
		ctl->flush_bits |= BIT(20);
	else if (mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK)
		ctl->flush_bits |= BIT(9) << mixer->num;
	else
		ctl->flush_bits |= BIT(6) << mixer->num;

	/* Read GC enable/disable status on LM */
	mixer_op_mode |=
		(mdp_mixer_read(mixer, MDSS_MDP_REG_LM_OP_MODE) & BIT(0));

	if (mdata->has_src_split && mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
		mixer_op_mode |= BIT(31);

	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OP_MODE, mixer_op_mode);
	off = __mdss_mdp_ctl_get_mixer_off(mixer);
	mdss_mdp_ctl_write(ctl, off, mixercfg);
}

int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata,
	 u32 *mixer_offsets, u32 *dspp_offsets, u32 *pingpong_offsets,
	 u32 type, u32 len)
{
	struct mdss_mdp_mixer *head;
	u32 i;
	int rc = 0;
	u32 size = len;

	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		size++;

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_mixer) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup mixer type=%d :kzalloc fail\n",
			type);
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].type = type;
		head[i].base = mdata->mdss_base + mixer_offsets[i];
		head[i].ref_cnt = 0;
		head[i].num = i;
		if (type == MDSS_MDP_MIXER_TYPE_INTF) {
			head[i].dspp_base = mdata->mdss_base + dspp_offsets[i];
			head[i].pingpong_base = mdata->mdss_base +
				pingpong_offsets[i];
		}
	}

	/*
	 * Duplicate the last writeback mixer for concurrent line and block mode
	 * operations
	*/
	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		head[len] = head[len - 1];

	switch (type) {

	case MDSS_MDP_MIXER_TYPE_INTF:
		mdata->mixer_intf = head;
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mdata->mixer_wb = head;
		break;

	default:
		pr_err("Invalid mixer type=%d\n", type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata,
	u32 *ctl_offsets, u32 *wb_offsets, u32 len)
{
	struct mdss_mdp_ctl *head;
	struct mutex *shared_lock = NULL;
	u32 i;
	u32 size = len;

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		size++;
		shared_lock = devm_kzalloc(&mdata->pdev->dev,
					   sizeof(struct mutex),
					   GFP_KERNEL);
		if (!shared_lock) {
			pr_err("unable to allocate mem for mutex\n");
			return -ENOMEM;
		}
		mutex_init(shared_lock);
	}

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_ctl) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup ctl and wb: kzalloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].num = i;
		head[i].base = (mdata->mdss_base) + ctl_offsets[i];
		head[i].wb_base = (mdata->mdss_base) + wb_offsets[i];
		head[i].ref_cnt = 0;
	}

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		head[len - 1].shared_lock = shared_lock;
		/*
		 * Allocate a virtual ctl to be able to perform simultaneous
		 * line mode and block mode operations on the same
		 * writeback block
		*/
		head[len] = head[len - 1];
		head[len].num = head[len - 1].num;
	}
	mdata->ctl_off = head;

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;

	if (!ctl) {
		pr_err("ctl not initialized\n");
		return NULL;
	}

	switch (mux) {
	case MDSS_MDP_MIXER_MUX_DEFAULT:
	case MDSS_MDP_MIXER_MUX_LEFT:
		mixer = ctl->mixer_left;
		break;
	case MDSS_MDP_MIXER_MUX_RIGHT:
		mixer = ctl->mixer_right;
		break;
	}

	return mixer;
}

struct mdss_mdp_pipe *mdss_mdp_get_staged_pipe(struct mdss_mdp_ctl *ctl,
	int mux, int stage, bool is_right_blend)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_mixer *mixer;
	int index = (stage * MAX_PIPES_PER_STAGE) + (int)is_right_blend;

	if (!ctl)
		return NULL;

	BUG_ON(index > MAX_PIPES_PER_LM);

	mixer = mdss_mdp_mixer_get(ctl, mux);
	if (mixer)
		pipe = mixer->stage_pipe[index];

	pr_debug("%pS index=%d pipe%d\n", __builtin_return_address(0),
		index, pipe ? pipe->num : -1);
	return pipe;
}

int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe,
			 struct mdss_mdp_mixer *mixer, int params_changed)
{
	struct mdss_mdp_ctl *ctl;
	int i, j;
	u32 mpq_num;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	if (pipe->mixer_stage >= MDSS_MDP_MAX_STAGE) {
		pr_err("invalid mixer stage\n");
		return -EINVAL;
	}

	pr_debug("pnum=%x mixer=%d stage=%d\n", pipe->num, mixer->num,
			pipe->mixer_stage);

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	if (params_changed) {
		mixer->params_changed++;
		for (i = MDSS_MDP_STAGE_UNUSED; i < MDSS_MDP_MAX_STAGE; i++) {
			j = i * MAX_PIPES_PER_STAGE;
			if (pipe->is_right_blend)
				j++;

			if (i == pipe->mixer_stage)
				mixer->stage_pipe[j] = pipe;
			else if (mixer->stage_pipe[j] == pipe)
				mixer->stage_pipe[j] = NULL;
		}
	}

	if (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_200) {
		mpq_num = mdss_mdp_mpq_pipe_num_map(pipe->num);
		ctl->flush_bits |= BIT(mpq_num);
	} else if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		ctl->flush_bits |= BIT(pipe->num) << 5;
	else if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3)
		ctl->flush_bits |= BIT(pipe->num) << 10;
	else /* RGB/VIG 0-2 pipes */
		ctl->flush_bits |= BIT(pipe->num);

	mutex_unlock(&ctl->lock);

	return 0;
}

/**
 * mdss_mdp_mixer_unstage_all() - Unstage all pipes from mixer
 * @mixer:	Mixer from which to unstage all pipes
 *
 * Unstage any pipes that are currently attached to mixer.
 *
 * NOTE: this will not update the pipe structure, and thus a full
 * deinitialization or reconfiguration of all pipes is expected after this call.
 */
void mdss_mdp_mixer_unstage_all(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *tmp;
	int i;

	if (!mixer)
		return;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		tmp = mixer->stage_pipe[i];
		if (tmp) {
			mixer->stage_pipe[i] = NULL;
			mixer->params_changed++;
			tmp->params_changed++;
		}
	}
}

int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer)
{
	int index;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;

	index = (pipe->mixer_stage * MAX_PIPES_PER_STAGE) +
		(int)pipe->is_right_blend;

	if (pipe == mixer->stage_pipe[index]) {
		pr_debug("unstage p%d from %s side of stage=%d lm=%d ndx=%d\n",
			pipe->num, pipe->is_right_blend ? "right" : "left",
			pipe->mixer_stage, mixer->num, index);

		mixer->params_changed++;
		mixer->stage_pipe[index] = NULL;
	}

	return 0;
}

int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl, int fps)
{
	int ret = 0;
	struct mdss_mdp_ctl *sctl = NULL;

	sctl = mdss_mdp_get_split_ctl(ctl);

	if (ctl->config_fps_fnc)
		ret = ctl->config_fps_fnc(ctl, sctl, fps);

	return ret;
}

int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time)
{
	struct mdss_panel_info *pinfo;
	u32 clk_rate, clk_period;
	u32 current_line, total_line;
	u32 time_of_line, time_to_vsync;
	ktime_t current_time = ktime_get();

	if (!ctl->read_line_cnt_fnc)
		return -ENOSYS;

	pinfo = &ctl->panel_data->panel_info;
	if (!pinfo)
		return -ENODEV;

	clk_rate = mdss_mdp_get_pclk_rate(ctl);

	clk_rate /= 1000;	/* in kHz */
	if (!clk_rate)
		return -EINVAL;

	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = 1000000000 / clk_rate;
	if (!clk_period)
		return -EINVAL;

	time_of_line = (pinfo->lcdc.h_back_porch +
		 pinfo->lcdc.h_front_porch +
		 pinfo->lcdc.h_pulse_width +
		 pinfo->xres) * clk_period;

	time_of_line /= 1000;	/* in nano second */
	if (!time_of_line)
		return -EINVAL;

	current_line = ctl->read_line_cnt_fnc(ctl);

	total_line = pinfo->lcdc.v_back_porch +
		pinfo->lcdc.v_front_porch +
		pinfo->lcdc.v_pulse_width +
		pinfo->yres;

	if (current_line > total_line)
		return -EINVAL;

	time_to_vsync = time_of_line * (total_line - current_line);
	if (!time_to_vsync)
		return -EINVAL;

	*wakeup_time = ktime_add_ns(current_time, time_to_vsync);

	pr_debug("clk_rate=%dkHz clk_period=%d cur_line=%d tot_line=%d\n",
		clk_rate, clk_period, current_line, total_line);
	pr_debug("time_to_vsync=%d current_time=%d wakeup_time=%d\n",
		time_to_vsync, (int)ktime_to_ms(current_time),
		(int)ktime_to_ms(*wakeup_time));

	return 0;
}

int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl)
{
	int ret;
	u32 reg_data, flush_data;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl) {
		pr_err("invalid ctl\n");
		return -ENODEV;
	}

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	if (ctl->wait_fnc)
		ret = ctl->wait_fnc(ctl, NULL);

	trace_mdp_commit(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0);

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_103)) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		reg_data = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_FLUSH);
		flush_data = readl_relaxed(mdata->mdp_base + AHB_CLK_OFFSET);
		if ((flush_data & BIT(28)) &&
		    !(ctl->flush_reg_data & reg_data)) {

			flush_data &= ~(BIT(28));
			writel_relaxed(flush_data,
					 mdata->mdp_base + AHB_CLK_OFFSET);
			ctl->flush_reg_data = 0;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	}

	mutex_unlock(&ctl->lock);
	return ret;
}

int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl)
{
	int ret;

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	if (ctl->wait_pingpong)
		ret = ctl->wait_pingpong(ctl, NULL);

	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int ret = 0;
	bool is_bw_released;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	mutex_lock(&ctl->lock);
	pr_debug("commit ctl=%d play_cnt=%d\n", ctl->num, ctl->play_cnt);

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);
	mdss_mdp_ctl_perf_set_transaction_status(ctl, PERF_SW_COMMIT_STATE,
		PERF_STATUS_BUSY);

	if (is_bw_released || ctl->force_screen_state ||
		(ctl->mixer_left && ctl->mixer_left->params_changed) ||
		(ctl->mixer_right && ctl->mixer_right->params_changed)) {
		if (ctl->prepare_fnc)
			ret = ctl->prepare_fnc(ctl, arg);
		if (ret) {
			pr_err("error preparing display\n");
			goto done;
		}

		mdss_mdp_ctl_perf_update(ctl, 1);

		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_LEFT);
		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_RIGHT);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */

		if (sctl) {
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP,
					sctl->opmode);
			sctl->flush_bits |= BIT(17);
		}
	}

	if (!ctl->shared_lock)
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_READY);

	if (ctl->wait_pingpong)
		ctl->wait_pingpong(ctl, NULL);

	ctl->roi_bkup.w = ctl->roi.w;
	ctl->roi_bkup.h = ctl->roi.h;

	if (ctl->mfd && ctl->mfd->dcm_state != DTM_ENTER)
		/* postprocessing setup, including dspp */
		mdss_mdp_pp_setup_locked(ctl);

	if (sctl && ctl->split_flush_en) {
		ctl->flush_bits |= sctl->flush_bits;
		sctl->flush_bits = 0;
	}

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl->flush_bits);
	if (sctl && sctl->flush_bits) {
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH,
			sctl->flush_bits);
		sctl->flush_bits = 0;
	}
	wmb();
	ctl->flush_reg_data = ctl->flush_bits;
	ctl->flush_bits = 0;

	mdss_mdp_xlog_mixer_reg(ctl);

	if (ctl->display_fnc)
		ret = ctl->display_fnc(ctl, arg); /* kickoff */
	if (ret)
		pr_warn("error displaying frame\n");

	ctl->play_cnt++;

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
}

void mdss_mdp_ctl_notifier_register(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	blocking_notifier_chain_register(&ctl->notifier_head, notifier);
}

void mdss_mdp_ctl_notifier_unregister(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	blocking_notifier_chain_unregister(&ctl->notifier_head, notifier);
}

int mdss_mdp_ctl_notify(struct mdss_mdp_ctl *ctl, int event)
{
	return blocking_notifier_call_chain(&ctl->notifier_head, event, ctl);
}

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id)
{
	int i;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata;
	u32 mixer_cnt = 0;
	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = mdss_mdp_get_mdata();
	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if ((ctl->power_on) && (ctl->mfd) &&
			(ctl->mfd->index == fb_num)) {
			if (ctl->mixer_left) {
				mixer_id[mixer_cnt] = ctl->mixer_left->num;
				mixer_cnt++;
			}
			if (mixer_cnt && ctl->mixer_right) {
				mixer_id[mixer_cnt] = ctl->mixer_right->num;
				mixer_cnt++;
			}
			if (mixer_cnt)
				break;
		}
	}
	mutex_unlock(&mdss_mdp_ctl_lock);
	return mixer_cnt;
}

/**
 * @mdss_mdp_ctl_mixer_switch() - return ctl mixer of @return_type
 * @ctl: Pointer to ctl structure to be switched.
 * @return_type: wb_type of the ctl to be switched to.
 *
 * Virtual mixer switch should be performed only when there is no
 * dedicated wfd block and writeback block is shared.
 */
struct mdss_mdp_ctl *mdss_mdp_ctl_mixer_switch(struct mdss_mdp_ctl *ctl,
					       u32 return_type)
{
	int i;
	struct mdss_data_type *mdata = ctl->mdata;

	if (ctl->wb_type == return_type) {
		mdata->mixer_switched = false;
		return ctl;
	}
	for (i = 0; i <= mdata->nctl; i++) {
		if (mdata->ctl_off[i].wb_type == return_type) {
			pr_debug("switching mixer from ctl=%d to ctl=%d\n",
				 ctl->num, mdata->ctl_off[i].num);
			mdata->mixer_switched = true;
			return mdata->ctl_off + i;
		}
	}
	pr_err("unable to switch mixer to type=%d\n", return_type);
	return NULL;
}

static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer)
{
	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3)
			return MDSS_MDP_CTL_X_LAYER_5;
		else
			return MDSS_MDP_REG_CTL_LAYER(mixer->num);
	} else {
		return MDSS_MDP_REG_CTL_LAYER(mixer->num +
				MDSS_MDP_INTF_LAYERMIXER3);
	}
}

u32 mdss_mdp_get_mixercfg(struct mdss_mdp_mixer *mixer)
{
	if (!mixer && !mixer->ctl)
		return 0;

	return mdss_mdp_ctl_read(mixer->ctl,
		__mdss_mdp_ctl_get_mixer_off(mixer));
}

static int __mdss_mdp_mixer_handoff_helper(struct mdss_mdp_mixer *mixer,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;

	if (!mixer) {
		rc = -EINVAL;
		goto error;
	}

	if (mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED] != NULL) {
		pr_err("More than one pipe staged on mixer num %d\n",
			mixer->num);
		rc = -EINVAL;
		goto error;
	}

	pr_debug("Staging pipe num %d on mixer num %d\n",
		pipe->num, mixer->num);
	mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED] = pipe;
	pipe->mixer_left = mixer;
	pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;

error:
	return rc;
}

/**
 * mdss_mdp_mixer_handoff() - Stages a given pipe on the appropriate mixer
 * @ctl:  pointer to the control structure associated with the overlay device.
 * @num:  the mixer number on which the pipe needs to be staged.
 * @pipe: pointer to the pipe to be staged.
 *
 * Function stages a given pipe on either the left mixer or the right mixer
 * for the control structre based on the mixer number. If the input mixer
 * number does not match either of the mixers then an error is returned.
 * This function is called during overlay handoff when certain pipes are
 * already staged by the bootloader.
 */
int mdss_mdp_mixer_handoff(struct mdss_mdp_ctl *ctl, u32 num,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	struct mdss_mdp_mixer *mx_left = ctl->mixer_left;
	struct mdss_mdp_mixer *mx_right = ctl->mixer_right;

	/*
	 * For performance calculations, stage the handed off pipe
	 * as MDSS_MDP_STAGE_UNUSED
	 */
	if (mx_left && (mx_left->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_left, pipe);
	} else if (mx_right && (mx_right->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_right, pipe);
	} else {
		pr_err("pipe num %d staged on unallocated mixer num %d\n",
			pipe->num, num);
		rc = -EINVAL;
	}

	return rc;
}

static void mdss_mdp_xlog_mixer_reg(struct mdss_mdp_ctl *ctl)
{
	int i, off;
	u32 data[MDSS_MDP_INTF_MAX_LAYERMIXER];

	for (i = 0; i < MDSS_MDP_INTF_MAX_LAYERMIXER; i++) {
		off =  MDSS_MDP_REG_CTL_LAYER(i);
		data[i] = mdss_mdp_ctl_read(ctl, off);
	}
	MDSS_XLOG(data[MDSS_MDP_INTF_LAYERMIXER0],
		data[MDSS_MDP_INTF_LAYERMIXER1],
		data[MDSS_MDP_INTF_LAYERMIXER2],
		data[MDSS_MDP_INTF_LAYERMIXER3], off);
}
