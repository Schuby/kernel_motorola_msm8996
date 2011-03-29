/*
 * Marvell Wireless LAN device driver: major data structures and prototypes
 *
 * Copyright (C) 2011, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

#ifndef _MWIFIEX_MAIN_H_
#define _MWIFIEX_MAIN_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <net/lib80211.h>
#include <linux/firmware.h>
#include <linux/ctype.h>

#include "decl.h"
#include "ioctl.h"
#include "util.h"
#include "fw.h"

extern const char driver_version[];
extern struct mwifiex_adapter *g_adapter;

enum {
	MWIFIEX_NO_WAIT,
	MWIFIEX_IOCTL_WAIT,
	MWIFIEX_CMD_WAIT,
	MWIFIEX_PROC_WAIT,
	MWIFIEX_WSTATS_WAIT
};

#define DRV_MODE_STA       0x1
#define DRV_MODE_UAP       0x2
#define DRV_MODE_UAP_STA   0x3

#define SD8787_W0   0x30
#define SD8787_W1   0x31
#define SD8787_A0   0x40
#define SD8787_A1   0x41

#define DEFAULT_FW_NAME "mrvl/sd8787_uapsta.bin"
#define SD8787_W1_FW_NAME "mrvl/sd8787_uapsta_w1.bin"
#define SD8787_AX_FW_NAME "mrvl/sd8787_uapsta.bin"

struct mwifiex_drv_mode {
	u16 drv_mode;
	u16 intf_num;
	struct mwifiex_bss_attr *bss_attr;
};


#define MWIFIEX_DEFAULT_WATCHDOG_TIMEOUT	(5 * HZ)

#define MWIFIEX_TIMER_10S			10000
#define MWIFIEX_TIMER_1S			1000

#define NL_MAX_PAYLOAD      1024
#define NL_MULTICAST_GROUP  1

#define MAX_TX_PENDING      60

#define HEADER_ALIGNMENT                8

#define MWIFIEX_UPLD_SIZE               (2312)

#define MAX_EVENT_SIZE                  1024

#define ARP_FILTER_MAX_BUF_SIZE         68

#define MWIFIEX_KEY_BUFFER_SIZE			16
#define MWIFIEX_DEFAULT_LISTEN_INTERVAL 10
#define MWIFIEX_MAX_REGION_CODE         7

#define DEFAULT_BCN_AVG_FACTOR          8
#define DEFAULT_DATA_AVG_FACTOR         8

#define FIRST_VALID_CHANNEL				0xff
#define DEFAULT_AD_HOC_CHANNEL			6
#define DEFAULT_AD_HOC_CHANNEL_A		36

#define DEFAULT_BCN_MISS_TIMEOUT		5

#define MAX_SCAN_BEACON_BUFFER			8000

#define SCAN_BEACON_ENTRY_PAD			6

#define MWIFIEX_PASSIVE_SCAN_CHAN_TIME	200
#define MWIFIEX_ACTIVE_SCAN_CHAN_TIME	200
#define MWIFIEX_SPECIFIC_SCAN_CHAN_TIME	110

#define SCAN_RSSI(RSSI)					(0x100 - ((u8)(RSSI)))

#define MWIFIEX_MAX_TOTAL_SCAN_TIME	(MWIFIEX_TIMER_10S - MWIFIEX_TIMER_1S)

#define RSN_GTK_OUI_OFFSET				2

#define MWIFIEX_OUI_NOT_PRESENT			0
#define MWIFIEX_OUI_PRESENT				1

#define IS_CARD_RX_RCVD(adapter) (adapter->cmd_resp_received || \
					adapter->event_received || \
					adapter->data_received)

#define MWIFIEX_TYPE_CMD				1
#define MWIFIEX_TYPE_DATA				0
#define MWIFIEX_TYPE_EVENT				3

#define DBG_CMD_NUM						5

#define MAX_BITMAP_RATES_SIZE			10

#define MAX_CHANNEL_BAND_BG     14

#define MAX_FREQUENCY_BAND_BG   2484

struct mwifiex_dbg {
	u32 num_cmd_host_to_card_failure;
	u32 num_cmd_sleep_cfm_host_to_card_failure;
	u32 num_tx_host_to_card_failure;
	u32 num_event_deauth;
	u32 num_event_disassoc;
	u32 num_event_link_lost;
	u32 num_cmd_deauth;
	u32 num_cmd_assoc_success;
	u32 num_cmd_assoc_failure;
	u32 num_tx_timeout;
	u32 num_cmd_timeout;
	u16 timeout_cmd_id;
	u16 timeout_cmd_act;
	u16 last_cmd_id[DBG_CMD_NUM];
	u16 last_cmd_act[DBG_CMD_NUM];
	u16 last_cmd_index;
	u16 last_cmd_resp_id[DBG_CMD_NUM];
	u16 last_cmd_resp_index;
	u16 last_event[DBG_CMD_NUM];
	u16 last_event_index;
};

enum MWIFIEX_HARDWARE_STATUS {
	MWIFIEX_HW_STATUS_READY,
	MWIFIEX_HW_STATUS_INITIALIZING,
	MWIFIEX_HW_STATUS_FW_READY,
	MWIFIEX_HW_STATUS_INIT_DONE,
	MWIFIEX_HW_STATUS_RESET,
	MWIFIEX_HW_STATUS_CLOSING,
	MWIFIEX_HW_STATUS_NOT_READY
};

enum MWIFIEX_802_11_POWER_MODE {
	MWIFIEX_802_11_POWER_MODE_CAM,
	MWIFIEX_802_11_POWER_MODE_PSP
};

struct mwifiex_tx_param {
	u32 next_pkt_len;
};

enum MWIFIEX_PS_STATE {
	PS_STATE_AWAKE,
	PS_STATE_PRE_SLEEP,
	PS_STATE_SLEEP_CFM,
	PS_STATE_SLEEP
};

struct mwifiex_add_ba_param {
	u32 tx_win_size;
	u32 rx_win_size;
	u32 timeout;
};

struct mwifiex_tx_aggr {
	u8 ampdu_user;
	u8 ampdu_ap;
	u8 amsdu;
};

struct mwifiex_ra_list_tbl {
	struct list_head list;
	struct sk_buff_head skb_head;
	u8 ra[ETH_ALEN];
	u32 total_pkts_size;
	u32 is_11n_enabled;
};

struct mwifiex_tid_tbl {
	struct list_head ra_list;
	/* spin lock for tid table */
	spinlock_t tid_tbl_lock;
	struct mwifiex_ra_list_tbl *ra_list_curr;
};

