/*
 * Remote Network Driver Interface Specification (RNDIS)
 * definitions of the magic numbers used by this protocol
 */

#define RNDIS_MSG_COMPLETION	0x80000000

/* codes for "msg_type" field of rndis messages;
 * only the data channel uses packet messages (maybe batched);
 * everything else goes on the control channel.
 */
#define RNDIS_MSG_PACKET	0x00000001	/* 1-N packets */
#define RNDIS_MSG_INIT		0x00000002
#define RNDIS_MSG_INIT_C	(RNDIS_MSG_INIT|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_HALT		0x00000003
#define RNDIS_MSG_QUERY		0x00000004
#define RNDIS_MSG_QUERY_C	(RNDIS_MSG_QUERY|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_SET		0x00000005
#define RNDIS_MSG_SET_C		(RNDIS_MSG_SET|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_RESET		0x00000006
#define RNDIS_MSG_RESET_C	(RNDIS_MSG_RESET|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_INDICATE	0x00000007
#define RNDIS_MSG_KEEPALIVE	0x00000008
#define RNDIS_MSG_KEEPALIVE_C	(RNDIS_MSG_KEEPALIVE|RNDIS_MSG_COMPLETION)

/* codes for "status" field of completion messages */
#define	RNDIS_STATUS_SUCCESS			0x00000000
#define RNDIS_STATUS_PENDING			0x00000103

/*  Status codes */
#define RNDIS_STATUS_NOT_RECOGNIZED		0x00010001
#define RNDIS_STATUS_NOT_COPIED			0x00010002
#define RNDIS_STATUS_NOT_ACCEPTED		0x00010003
#define RNDIS_STATUS_CALL_ACTIVE		0x00010007

#define RNDIS_STATUS_ONLINE			0x40010003
#define RNDIS_STATUS_RESET_START		0x40010004
#define RNDIS_STATUS_RESET_END			0x40010005
#define RNDIS_STATUS_RING_STATUS		0x40010006
#define RNDIS_STATUS_CLOSED			0x40010007
#define RNDIS_STATUS_WAN_LINE_UP		0x40010008
#define RNDIS_STATUS_WAN_LINE_DOWN		0x40010009
#define RNDIS_STATUS_WAN_FRAGMENT		0x4001000A
#define	RNDIS_STATUS_MEDIA_CONNECT		0x4001000B
#define	RNDIS_STATUS_MEDIA_DISCONNECT		0x4001000C
#define RNDIS_STATUS_HARDWARE_LINE_UP		0x4001000D
#define RNDIS_STATUS_HARDWARE_LINE_DOWN		0x4001000E
#define RNDIS_STATUS_INTERFACE_UP		0x4001000F
#define RNDIS_STATUS_INTERFACE_DOWN		0x40010010
#define RNDIS_STATUS_MEDIA_BUSY			0x40010011
#define	RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION	0x40010012
#define RNDIS_STATUS_WW_INDICATION		RDIA_SPECIFIC_INDICATION
#define RNDIS_STATUS_LINK_SPEED_CHANGE		0x40010013L

#define RNDIS_STATUS_NOT_RESETTABLE		0x80010001
#define RNDIS_STATUS_SOFT_ERRORS		0x80010003
#define RNDIS_STATUS_HARD_ERRORS		0x80010004
#define RNDIS_STATUS_BUFFER_OVERFLOW		0x80000005

