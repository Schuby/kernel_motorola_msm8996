/***********************************************************************
   sme_Trace.h

  \brief definition for trace related APIs

  \author Kiran Kumar Reddy CH L V

   Copyright (c) 2013 Qualcomm Atheros, Inc.

   All Rights Reserved.

   Qualcomm Atheros Confidential and Proprietary.

  ========================================================================*/

#ifndef __SME_TRACE_H__
#define __SME_TRACE_H__


#include "macTrace.h"

#define NO_SESSION 0xFF
#define TRACE_CODE_SME_COMMAND 0xFF
enum {
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_REQ,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_GET_RESULTS,
    TRACE_CODE_SME_RX_HDD_MSG_CONNECT,
    TRACE_CODE_SME_RX_HDD_MSG_SET_11DINFO,
    TRACE_CODE_SME_RX_HDD_MSG_GET_SOFTAP_DOMAIN,
    TRACE_CODE_SME_RX_HDD_MSG_SET_REGINFO,
    TRACE_CODE_SME_RX_HDD_MSG_UPDATE_CHANNEL_CONFIG,
    TRACE_CODE_SME_RX_HDD_MSG_UPDATE_CONFIG,
    TRACE_CODE_SME_RX_HDD_MSG_HDDREADYIND,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_FLUSH_RESULTS,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_FLUSH_P2PRESULTS,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_GETFIRST,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_GETNEXT,
    TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_PURGE,
    TRACE_CODE_SME_RX_HDD_ROAM_REASSOC,
    TRACE_CODE_SME_RX_HDD_ROAM_DISCONNECT,
    TRACE_CODE_SME_RX_HDD_ROAM_GET_CONNECTPROFILE,
    TRACE_CODE_SME_RX_HDD_ROAM_FREE_CONNECTPROFILE,
    TRACE_CODE_SME_RX_HDD_ROAM_SET_PMKIDCACHE,
    TRACE_CODE_SME_RX_HDD_GET_CONFIGPARAM,
    TRACE_CODE_SME_RX_HDD_GET_MODPROFFIELDS,
    TRACE_CODE_SME_RX_HDD_SET_CONFIG_PWRSAVE,
    TRACE_CODE_SME_RX_HDD_GET_CONFIG_PWRSAVE,
    TRACE_CODE_SME_RX_HDD_ENABLE_PWRSAVE,
    TRACE_CODE_SME_RX_HDD_DISABLE_PWRSAVE,
    TRACE_CODE_SME_RX_HDD_START_AUTO_BMPSTIMER,
    TRACE_CODE_SME_RX_HDD_STOP_AUTO_BMPSTIMER,
    TRACE_CODE_SME_RX_HDD_IS_PWRSAVE_ENABLED,
    TRACE_CODE_SME_RX_HDD_REQUEST_FULLPOWER,
    TRACE_CODE_SME_RX_HDD_REQUEST_BMPS,
    TRACE_CODE_SME_RX_HDD_SET_DHCP_FLAG,
    TRACE_CODE_SME_RX_HDD_REQUEST_STANDBY,
    TRACE_CODE_SME_RX_HDD_WOWL_ADDBCAST_PATTERN,
    TRACE_CODE_SME_RX_HDD_WOWL_DELBCAST_PATTERN,
    TRACE_CODE_SME_RX_HDD_ENTER_WOWL,
    TRACE_CODE_SME_RX_HDD_EXIT_WOWL,
    TRACE_CODE_SME_RX_HDD_SET_KEY,
    TRACE_CODE_SME_RX_HDD_REMOVE_KEY,
    TRACE_CODE_SME_RX_HDD_GET_STATS,
    TRACE_CODE_SME_RX_HDD_GET_CNTRYCODE,
    TRACE_CODE_SME_RX_HDD_SET_CNTRYCODE,
    TRACE_CODE_SME_RX_HDD_CHANGE_CNTRYCODE,
    TRACE_CODE_SME_RX_HDD_BTC_SIGNALEVENT,
    TRACE_CODE_SME_RX_HDD_BTC_SETCONFIG,
    TRACE_CODE_SME_RX_HDD_BTC_GETCONFIG,
    TRACE_CODE_SME_RX_HDD_SET_CFGPRIVACY,
    TRACE_CODE_SME_RX_HDD_NEIGHBOR_REPORTREQ,
    TRACE_CODE_SME_RX_HDD_DBG_READREG,
    TRACE_CODE_SME_RX_HDD_DBG_WRITEREG,
    TRACE_CODE_SME_RX_HDD_DBG_READMEM,
    TRACE_CODE_SME_RX_HDD_DBG_WRITEMEM,
    TRACE_CODE_SME_RX_HDD_OPEN_SESSION,
    TRACE_CODE_SME_RX_HDD_CLOSE_SESSION,
    TRACE_CODE_SME_RX_HDD_SET_HOSTOFFLOAD,
    TRACE_CODE_SME_RX_HDD_SET_GTKOFFLOAD,
    TRACE_CODE_SME_RX_HDD_GET_GTKOFFLOAD,
    TRACE_CODE_SME_RX_HDD_SET_POWERPARAMS,
    TRACE_CODE_SME_RX_HDD_ABORT_MACSCAN,
    TRACE_CODE_SME_RX_HDD_REGISTER_MGMTFR,
    TRACE_CODE_SME_RX_HDD_DEREGISTER_MGMTFR,
    TRACE_CODE_SME_RX_HDD_REMAIN_ONCHAN,
    TRACE_CODE_SME_RX_HDD_SEND_ACTION,
    TRACE_CODE_SME_RX_HDD_CANCEL_REMAIN_ONCHAN,
    TRACE_CODE_SME_RX_HDD_CONFIG_RXPFIL,
    TRACE_CODE_SME_RX_HDD_CONFIG_SUSPENDIND,
    TRACE_CODE_SME_RX_HDD_CONFIG_RESUMEREQ,
    TRACE_CODE_SME_RX_HDD_SET_MAXTXPOW,
    TRACE_CODE_SME_RX_HDD_SET_TXPOW,
    TRACE_CODE_SME_RX_HDD_SET_TMLEVEL,
    TRACE_CODE_SME_RX_HDD_CAPS_EXCH,
    TRACE_CODE_SME_RX_HDD_DISABLE_CAP,
    TRACE_CODE_SME_RX_HDD_GET_DEFCCNV,
    TRACE_CODE_SME_RX_HDD_GET_CURCC,
    TRACE_CODE_SME_RX_HDD_RESET_PW5G,
    TRACE_CODE_SME_RX_HDD_UPDATE_RP5G,
    TRACE_CODE_SME_RX_HDD_SET_ROAMIBAND,
    TRACE_CODE_SME_RX_HDD_GET_ROAMIBAND,
    TRACE_CODE_SME_RX_HDD_UPDATE_RSSIDIFF,
    TRACE_CODE_SME_RX_HDD_UPDATE_IMMRSSIDIFF,
    TRACE_CODE_SME_RX_HDD_UPDATE_FTENABLED,
    TRACE_CODE_SME_RX_HDD_UPDATE_WESMODE,
    TRACE_CODE_SME_RX_HDD_SET_SCANCTRL,
};

void smeTraceInit(tpAniSirGlobal pMac);
#endif //__SME_TRACE_H__