#define WMM_HIGHEST_PRIORITY		7
#define HIGH_PRIO_TID				7
#define LOW_PRIO_TID				0

struct mwifiex_wmm_desc {
	struct mwifiex_tid_tbl tid_tbl_ptr[MAX_NUM_TID];
	u32 packets_out[MAX_NUM_TID];
	/* spin lock to protect ra_list */
	spinlock_t ra_list_spinlock;
	struct mwifiex_wmm_ac_status ac_status[IEEE80211_MAX_QUEUES];
	enum mwifiex_wmm_ac_e ac_down_graded_vals[IEEE80211_MAX_QUEUES];
	u32 drv_pkt_delay_max;
	u8 queue_priority[IEEE80211_MAX_QUEUES];
	u32 user_pri_pkt_tx_ctrl[WMM_HIGHEST_PRIORITY + 1];	/* UP: 0 to 7 */

};

struct mwifiex_802_11_security {
	u8 wpa_enabled;
	u8 wpa2_enabled;
	u8 wapi_enabled;
	u8 wapi_key_on;
	enum MWIFIEX_802_11_WEP_STATUS wep_status;
	u32 authentication_mode;
	u32 encryption_mode;
};

struct ieee_types_header {
	u8 element_id;
	u8 len;
} __packed;

struct ieee_obss_scan_param {
	u16 obss_scan_passive_dwell;
	u16 obss_scan_active_dwell;
	u16 bss_chan_width_trigger_scan_int;
	u16 obss_scan_passive_total;
	u16 obss_scan_active_total;
	u16 bss_width_chan_trans_delay;
	u16 obss_scan_active_threshold;
} __packed;

struct ieee_types_obss_scan_param {
	struct ieee_types_header ieee_hdr;
	struct ieee_obss_scan_param obss_scan;
} __packed;

#define MWIFIEX_SUPPORTED_RATES                 14

#define MWIFIEX_SUPPORTED_RATES_EXT             32

#define IEEE_MAX_IE_SIZE			256

struct ieee_types_vendor_specific {
	struct ieee_types_vendor_header vend_hdr;
	u8 data[IEEE_MAX_IE_SIZE - sizeof(struct ieee_types_vendor_header)];
} __packed;

struct ieee_types_generic {
	struct ieee_types_header ieee_hdr;
	u8 data[IEEE_MAX_IE_SIZE - sizeof(struct ieee_types_header)];
} __packed;

struct mwifiex_bssdescriptor {
	u8 mac_address[ETH_ALEN];
	struct mwifiex_802_11_ssid ssid;
	u32 privacy;
	s32 rssi;
	u32 channel;
	u32 freq;
	u16 beacon_period;
	u8 erp_flags;
	u32 bss_mode;
	u8 supported_rates[MWIFIEX_SUPPORTED_RATES];
	u8 data_rates[MWIFIEX_SUPPORTED_RATES];
	/* Network band.
	 * BAND_B(0x01): 'b' band
	 * BAND_G(0x02): 'g' band
	 * BAND_A(0X04): 'a' band
	 */
	u16 bss_band;
	long long network_tsf;
	u8 time_stamp[8];
	union ieee_types_phy_param_set phy_param_set;
	union ieee_types_ss_param_set ss_param_set;
	u16 cap_info_bitmap;
	struct ieee_types_wmm_parameter wmm_ie;
	u8  disable_11n;
	struct ieee80211_ht_cap *bcn_ht_cap;
	u16 ht_cap_offset;
	struct ieee80211_ht_info *bcn_ht_info;
	u16 ht_info_offset;
	u8 *bcn_bss_co_2040;
	u16 bss_co_2040_offset;
	u8 *bcn_ext_cap;
	u16 ext_cap_offset;
	struct ieee_types_obss_scan_param *bcn_obss_scan;
	u16 overlap_bss_offset;
	struct ieee_types_vendor_specific *bcn_wpa_ie;
	u16 wpa_offset;
	struct ieee_types_generic *bcn_rsn_ie;
	u16 rsn_offset;
	struct ieee_types_generic *bcn_wapi_ie;
	u16 wapi_offset;
	u8 *beacon_buf;
	u32 beacon_buf_size;
	u32 beacon_buf_size_max;

};