#define	RNDIS_STATUS_FAILURE			0xC0000001
#define RNDIS_STATUS_RESOURCES			0xC000009A
#define	RNDIS_STATUS_NOT_SUPPORTED		0xc00000BB
#define RNDIS_STATUS_CLOSING			0xC0010002
#define RNDIS_STATUS_BAD_VERSION		0xC0010004
#define RNDIS_STATUS_BAD_CHARACTERISTICS	0xC0010005
#define RNDIS_STATUS_ADAPTER_NOT_FOUND		0xC0010006
#define RNDIS_STATUS_OPEN_FAILED		0xC0010007
#define RNDIS_STATUS_DEVICE_FAILED		0xC0010008
#define RNDIS_STATUS_MULTICAST_FULL		0xC0010009
#define RNDIS_STATUS_MULTICAST_EXISTS		0xC001000A
#define RNDIS_STATUS_MULTICAST_NOT_FOUND	0xC001000B
#define RNDIS_STATUS_REQUEST_ABORTED		0xC001000C
#define RNDIS_STATUS_RESET_IN_PROGRESS		0xC001000D
#define RNDIS_STATUS_CLOSING_INDICATING		0xC001000E
#define RNDIS_STATUS_INVALID_PACKET		0xC001000F
#define RNDIS_STATUS_OPEN_LIST_FULL		0xC0010010
#define RNDIS_STATUS_ADAPTER_NOT_READY		0xC0010011
#define RNDIS_STATUS_ADAPTER_NOT_OPEN		0xC0010012
#define RNDIS_STATUS_NOT_INDICATING		0xC0010013
#define RNDIS_STATUS_INVALID_LENGTH		0xC0010014
#define	RNDIS_STATUS_INVALID_DATA		0xC0010015
#define RNDIS_STATUS_BUFFER_TOO_SHORT		0xC0010016
#define RNDIS_STATUS_INVALID_OID		0xC0010017
#define RNDIS_STATUS_ADAPTER_REMOVED		0xC0010018
#define RNDIS_STATUS_UNSUPPORTED_MEDIA		0xC0010019
#define RNDIS_STATUS_GROUP_ADDRESS_IN_USE	0xC001001A
#define RNDIS_STATUS_FILE_NOT_FOUND		0xC001001B
#define RNDIS_STATUS_ERROR_READING_FILE		0xC001001C
#define RNDIS_STATUS_ALREADY_MAPPED		0xC001001D
#define RNDIS_STATUS_RESOURCE_CONFLICT		0xC001001E
#define RNDIS_STATUS_NO_CABLE			0xC001001F

#define RNDIS_STATUS_INVALID_SAP		0xC0010020
#define RNDIS_STATUS_SAP_IN_USE			0xC0010021
#define RNDIS_STATUS_INVALID_ADDRESS		0xC0010022
#define RNDIS_STATUS_VC_NOT_ACTIVATED		0xC0010023
#define RNDIS_STATUS_DEST_OUT_OF_ORDER		0xC0010024
#define RNDIS_STATUS_VC_NOT_AVAILABLE		0xC0010025
#define RNDIS_STATUS_CELLRATE_NOT_AVAILABLE	0xC0010026
#define RNDIS_STATUS_INCOMPATABLE_QOS		0xC0010027
#define RNDIS_STATUS_AAL_PARAMS_UNSUPPORTED	0xC0010028
#define RNDIS_STATUS_NO_ROUTE_TO_DESTINATION	0xC0010029

#define RNDIS_STATUS_TOKEN_RING_OPEN_ERROR	0xC0011000

/* codes for RNDIS_OID_GEN_PHYSICAL_MEDIUM */
#define	RNDIS_PHYSICAL_MEDIUM_UNSPECIFIED	0x00000000
#define	RNDIS_PHYSICAL_MEDIUM_WIRELESS_LAN	0x00000001
#define	RNDIS_PHYSICAL_MEDIUM_CABLE_MODEM	0x00000002
#define	RNDIS_PHYSICAL_MEDIUM_PHONE_LINE	0x00000003
#define	RNDIS_PHYSICAL_MEDIUM_POWER_LINE	0x00000004
#define	RNDIS_PHYSICAL_MEDIUM_DSL		0x00000005
#define	RNDIS_PHYSICAL_MEDIUM_FIBRE_CHANNEL	0x00000006
#define	RNDIS_PHYSICAL_MEDIUM_1394		0x00000007
#define	RNDIS_PHYSICAL_MEDIUM_WIRELESS_WAN	0x00000008
#define	RNDIS_PHYSICAL_MEDIUM_MAX		0x00000009