struct mwifiex_current_bss_params {
	struct mwifiex_bssdescriptor bss_descriptor;
	u8 wmm_enabled;
	u8 wmm_uapsd_enabled;
	u8 band;
	u32 num_of_rates;
	u8 data_rates[MWIFIEX_SUPPORTED_RATES];
};

struct mwifiex_sleep_params {
	u16 sp_error;
	u16 sp_offset;
	u16 sp_stable_time;
	u8 sp_cal_control;
	u8 sp_ext_sleep_clk;
	u16 sp_reserved;
};

struct mwifiex_sleep_period {
	u16 period;
	u16 reserved;
};

struct mwifiex_wep_key {
	u32 length;
	u32 key_index;
	u32 key_length;
	u8 key_material[MWIFIEX_KEY_BUFFER_SIZE];
};

#define MAX_REGION_CHANNEL_NUM  2

struct mwifiex_chan_freq_power {
	u16 channel;
	u32 freq;
	u16 max_tx_power;
	u8 unsupported;
};

enum state_11d_t {
	DISABLE_11D = 0,
	ENABLE_11D = 1,
};

#define MWIFIEX_MAX_TRIPLET_802_11D		83

struct mwifiex_802_11d_domain_reg {
	u8 country_code[IEEE80211_COUNTRY_STRING_LEN];
	u8 no_of_triplet;
	struct ieee80211_country_ie_triplet
		triplet[MWIFIEX_MAX_TRIPLET_802_11D];
};

struct mwifiex_vendor_spec_cfg_ie {
	u16 mask;
	u16 flag;
	u8 ie[MWIFIEX_MAX_VSIE_LEN];
};

struct wps {
	u8 session_enable;
};

struct mwifiex_adapter;
struct mwifiex_private;

struct mwifiex_private {
	struct mwifiex_adapter *adapter;
	u8 bss_index;
	u8 bss_type;
	u8 bss_role;
	u8 bss_priority;
	u8 bss_num;
	u8 frame_type;
	u8 curr_addr[ETH_ALEN];
	u8 media_connected;
	u32 num_tx_timeout;
	struct net_device *netdev;
	struct net_device_stats stats;
	u16 curr_pkt_filter;
	u32 bss_mode;
	u32 pkt_tx_ctrl;
	u16 tx_power_level;
	u8 max_tx_power_level;
	u8 min_tx_power_level;
	u8 tx_rate;
	u8 tx_htinfo;
	u8 rxpd_htinfo;
	u8 rxpd_rate;
	u16 rate_bitmap;
	u16 bitmap_rates[MAX_BITMAP_RATES_SIZE];
	u32 data_rate;
	u8 is_data_rate_auto;
	u16 bcn_avg_factor;
	u16 data_avg_factor;
	s16 data_rssi_last;
	s16 data_nf_last;
	s16 data_rssi_avg;
	s16 data_nf_avg;
	s16 bcn_rssi_last;
	s16 bcn_nf_last;
	s16 bcn_rssi_avg;
	s16 bcn_nf_avg;
	struct mwifiex_bssdescriptor *attempted_bss_desc;
	struct mwifiex_802_11_ssid prev_ssid;
	u8 prev_bssid[ETH_ALEN];
	struct mwifiex_current_bss_params curr_bss_params;
	u16 beacon_period;
	u16 listen_interval;
	u16 atim_window;
	u8 adhoc_channel;
	u8 adhoc_is_link_sensed;
	u8 adhoc_state;
	struct mwifiex_802_11_security sec_info;
	struct mwifiex_wep_key wep_key[NUM_WEP_KEYS];
	u16 wep_key_curr_index;
	u8 wpa_ie[256];
	u8 wpa_ie_len;
	u8 wpa_is_gtk_set;
	struct host_cmd_ds_802_11_key_material aes_key;
	u8 wapi_ie[256];
	u8 wapi_ie_len;
	u8 wmm_required;
	u8 wmm_enabled;
	u8 wmm_qosinfo;
	struct mwifiex_wmm_desc wmm;
	struct list_head tx_ba_stream_tbl_ptr;
	/* spin lock for tx_ba_stream_tbl_ptr queue */
	spinlock_t tx_ba_stream_tbl_lock;
	struct mwifiex_tx_aggr aggr_prio_tbl[MAX_NUM_TID];
	struct mwifiex_add_ba_param add_ba_param;
	u16 rx_seq[MAX_NUM_TID];
	struct list_head rx_reorder_tbl_ptr;
	/* spin lock for rx_reorder_tbl_ptr queue */
	spinlock_t rx_reorder_tbl_lock;
	/* spin lock for Rx packets */
	spinlock_t rx_pkt_lock;

#define MWIFIEX_ASSOC_RSP_BUF_SIZE  500
	u8 assoc_rsp_buf[MWIFIEX_ASSOC_RSP_BUF_SIZE];
	u32 assoc_rsp_size;

#define MWIFIEX_GENIE_BUF_SIZE      256
	u8 gen_ie_buf[MWIFIEX_GENIE_BUF_SIZE];
	u8 gen_ie_buf_len;