/*  Remote NDIS medium types. */
#define RNDIS_MEDIUM_UNSPECIFIED		0x00000000
#define RNDIS_MEDIUM_802_3			0x00000000
#define RNDIS_MEDIUM_802_5			0x00000001
#define RNDIS_MEDIUM_FDDI			0x00000002
#define RNDIS_MEDIUM_WAN			0x00000003
#define RNDIS_MEDIUM_LOCAL_TALK			0x00000004
#define RNDIS_MEDIUM_ARCNET_RAW			0x00000006
#define RNDIS_MEDIUM_ARCNET_878_2		0x00000007
#define RNDIS_MEDIUM_ATM			0x00000008
#define RNDIS_MEDIUM_WIRELESS_LAN		0x00000009
#define RNDIS_MEDIUM_IRDA			0x0000000A
#define RNDIS_MEDIUM_BPC			0x0000000B
#define RNDIS_MEDIUM_CO_WAN			0x0000000C
#define RNDIS_MEDIUM_1394			0x0000000D
/* Not a real medium, defined as an upper-bound */
#define RNDIS_MEDIUM_MAX			0x0000000E

/* Remote NDIS medium connection states. */
#define RNDIS_MEDIA_STATE_CONNECTED		0x00000000
#define RNDIS_MEDIA_STATE_DISCONNECTED		0x00000001

/* packet filter bits used by RNDIS_OID_GEN_CURRENT_PACKET_FILTER */
#define RNDIS_PACKET_TYPE_DIRECTED		0x00000001
#define RNDIS_PACKET_TYPE_MULTICAST		0x00000002
#define RNDIS_PACKET_TYPE_ALL_MULTICAST		0x00000004
#define RNDIS_PACKET_TYPE_BROADCAST		0x00000008
#define RNDIS_PACKET_TYPE_SOURCE_ROUTING	0x00000010
#define RNDIS_PACKET_TYPE_PROMISCUOUS		0x00000020
#define RNDIS_PACKET_TYPE_SMT			0x00000040
#define RNDIS_PACKET_TYPE_ALL_LOCAL		0x00000080
#define RNDIS_PACKET_TYPE_GROUP			0x00001000
#define RNDIS_PACKET_TYPE_ALL_FUNCTIONAL	0x00002000
#define RNDIS_PACKET_TYPE_FUNCTIONAL		0x00004000
#define RNDIS_PACKET_TYPE_MAC_FRAME		0x00008000

/* NDIS_PNP_CAPABILITIES.Flags constants */
#define NDIS_DEVICE_WAKE_UP_ENABLE                0x00000001
#define NDIS_DEVICE_WAKE_ON_PATTERN_MATCH_ENABLE  0x00000002
#define NDIS_DEVICE_WAKE_ON_MAGIC_PACKET_ENABLE   0x00000004

/* IEEE 802.3 (Ethernet) OIDs */
#define NDIS_802_3_MAC_OPTION_PRIORITY    0x00000001

/* RNDIS_OID_GEN_MINIPORT_INFO constants */
#define NDIS_MINIPORT_BUS_MASTER                      0x00000001
#define NDIS_MINIPORT_WDM_DRIVER                      0x00000002
#define NDIS_MINIPORT_SG_LIST                         0x00000004
#define NDIS_MINIPORT_SUPPORTS_MEDIA_QUERY            0x00000008
#define NDIS_MINIPORT_INDICATES_PACKETS               0x00000010
#define NDIS_MINIPORT_IGNORE_PACKET_QUEUE             0x00000020
#define NDIS_MINIPORT_IGNORE_REQUEST_QUEUE            0x00000040
#define NDIS_MINIPORT_IGNORE_TOKEN_RING_ERRORS        0x00000080
#define NDIS_MINIPORT_INTERMEDIATE_DRIVER             0x00000100
#define NDIS_MINIPORT_IS_NDIS_5                       0x00000200
#define NDIS_MINIPORT_IS_CO                           0x00000400
#define NDIS_MINIPORT_DESERIALIZE                     0x00000800
#define NDIS_MINIPORT_REQUIRES_MEDIA_POLLING          0x00001000
#define NDIS_MINIPORT_SUPPORTS_MEDIA_SENSE            0x00002000
#define NDIS_MINIPORT_NETBOOT_CARD                    0x00004000
#define NDIS_MINIPORT_PM_SUPPORTED                    0x00008000
#define NDIS_MINIPORT_SUPPORTS_MAC_ADDRESS_OVERWRITE  0x00010000
#define NDIS_MINIPORT_USES_SAFE_BUFFER_APIS           0x00020000
#define NDIS_MINIPORT_HIDDEN                          0x00040000
#define NDIS_MINIPORT_SWENUM                          0x00080000
#define NDIS_MINIPORT_SURPRISE_REMOVE_OK              0x00100000
#define NDIS_MINIPORT_NO_HALT_ON_SUSPEND              0x00200000
#define NDIS_MINIPORT_HARDWARE_DEVICE                 0x00400000
#define NDIS_MINIPORT_SUPPORTS_CANCEL_SEND_PACKETS    0x00800000
#define NDIS_MINIPORT_64BITS_DMA                      0x01000000