	struct mwifiex_vendor_spec_cfg_ie vs_ie[MWIFIEX_MAX_VSIE_NUM];

#define MWIFIEX_ASSOC_TLV_BUF_SIZE  256
	u8 assoc_tlv_buf[MWIFIEX_ASSOC_TLV_BUF_SIZE];
	u8 assoc_tlv_buf_len;

	u8 *curr_bcn_buf;
	u32 curr_bcn_size;
	/* spin lock for beacon buffer */
	spinlock_t curr_bcn_buf_lock;
	u16 ioctl_wait_q_woken;
	wait_queue_head_t ioctl_wait_q;
	u16 cmd_wait_q_woken;
	wait_queue_head_t cmd_wait_q;
	struct wireless_dev *wdev;
	struct mwifiex_chan_freq_power cfp;
	char version_str[128];
#ifdef CONFIG_DEBUG_FS
	struct dentry *dfs_dev_dir;
#endif
	u8 nick_name[16];
	struct iw_statistics w_stats;
	u16 w_stats_wait_q_woken;
	wait_queue_head_t w_stats_wait_q;
	u16 current_key_index;
	struct semaphore async_sem;
	u8 scan_pending_on_block;
	u8 report_scan_result;
	struct cfg80211_scan_request *scan_request;
	int scan_result_status;
	bool assoc_request;
	u16 assoc_result;
	bool ibss_join_request;
	u16 ibss_join_result;
	bool disconnect;
	u8 cfg_bssid[6];
	struct workqueue_struct *workqueue;
	struct work_struct cfg_workqueue;
	u8 country_code[IEEE80211_COUNTRY_STRING_LEN];
	struct wps wps;
	u8 scan_block;
};

enum mwifiex_ba_status {
	BA_STREAM_NOT_SETUP = 0,
	BA_STREAM_SETUP_INPROGRESS,
	BA_STREAM_SETUP_COMPLETE
};

struct mwifiex_tx_ba_stream_tbl {
	struct list_head list;
	int tid;
	u8 ra[ETH_ALEN];
	enum mwifiex_ba_status ba_status;
};

struct mwifiex_rx_reorder_tbl;

struct reorder_tmr_cnxt {
	struct timer_list timer;
	struct mwifiex_rx_reorder_tbl *ptr;
	struct mwifiex_private *priv;
};

struct mwifiex_rx_reorder_tbl {
	struct list_head list;
	int tid;
	u8 ta[ETH_ALEN];
	int start_win;
	int win_size;
	void **rx_reorder_ptr;
	struct reorder_tmr_cnxt timer_context;
};

struct mwifiex_bss_prio_node {
	struct list_head list;
	struct mwifiex_private *priv;
};

struct mwifiex_bss_prio_tbl {
	struct list_head bss_prio_head;
	/* spin lock for bss priority  */
	spinlock_t bss_prio_lock;
	struct mwifiex_bss_prio_node *bss_prio_cur;
};

struct cmd_ctrl_node {
	struct list_head list;
	struct mwifiex_private *priv;
	u32 cmd_oid;
	u32 cmd_flag;
	struct sk_buff *cmd_skb;
	struct sk_buff *resp_skb;
	void *data_buf;
	void *wq_buf;
	struct sk_buff *skb;
};

struct mwifiex_if_ops {
	int (*init_if) (struct mwifiex_adapter *);
	void (*cleanup_if) (struct mwifiex_adapter *);
	int (*check_fw_status) (struct mwifiex_adapter *, u32, int *);
	int (*prog_fw) (struct mwifiex_adapter *, struct mwifiex_fw_image *);
	int (*register_dev) (struct mwifiex_adapter *);
	void (*unregister_dev) (struct mwifiex_adapter *);
	int (*enable_int) (struct mwifiex_adapter *);
	int (*process_int_status) (struct mwifiex_adapter *);
	int (*host_to_card) (struct mwifiex_adapter *, u8,
			     u8 *payload, u32 pkt_len,
			     struct mwifiex_tx_param *);
	int (*wakeup) (struct mwifiex_adapter *);
	int (*wakeup_complete) (struct mwifiex_adapter *);

	void (*update_mp_end_port) (struct mwifiex_adapter *, u16);
	void (*cleanup_mpa_buf) (struct mwifiex_adapter *);
};