#define NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA     0x00000001
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED      0x00000002
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND      0x00000004
#define NDIS_MAC_OPTION_NO_LOOPBACK             0x00000008
#define NDIS_MAC_OPTION_FULL_DUPLEX             0x00000010
#define NDIS_MAC_OPTION_EOTX_INDICATION         0x00000020
#define NDIS_MAC_OPTION_8021P_PRIORITY          0x00000040
#define NDIS_MAC_OPTION_RESERVED                0x80000000

/* Remote NDIS Versions */
#define RNDIS_MAJOR_VERSION		0x00000001
#define RNDIS_MINOR_VERSION		0x00000000

/* Device Flags */
#define RNDIS_DF_CONNECTIONLESS		0x00000001U
#define RNDIS_DF_CONNECTION_ORIENTED	0x00000002U
#define RNDIS_DF_RAW_DATA		0x00000004U

/* Object Identifiers used by NdisRequest Query/Set Information */
/* General (Required) Objects */
#define RNDIS_OID_GEN_SUPPORTED_LIST		0x00010101
#define RNDIS_OID_GEN_HARDWARE_STATUS		0x00010102
#define RNDIS_OID_GEN_MEDIA_SUPPORTED		0x00010103
#define RNDIS_OID_GEN_MEDIA_IN_USE		0x00010104
#define RNDIS_OID_GEN_MAXIMUM_LOOKAHEAD		0x00010105
#define RNDIS_OID_GEN_MAXIMUM_FRAME_SIZE	0x00010106
#define RNDIS_OID_GEN_LINK_SPEED		0x00010107
#define RNDIS_OID_GEN_TRANSMIT_BUFFER_SPACE	0x00010108
#define RNDIS_OID_GEN_RECEIVE_BUFFER_SPACE	0x00010109
#define RNDIS_OID_GEN_TRANSMIT_BLOCK_SIZE	0x0001010A
#define RNDIS_OID_GEN_RECEIVE_BLOCK_SIZE	0x0001010B
#define RNDIS_OID_GEN_VENDOR_ID			0x0001010C
#define RNDIS_OID_GEN_VENDOR_DESCRIPTION	0x0001010D
#define RNDIS_OID_GEN_CURRENT_PACKET_FILTER	0x0001010E
#define RNDIS_OID_GEN_CURRENT_LOOKAHEAD		0x0001010F
#define RNDIS_OID_GEN_DRIVER_VERSION		0x00010110
#define RNDIS_OID_GEN_MAXIMUM_TOTAL_SIZE	0x00010111
#define RNDIS_OID_GEN_PROTOCOL_OPTIONS		0x00010112
#define RNDIS_OID_GEN_MAC_OPTIONS		0x00010113
#define RNDIS_OID_GEN_MEDIA_CONNECT_STATUS	0x00010114
#define RNDIS_OID_GEN_MAXIMUM_SEND_PACKETS	0x00010115
#define RNDIS_OID_GEN_VENDOR_DRIVER_VERSION	0x00010116
#define RNDIS_OID_GEN_SUPPORTED_GUIDS		0x00010117
#define RNDIS_OID_GEN_NETWORK_LAYER_ADDRESSES	0x00010118
#define RNDIS_OID_GEN_TRANSPORT_HEADER_OFFSET	0x00010119
#define RNDIS_OID_GEN_PHYSICAL_MEDIUM		0x00010202
#define RNDIS_OID_GEN_MACHINE_NAME		0x0001021A
#define RNDIS_OID_GEN_RNDIS_CONFIG_PARAMETER	0x0001021B
#define RNDIS_OID_GEN_VLAN_ID			0x0001021C