struct mwifiex_adapter {
	struct mwifiex_private *priv[MWIFIEX_MAX_BSS_NUM];
	u8 priv_num;
	struct mwifiex_drv_mode *drv_mode;
	const struct firmware *firmware;
	struct device *dev;
	bool surprise_removed;
	u32 fw_release_number;
	u32 revision_id;
	u16 init_wait_q_woken;
	wait_queue_head_t init_wait_q;
	void *card;
	struct mwifiex_if_ops if_ops;
	atomic_t rx_pending;
	atomic_t tx_pending;
	atomic_t ioctl_pending;
	struct workqueue_struct *workqueue;
	struct work_struct main_work;
	struct mwifiex_bss_prio_tbl bss_prio_tbl[MWIFIEX_MAX_BSS_NUM];
	/* spin lock for init/shutdown */
	spinlock_t mwifiex_lock;
	/* spin lock for main process */
	spinlock_t main_proc_lock;
	u32 mwifiex_processing;
	u16 max_tx_buf_size;
	u16 tx_buf_size;
	u16 curr_tx_buf_size;
	u32 ioport;
	enum MWIFIEX_HARDWARE_STATUS hw_status;
	u16 radio_on;
	u16 number_of_antenna;
	u32 fw_cap_info;
	/* spin lock for interrupt handling */
	spinlock_t int_lock;
	u8 int_status;
	u32 event_cause;
	struct sk_buff *event_skb;
	u8 upld_buf[MWIFIEX_UPLD_SIZE];
	u8 data_sent;
	u8 cmd_sent;
	u8 cmd_resp_received;
	u8 event_received;
	u8 data_received;
	u16 seq_num;
	struct cmd_ctrl_node *cmd_pool;
	struct cmd_ctrl_node *curr_cmd;
	/* spin lock for command */
	spinlock_t mwifiex_cmd_lock;
	u32 num_cmd_timeout;
	u16 last_init_cmd;
	struct timer_list cmd_timer;
	struct list_head cmd_free_q;
	/* spin lock for cmd_free_q */
	spinlock_t cmd_free_q_lock;
	struct list_head cmd_pending_q;
	/* spin lock for cmd_pending_q */
	spinlock_t cmd_pending_q_lock;
	struct list_head scan_pending_q;
	/* spin lock for scan_pending_q */
	spinlock_t scan_pending_q_lock;
	u32 scan_processing;
	u16 region_code;
	struct mwifiex_802_11d_domain_reg domain_reg;
	struct mwifiex_bssdescriptor *scan_table;
	u32 num_in_scan_table;
	u16 scan_probes;
	u32 scan_mode;
	u16 specific_scan_time;
	u16 active_scan_time;
	u16 passive_scan_time;
	u8 bcn_buf[MAX_SCAN_BEACON_BUFFER];
	u8 *bcn_buf_end;
	u8 fw_bands;
	u8 adhoc_start_band;
	u8 config_bands;
	struct mwifiex_chan_scan_param_set *scan_channels;
	u8 tx_lock_flag;
	struct mwifiex_sleep_params sleep_params;
	struct mwifiex_sleep_period sleep_period;
	u16 ps_mode;
	u32 ps_state;
	u8 need_to_wakeup;
	u16 multiple_dtim;
	u16 local_listen_interval;
	u16 null_pkt_interval;
	struct sk_buff *sleep_cfm;
	u16 bcn_miss_time_out;
	u16 adhoc_awake_period;
	u8 is_deep_sleep;
	u8 delay_null_pkt;
	u16 delay_to_ps;
	u16 enhanced_ps_mode;
	u8 pm_wakeup_card_req;
	u16 gen_null_pkt;
	u16 pps_uapsd_mode;
	u32 pm_wakeup_fw_try;
	u8 is_hs_configured;
	struct mwifiex_hs_config_param hs_cfg;
	u8 hs_activated;
	u16 hs_activate_wait_q_woken;
	wait_queue_head_t hs_activate_wait_q;
	bool is_suspended;
	u8 event_body[MAX_EVENT_SIZE];
	u32 hw_dot_11n_dev_cap;
	u8 hw_dev_mcs_support;
	u8 adhoc_11n_enabled;
	u8 chan_offset;
	struct mwifiex_dbg dbg;
	u8 arp_filter[ARP_FILTER_MAX_BUF_SIZE];
	u32 arp_filter_size;
};

int mwifiex_init_lock_list(struct mwifiex_adapter *adapter);
void mwifiex_free_lock_list(struct mwifiex_adapter *adapter);

int mwifiex_init_fw(struct mwifiex_adapter *adapter);

int mwifiex_init_fw_complete(struct mwifiex_adapter *adapter);

int mwifiex_shutdown_drv(struct mwifiex_adapter *adapter);

int mwifiex_shutdown_fw_complete(struct mwifiex_adapter *adapter);

int mwifiex_dnld_fw(struct mwifiex_adapter *, struct mwifiex_fw_image *);

int mwifiex_recv_complete(struct mwifiex_adapter *,
			  struct sk_buff *skb,
			  int status);

int mwifiex_recv_packet(struct mwifiex_adapter *, struct sk_buff *skb);

int mwifiex_process_event(struct mwifiex_adapter *adapter);

int mwifiex_ioctl_complete(struct mwifiex_adapter *adapter,
			   struct mwifiex_wait_queue *ioctl_wq,
			   int status);

int mwifiex_prepare_cmd(struct mwifiex_private *priv,
			uint16_t cmd_no,
			u16 cmd_action,
			u32 cmd_oid,
			void *wait_queue, void *data_buf);

void mwifiex_cmd_timeout_func(unsigned long function_context);

int mwifiex_misc_ioctl_init_shutdown(struct mwifiex_adapter *adapter,
				     struct mwifiex_wait_queue *wait_queue,
				     u32 func_init_shutdown);
int mwifiex_get_debug_info(struct mwifiex_private *,
			   struct mwifiex_debug_info *);

int mwifiex_alloc_cmd_buffer(struct mwifiex_adapter *adapter);
int mwifiex_free_cmd_buffer(struct mwifiex_adapter *adapter);
void mwifiex_cancel_all_pending_cmd(struct mwifiex_adapter *adapter);
void mwifiex_cancel_pending_ioctl(struct mwifiex_adapter *adapter,
				  struct mwifiex_wait_queue *ioctl_wq);

void mwifiex_insert_cmd_to_free_q(struct mwifiex_adapter *adapter,
				  struct cmd_ctrl_node *cmd_node);