/* Optional OIDs */
#define OID_GEN_MEDIA_CAPABILITIES		0x00010201

/* Required statistics OIDs */
#define RNDIS_OID_GEN_XMIT_OK			0x00020101
#define RNDIS_OID_GEN_RCV_OK			0x00020102
#define RNDIS_OID_GEN_XMIT_ERROR		0x00020103
#define RNDIS_OID_GEN_RCV_ERROR			0x00020104
#define RNDIS_OID_GEN_RCV_NO_BUFFER		0x00020105

/* Optional statistics OIDs */
#define RNDIS_OID_GEN_DIRECTED_BYTES_XMIT	0x00020201
#define RNDIS_OID_GEN_DIRECTED_FRAMES_XMIT	0x00020202
#define RNDIS_OID_GEN_MULTICAST_BYTES_XMIT	0x00020203
#define RNDIS_OID_GEN_MULTICAST_FRAMES_XMIT	0x00020204
#define RNDIS_OID_GEN_BROADCAST_BYTES_XMIT	0x00020205
#define RNDIS_OID_GEN_BROADCAST_FRAMES_XMIT	0x00020206
#define RNDIS_OID_GEN_DIRECTED_BYTES_RCV	0x00020207
#define RNDIS_OID_GEN_DIRECTED_FRAMES_RCV	0x00020208
#define RNDIS_OID_GEN_MULTICAST_BYTES_RCV	0x00020209
#define RNDIS_OID_GEN_MULTICAST_FRAMES_RCV	0x0002020A
#define RNDIS_OID_GEN_BROADCAST_BYTES_RCV	0x0002020B
#define RNDIS_OID_GEN_BROADCAST_FRAMES_RCV	0x0002020C

#define RNDIS_OID_GEN_RCV_CRC_ERROR		0x0002020D
#define RNDIS_OID_GEN_TRANSMIT_QUEUE_LENGTH	0x0002020E

#define RNDIS_OID_GEN_GET_TIME_CAPS		0x0002020F
#define RNDIS_OID_GEN_GET_NETCARD_TIME		0x00020210

#define RNDIS_OID_GEN_NETCARD_LOAD		0x00020211
#define RNDIS_OID_GEN_DEVICE_PROFILE		0x00020212
#define RNDIS_OID_GEN_INIT_TIME_MS		0x00020213
#define RNDIS_OID_GEN_RESET_COUNTS		0x00020214
#define RNDIS_OID_GEN_MEDIA_SENSE_COUNTS	0x00020215
#define RNDIS_OID_GEN_FRIENDLY_NAME		0x00020216
#define RNDIS_OID_GEN_MINIPORT_INFO		0x00020217
#define RNDIS_OID_GEN_RESET_VERIFY_PARAMETERS	0x00020218

/* These are connection-oriented general OIDs. */
/* These replace the above OIDs for connection-oriented media. */
#define RNDIS_OID_GEN_CO_SUPPORTED_LIST		0x00010101
#define RNDIS_OID_GEN_CO_HARDWARE_STATUS	0x00010102
#define RNDIS_OID_GEN_CO_MEDIA_SUPPORTED	0x00010103
#define RNDIS_OID_GEN_CO_MEDIA_IN_USE		0x00010104
#define RNDIS_OID_GEN_CO_LINK_SPEED		0x00010105
#define RNDIS_OID_GEN_CO_VENDOR_ID		0x00010106
#define RNDIS_OID_GEN_CO_VENDOR_DESCRIPTION	0x00010107
#define RNDIS_OID_GEN_CO_DRIVER_VERSION		0x00010108
#define RNDIS_OID_GEN_CO_PROTOCOL_OPTIONS	0x00010109
#define RNDIS_OID_GEN_CO_MAC_OPTIONS		0x0001010A
#define RNDIS_OID_GEN_CO_MEDIA_CONNECT_STATUS	0x0001010B
#define RNDIS_OID_GEN_CO_VENDOR_DRIVER_VERSION	0x0001010C
#define RNDIS_OID_GEN_CO_MINIMUM_LINK_SPEED	0x0001010D

#define RNDIS_OID_GEN_CO_GET_TIME_CAPS		0x00010201
#define RNDIS_OID_GEN_CO_GET_NETCARD_TIME	0x00010202

/* These are connection-oriented statistics OIDs. */
#define RNDIS_OID_GEN_CO_XMIT_PDUS_OK		0x00020101
#define RNDIS_OID_GEN_CO_RCV_PDUS_OK		0x00020102
#define RNDIS_OID_GEN_CO_XMIT_PDUS_ERROR	0x00020103
#define RNDIS_OID_GEN_CO_RCV_PDUS_ERROR		0x00020104
#define RNDIS_OID_GEN_CO_RCV_PDUS_NO_BUFFER	0x00020105


#define RNDIS_OID_GEN_CO_RCV_CRC_ERROR		0x00020201
#define RNDIS_OID_GEN_CO_TRANSMIT_QUEUE_LENGTH	0x00020202
#define RNDIS_OID_GEN_CO_BYTES_XMIT		0x00020203
#define RNDIS_OID_GEN_CO_BYTES_RCV		0x00020204
#define RNDIS_OID_GEN_CO_BYTES_XMIT_OUTSTANDING	0x00020205
#define RNDIS_OID_GEN_CO_NETCARD_LOAD		0x00020206

/* These are objects for Connection-oriented media call-managers. */
#define RNDIS_OID_CO_ADD_PVC			0xFF000001
#define RNDIS_OID_CO_DELETE_PVC			0xFF000002
#define RNDIS_OID_CO_GET_CALL_INFORMATION	0xFF000003
#define RNDIS_OID_CO_ADD_ADDRESS		0xFF000004
#define RNDIS_OID_CO_DELETE_ADDRESS		0xFF000005
#define RNDIS_OID_CO_GET_ADDRESSES		0xFF000006
#define RNDIS_OID_CO_ADDRESS_CHANGE		0xFF000007
#define RNDIS_OID_CO_SIGNALING_ENABLED		0xFF000008
#define RNDIS_OID_CO_SIGNALING_DISABLED		0xFF000009

/* 802.3 Objects (Ethernet) */
#define RNDIS_OID_802_3_PERMANENT_ADDRESS	0x01010101
#define RNDIS_OID_802_3_CURRENT_ADDRESS		0x01010102
#define RNDIS_OID_802_3_MULTICAST_LIST		0x01010103
#define RNDIS_OID_802_3_MAXIMUM_LIST_SIZE	0x01010104
#define RNDIS_OID_802_3_MAC_OPTIONS		0x01010105

#define NDIS_802_3_MAC_OPTION_PRIORITY		0x00000001

#define RNDIS_OID_802_3_RCV_ERROR_ALIGNMENT	0x01020101
#define RNDIS_OID_802_3_XMIT_ONE_COLLISION	0x01020102
#define RNDIS_OID_802_3_XMIT_MORE_COLLISIONS	0x01020103

#define RNDIS_OID_802_3_XMIT_DEFERRED		0x01020201
#define RNDIS_OID_802_3_XMIT_MAX_COLLISIONS	0x01020202
#define RNDIS_OID_802_3_RCV_OVERRUN		0x01020203
#define RNDIS_OID_802_3_XMIT_UNDERRUN		0x01020204
#define RNDIS_OID_802_3_XMIT_HEARTBEAT_FAILURE	0x01020205
#define RNDIS_OID_802_3_XMIT_TIMES_CRS_LOST	0x01020206
#define RNDIS_OID_802_3_XMIT_LATE_COLLISIONS	0x01020207