void mwifiex_insert_cmd_to_pending_q(struct mwifiex_adapter *adapter,
				     struct cmd_ctrl_node *cmd_node,
				     u32 addtail);

int mwifiex_exec_next_cmd(struct mwifiex_adapter *adapter);
int mwifiex_process_cmdresp(struct mwifiex_adapter *adapter);
int mwifiex_handle_rx_packet(struct mwifiex_adapter *adapter,
			     struct sk_buff *skb);
int mwifiex_process_tx(struct mwifiex_private *priv, struct sk_buff *skb,
		       struct mwifiex_tx_param *tx_param);
int mwifiex_send_null_packet(struct mwifiex_private *priv, u8 flags);
int mwifiex_write_data_complete(struct mwifiex_adapter *adapter,
				struct sk_buff *skb, int status);
int mwifiex_recv_packet_complete(struct mwifiex_adapter *,
				 struct sk_buff *skb, int status);
void mwifiex_clean_txrx(struct mwifiex_private *priv);
u8 mwifiex_check_last_packet_indication(struct mwifiex_private *priv);
void mwifiex_check_ps_cond(struct mwifiex_adapter *adapter);
void mwifiex_process_sleep_confirm_resp(struct mwifiex_adapter *, u8 *,
					u32);
int mwifiex_cmd_enh_power_mode(struct mwifiex_private *priv,
			       struct host_cmd_ds_command *cmd,
			       u16 cmd_action, uint16_t ps_bitmap,
			       void *data_buf);
int mwifiex_ret_enh_power_mode(struct mwifiex_private *priv,
			       struct host_cmd_ds_command *resp,
			       void *data_buf);
void mwifiex_process_hs_config(struct mwifiex_adapter *adapter);
void mwifiex_hs_activated_event(struct mwifiex_private *priv,
					u8 activated);
int mwifiex_ret_802_11_hs_cfg(struct mwifiex_private *priv,
			      struct host_cmd_ds_command *resp);
int mwifiex_process_rx_packet(struct mwifiex_adapter *adapter,
			      struct sk_buff *skb);
int mwifiex_sta_prepare_cmd(struct mwifiex_private *, uint16_t cmd_no,
			    u16 cmd_action, u32 cmd_oid,
			    void *data_buf, void *cmd_buf);
int mwifiex_process_sta_cmdresp(struct mwifiex_private *, u16 cmdresp_no,
				void *cmd_buf, void *ioctl);
int mwifiex_process_sta_rx_packet(struct mwifiex_adapter *,
				  struct sk_buff *skb);
int mwifiex_process_sta_event(struct mwifiex_private *);
void *mwifiex_process_sta_txpd(struct mwifiex_private *, struct sk_buff *skb);
int mwifiex_sta_init_cmd(struct mwifiex_private *, u8 first_sta);
int mwifiex_scan_networks(struct mwifiex_private *priv, void *wait_queue,
			  u16 action,
			  const struct mwifiex_user_scan_cfg
			  *user_scan_in, struct mwifiex_scan_resp *);
int mwifiex_cmd_802_11_scan(struct mwifiex_private *priv,
			    struct host_cmd_ds_command *cmd,
			    void *data_buf);
void mwifiex_queue_scan_cmd(struct mwifiex_private *priv,
			    struct cmd_ctrl_node *cmd_node);
int mwifiex_ret_802_11_scan(struct mwifiex_private *priv,
			    struct host_cmd_ds_command *resp,
			    void *wait_queue);
s32 mwifiex_find_ssid_in_list(struct mwifiex_private *priv,
				struct mwifiex_802_11_ssid *ssid, u8 *bssid,
				u32 mode);
s32 mwifiex_find_bssid_in_list(struct mwifiex_private *priv, u8 *bssid,
				 u32 mode);
int mwifiex_find_best_network(struct mwifiex_private *priv,
			      struct mwifiex_ssid_bssid *req_ssid_bssid);
s32 mwifiex_ssid_cmp(struct mwifiex_802_11_ssid *ssid1,
		       struct mwifiex_802_11_ssid *ssid2);
int mwifiex_associate(struct mwifiex_private *priv, void *wait_queue,
		      struct mwifiex_bssdescriptor *bss_desc);
int mwifiex_cmd_802_11_associate(struct mwifiex_private *priv,
				 struct host_cmd_ds_command
				 *cmd, void *data_buf);
int mwifiex_ret_802_11_associate(struct mwifiex_private *priv,
				 struct host_cmd_ds_command *resp,
				 void *wait_queue);
void mwifiex_reset_connect_state(struct mwifiex_private *priv);
void mwifiex_2040_coex_event(struct mwifiex_private *priv);
u8 mwifiex_band_to_radio_type(u8 band);
int mwifiex_deauthenticate(struct mwifiex_private *priv,
			   struct mwifiex_wait_queue *wait_queue,
			   u8 *mac);
int mwifiex_adhoc_start(struct mwifiex_private *priv, void *wait_queue,
			struct mwifiex_802_11_ssid *adhoc_ssid);
int mwifiex_adhoc_join(struct mwifiex_private *priv, void *wait_queue,
		       struct mwifiex_bssdescriptor *bss_desc);
int mwifiex_cmd_802_11_ad_hoc_start(struct mwifiex_private *priv,
				    struct host_cmd_ds_command *cmd,
				    void *data_buf);