#define RNDIS_OID_802_11_BSSID				0x0d010101
#define RNDIS_OID_802_11_SSID				0x0d010102
#define RNDIS_OID_802_11_INFRASTRUCTURE_MODE		0x0d010108
#define RNDIS_OID_802_11_ADD_WEP			0x0d010113
#define RNDIS_OID_802_11_REMOVE_WEP			0x0d010114
#define RNDIS_OID_802_11_DISASSOCIATE			0x0d010115
#define RNDIS_OID_802_11_AUTHENTICATION_MODE		0x0d010118
#define RNDIS_OID_802_11_PRIVACY_FILTER			0x0d010119
#define RNDIS_OID_802_11_BSSID_LIST_SCAN		0x0d01011a
#define RNDIS_OID_802_11_ENCRYPTION_STATUS		0x0d01011b
#define RNDIS_OID_802_11_ADD_KEY			0x0d01011d
#define RNDIS_OID_802_11_REMOVE_KEY			0x0d01011e
#define RNDIS_OID_802_11_ASSOCIATION_INFORMATION	0x0d01011f
#define RNDIS_OID_802_11_CAPABILITY			0x0d010122
#define RNDIS_OID_802_11_PMKID				0x0d010123
#define RNDIS_OID_802_11_NETWORK_TYPES_SUPPORTED	0x0d010203
#define RNDIS_OID_802_11_NETWORK_TYPE_IN_USE		0x0d010204
#define RNDIS_OID_802_11_TX_POWER_LEVEL			0x0d010205
#define RNDIS_OID_802_11_RSSI				0x0d010206
#define RNDIS_OID_802_11_RSSI_TRIGGER			0x0d010207
#define RNDIS_OID_802_11_FRAGMENTATION_THRESHOLD	0x0d010209
#define RNDIS_OID_802_11_RTS_THRESHOLD			0x0d01020a
#define RNDIS_OID_802_11_SUPPORTED_RATES		0x0d01020e
#define RNDIS_OID_802_11_CONFIGURATION			0x0d010211
#define RNDIS_OID_802_11_POWER_MODE			0x0d010216
#define RNDIS_OID_802_11_BSSID_LIST			0x0d010217

/* Plug and Play capabilities */
#define RNDIS_OID_PNP_CAPABILITIES		0xFD010100
#define RNDIS_OID_PNP_SET_POWER			0xFD010101
#define RNDIS_OID_PNP_QUERY_POWER		0xFD010102
#define RNDIS_OID_PNP_ADD_WAKE_UP_PATTERN	0xFD010103
#define RNDIS_OID_PNP_REMOVE_WAKE_UP_PATTERN	0xFD010104
#define RNDIS_OID_PNP_ENABLE_WAKE_UP		0xFD010106

#define REMOTE_CONDIS_MP_CREATE_VC_MSG		0x00008001
#define REMOTE_CONDIS_MP_DELETE_VC_MSG		0x00008002
#define REMOTE_CONDIS_MP_ACTIVATE_VC_MSG	0x00008005
#define REMOTE_CONDIS_MP_DEACTIVATE_VC_MSG	0x00008006
#define REMOTE_CONDIS_INDICATE_STATUS_MSG	0x00008007

#define REMOTE_CONDIS_MP_CREATE_VC_CMPLT	0x80008001
#define REMOTE_CONDIS_MP_DELETE_VC_CMPLT	0x80008002
#define REMOTE_CONDIS_MP_ACTIVATE_VC_CMPLT	0x80008005
#define REMOTE_CONDIS_MP_DEACTIVATE_VC_CMPLT	0x80008006

/*
 * Reserved message type for private communication between lower-layer host
 * driver and remote device, if necessary.
 */
#define REMOTE_NDIS_BUS_MSG			0xff000001