int mwifiex_cmd_802_11_ad_hoc_join(struct mwifiex_private *priv,
				   struct host_cmd_ds_command *cmd,
				   void *data_buf);
int mwifiex_ret_802_11_ad_hoc(struct mwifiex_private *priv,
			      struct host_cmd_ds_command *resp,
			      void *wait_queue);
int mwifiex_cmd_802_11_bg_scan_query(struct mwifiex_private *priv,
				     struct host_cmd_ds_command *cmd,
				     void *data_buf);
struct mwifiex_chan_freq_power *
			mwifiex_get_cfp_by_band_and_channel_from_cfg80211(
						struct mwifiex_private *priv,
						u8 band, u16 channel);
struct mwifiex_chan_freq_power *mwifiex_get_cfp_by_band_and_freq_from_cfg80211(
						struct mwifiex_private *priv,
						u8 band, u32 freq);
u32 mwifiex_index_to_data_rate(struct mwifiex_adapter *adapter, u8 index,
				 u8 ht_info);
u32 mwifiex_find_freq_from_band_chan(u8, u8);
int mwifiex_cmd_append_vsie_tlv(struct mwifiex_private *priv, u16 vsie_mask,
				u8 **buffer);
u32 mwifiex_index_to_data_rate(struct mwifiex_adapter *adapter, u8 index,
				 u8 ht_info);
u32 mwifiex_get_active_data_rates(struct mwifiex_private *priv,
				    u8 *rates);
u32 mwifiex_get_supported_rates(struct mwifiex_private *priv, u8 *rates);
u8 mwifiex_data_rate_to_index(struct mwifiex_adapter *adapter, u32 rate);
u8 mwifiex_is_rate_auto(struct mwifiex_private *priv);
int mwifiex_get_rate_index(struct mwifiex_adapter *adapter,
			   u16 *rateBitmap, int size);
extern u16 region_code_index[MWIFIEX_MAX_REGION_CODE];
void mwifiex_save_curr_bcn(struct mwifiex_private *priv);
void mwifiex_free_curr_bcn(struct mwifiex_private *priv);
int mwifiex_cmd_get_hw_spec(struct mwifiex_private *priv,
			    struct host_cmd_ds_command *cmd);
int mwifiex_ret_get_hw_spec(struct mwifiex_private *priv,
			    struct host_cmd_ds_command *resp);
int is_command_pending(struct mwifiex_adapter *adapter);

/*
 * This function checks if the queuing is RA based or not.
 */
static inline u8
mwifiex_queuing_ra_based(struct mwifiex_private *priv)
{
	/*
	 * Currently we assume if we are in Infra, then DA=RA. This might not be
	 * true in the future
	 */
	if ((priv->bss_mode == NL80211_IFTYPE_STATION) &&
	    (GET_BSS_ROLE(priv) == MWIFIEX_BSS_ROLE_STA))
		return false;

	return true;
}

/*
 * This function copies rates.
 */
static inline u32
mwifiex_copy_rates(u8 *dest, u32 pos, u8 *src, int len)
{
	int i;

	for (i = 0; i < len && src[i]; i++, pos++) {
		if (pos >= MWIFIEX_SUPPORTED_RATES)
			break;
		dest[pos] = src[i];
	}

	return pos;
}

/*
 * This function returns the correct private structure pointer based
 * upon the BSS type and BSS number.
 */
static inline struct mwifiex_private *
mwifiex_get_priv_by_id(struct mwifiex_adapter *adapter,
		       u32 bss_num, u32 bss_type)
{
	int i;

	for (i = 0; i < adapter->priv_num; i++) {
		if (adapter->priv[i]) {
			if ((adapter->priv[i]->bss_num == bss_num)
			    && (adapter->priv[i]->bss_type == bss_type))
				break;
		}
	}
	return ((i < adapter->priv_num) ? adapter->priv[i] : NULL);
}

/*
 * This function returns the first available private structure pointer
 * based upon the BSS role.
 */
static inline struct mwifiex_private *
mwifiex_get_priv(struct mwifiex_adapter *adapter,
		 enum mwifiex_bss_role bss_role)
{
	int i;

	for (i = 0; i < adapter->priv_num; i++) {
		if (adapter->priv[i]) {
			if (bss_role == MWIFIEX_BSS_ROLE_ANY ||
			    GET_BSS_ROLE(adapter->priv[i]) == bss_role)
				break;
		}
	}

	return ((i < adapter->priv_num) ? adapter->priv[i] : NULL);
}

/*
 * This function returns the driver private structure of a network device.
 */
static inline struct mwifiex_private *
mwifiex_netdev_get_priv(struct net_device *dev)
{
	return (struct mwifiex_private *) (*(unsigned long *) netdev_priv(dev));
}

struct mwifiex_wait_queue *mwifiex_alloc_fill_wait_queue(
				struct mwifiex_private *,
				u8 wait_option);
struct mwifiex_private *mwifiex_bss_index_to_priv(struct mwifiex_adapter
						*adapter, u8 bss_index);
int mwifiex_shutdown_fw(struct mwifiex_private *, u8);

int mwifiex_add_card(void *, struct semaphore *, struct mwifiex_if_ops *);
int mwifiex_remove_card(struct mwifiex_adapter *, struct semaphore *);

void mwifiex_get_version(struct mwifiex_adapter *adapter, char *version,
			 int maxlen);
int mwifiex_request_set_mac_address(struct mwifiex_private *priv);
void mwifiex_request_set_multicast_list(struct mwifiex_private *priv,
					struct net_device *dev);
int mwifiex_request_ioctl(struct mwifiex_private *priv,
			  struct mwifiex_wait_queue *req,
			  int, u8 wait_option);
int mwifiex_disconnect(struct mwifiex_private *, u8, u8 *);
int mwifiex_bss_start(struct mwifiex_private *priv,
		      u8 wait_option,
		      struct mwifiex_ssid_bssid *ssid_bssid);
int mwifiex_set_hs_params(struct mwifiex_private *priv,
			      u16 action, u8 wait_option,
			      struct mwifiex_ds_hs_cfg *hscfg);
int mwifiex_cancel_hs(struct mwifiex_private *priv, u8 wait_option);
int mwifiex_enable_hs(struct mwifiex_adapter *adapter);
void mwifiex_process_ioctl_resp(struct mwifiex_private *priv,
				struct mwifiex_wait_queue *req);
u32 mwifiex_get_mode(struct mwifiex_private *priv, u8 wait_option);
int mwifiex_get_signal_info(struct mwifiex_private *priv,
			    u8 wait_option,
			    struct mwifiex_ds_get_signal *signal);
int mwifiex_drv_get_data_rate(struct mwifiex_private *priv,
			      struct mwifiex_rate_cfg *rate);
int mwifiex_get_channel_list(struct mwifiex_private *priv,
			     u8 wait_option,
			     struct mwifiex_chan_list *chanlist);
int mwifiex_get_scan_table(struct mwifiex_private *priv,
			   u8 wait_option,
			   struct mwifiex_scan_resp *scanresp);
int mwifiex_enable_wep_key(struct mwifiex_private *priv, u8 wait_option);
int mwifiex_find_best_bss(struct mwifiex_private *priv, u8 wait_option,
			  struct mwifiex_ssid_bssid *ssid_bssid);
int mwifiex_request_scan(struct mwifiex_private *priv,
			 u8 wait_option,
			 struct mwifiex_802_11_ssid *req_ssid);
int mwifiex_set_user_scan_ioctl(struct mwifiex_private *priv,
				struct mwifiex_user_scan_cfg *scan_req);
int mwifiex_change_adhoc_chan(struct mwifiex_private *priv, int channel);
int mwifiex_set_radio(struct mwifiex_private *priv, u8 option);

int mwifiex_drv_change_adhoc_chan(struct mwifiex_private *priv, int channel);

int mwifiex_set_encode(struct mwifiex_private *priv, const u8 *key,
		       int key_len, u8 key_index, int disable);

int mwifiex_set_gen_ie(struct mwifiex_private *priv, u8 *ie, int ie_len);

int mwifiex_get_ver_ext(struct mwifiex_private *priv);

int mwifiex_get_stats_info(struct mwifiex_private *priv,
			   struct mwifiex_ds_get_stats *log);

int mwifiex_reg_write(struct mwifiex_private *priv, u32 reg_type,
		      u32 reg_offset, u32 reg_value);

int mwifiex_reg_read(struct mwifiex_private *priv, u32 reg_type,
		     u32 reg_offset, u32 *value);

int mwifiex_eeprom_read(struct mwifiex_private *priv, u16 offset, u16 bytes,
			u8 *value);

int mwifiex_set_11n_httx_cfg(struct mwifiex_private *priv, int data);

int mwifiex_get_11n_httx_cfg(struct mwifiex_private *priv, int *data);

int mwifiex_set_tx_rate_cfg(struct mwifiex_private *priv, int tx_rate_index);

int mwifiex_get_tx_rate_cfg(struct mwifiex_private *priv, int *tx_rate_index);

int mwifiex_drv_set_power(struct mwifiex_private *priv, bool power_on);

int mwifiex_drv_get_driver_version(struct mwifiex_adapter *adapter,
				   char *version, int max_len);

int mwifiex_set_tx_power(struct mwifiex_private *priv, int type, int dbm);

int mwifiex_main_process(struct mwifiex_adapter *);

int mwifiex_bss_ioctl_channel(struct mwifiex_private *,
			      u16 action,
			      struct mwifiex_chan_freq_power *cfp);
int mwifiex_bss_ioctl_find_bss(struct mwifiex_private *,
			       struct mwifiex_wait_queue *,
			       struct mwifiex_ssid_bssid *);
int mwifiex_radio_ioctl_band_cfg(struct mwifiex_private *,
				 u16 action,
				 struct mwifiex_ds_band_cfg *);
int mwifiex_snmp_mib_ioctl(struct mwifiex_private *,
			   struct mwifiex_wait_queue *,
			   u32 cmd_oid, u16 action, u32 *value);
int mwifiex_get_bss_info(struct mwifiex_private *,
			 struct mwifiex_bss_info *);

#ifdef CONFIG_DEBUG_FS
void mwifiex_debugfs_init(void);
void mwifiex_debugfs_remove(void);

void mwifiex_dev_debugfs_init(struct mwifiex_private *priv);
void mwifiex_dev_debugfs_remove(struct mwifiex_private *priv);
#endif
#endif /* !_MWIFIEX_MAIN_H_ */
