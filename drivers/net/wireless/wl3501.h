#ifndef __WL3501_H__
#define __WL3501_H__

#define WL3501_SLOW_DOWN_IO __asm__ __volatile__("outb %al,$0x80")

/* define for WLA 2.0 */

#define WL3501_BLKSZ 256
/*
 * ID for input Signals of DRIVER block
 * bit[7-5] is block ID: 000
 * bit[4-0] is signal ID
*/
#define WL3501_Alarm		0x00
#define WL3501_MdConfirm	0x01
#define WL3501_MdIndicate	0x02
#define WL3501_AssocConfirm	0x03
#define WL3501_AssocIndicate	0x04
#define WL3501_AutheConfirm	0x05
#define WL3501_AutheIndicate	0x06
#define WL3501_DeautheConfirm	0x07
#define WL3501_DeautheIndicate 	0x08
#define WL3501_DisassocConfirm 	0x09
#define WL3501_DisassocIndicate	0x0A
#define WL3501_GetConfirm	0x0B
#define WL3501_JoinConfirm 	0x0C
#define WL3501_PowermgtConfirm 	0x0D
#define WL3501_ReassocConfirm	0x0E
#define WL3501_ReassocIndicate 	0x0F
#define WL3501_ScanConfirm 	0x10
#define WL3501_SetConfirm	0x11
#define WL3501_StartConfirm	0x12
#define WL3501_ResyncConfirm	0x13
#define WL3501_SiteConfirm 	0x14
#define WL3501_SaveConfirm 	0x15
#define WL3501_RFtestConfirm	0x16

/*
 * ID for input Signals of MLME block
 * bit[7-5] is block ID: 010
 * bit[4-0] is signal ID
 */
#define WL3501_AssocRequest	0x20
#define WL3501_AutheRequest	0x21
#define WL3501_DeautheRequest	0x22
#define WL3501_DisassocRequest	0x23
#define WL3501_GetRequest	0x24
#define WL3501_JoinRequest	0x25
#define WL3501_PowermgtRequest	0x26
#define WL3501_ReassocRequest	0x27
#define WL3501_ScanRequest	0x28
#define WL3501_SetRequest	0x29
#define WL3501_StartRequest	0x2A
#define WL3501_MdRequest	0x2B
#define WL3501_ResyncRequest	0x2C
#define WL3501_SiteRequest	0x2D
#define WL3501_SaveRequest	0x2E
#define WL3501_RFtestRequest	0x2F

#define WL3501_MmConfirm	0x60
#define WL3501_MmIndicate	0x61

#define WL3501_Infrastructure		0
#define WL3501_Independent		1
#define WL3501_Any_bss			2
#define WL3501_ActiveScan		0
#define WL3501_PassiveScan		1
#define WL3501_TxResult_Success		0
#define WL3501_TxResult_NoBss		1
#define WL3501_TxResult_retryLimit	2

#define WL3501_Open_System	0
#define WL3501_Share_Key	1

#define EtherII     0
#define Ether802_3e 1
#define Ether802_3f 2

#define WL3501_STATUS_SUCCESS    0
#define WL3501_STATUS_INVALID    1
#define WL3501_STATUS_TIMEOUT    2
#define WL3501_STATUS_REFUSED    3
#define WL3501_STATUS_MANYREQ    4
#define WL3501_STATUS_ALREADYBSS 5

#define WL3501_FREQ_DOMAIN_FCC    0x10	/* Channel 1 to 11 */
#define WL3501_FREQ_DOMAIN_IC     0x20	/* Channel 1 to 11 */
#define WL3501_FREQ_DOMAIN_ETSI   0x30	/* Channel 1 to 13 */
#define WL3501_FREQ_DOMAIN_SPAIN  0x31	/* Channel 10 to 11 */
#define WL3501_FREQ_DOMAIN_FRANCE 0x32	/* Channel 10 to 13 */
#define WL3501_FREQ_DOMAIN_MKK    0x40	/* Channel 14 */

struct wl3501_tx_hdr {
	u16 tx_cnt;
	unsigned char sync[16];
	u16 sfd;
	unsigned char signal;
	unsigned char service;
	u16 len;
	u16 crc16;
	u16 frame_ctrl;
	u16 duration_id;
	unsigned char addr1[6];
	unsigned char addr2[6];
	unsigned char addr3[6];
	u16 seq_ctrl;
	unsigned char addr4[6];
};

struct wl3501_rx_hdr {
	u16 rx_next_blk;
	u16 rc_next_frame_blk;
	unsigned char rx_blk_ctrl;
	unsigned char rx_next_frame;
	unsigned char rx_next_frame1;
	unsigned char rssi;
	unsigned char time[8];
	unsigned char signal;
	unsigned char service;
	u16 len;
	u16 crc16;
	u16 frame_ctrl;
	u16 duration;
	unsigned char addr1[6];
	unsigned char addr2[6];
	unsigned char addr3[6];
	u16 seq;
	unsigned char addr4[6];
};

struct wl3501_start_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char bss_type;
	u16 beacon_period;
	u16 dtim_period;
	u16 probe_delay;
	u16 cap_info;
	unsigned char ssid[34];
	unsigned char bss_basic_rate_set[10];
	unsigned char operational_rate_set[10];
	unsigned char cf_pset[8];
	unsigned char phy_pset[3];
	unsigned char ibss_pset[4];
};

struct wl3501_assoc_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 timeout;
	u16 cap_info;
	u16 listen_interval;
	unsigned char mac_addr[6];
};

struct wl3501_assoc_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 status;
};

struct wl3501_assoc_ind {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char mac_addr[6];
};

struct wl3501_auth_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 type;
	u16 timeout;
	unsigned char mac_addr[6];
};

struct wl3501_auth_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 Type;
	u16 status;
	unsigned char mac_addr[6];
};

struct wl3501_get_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 mib_status;
	u16 mib_attrib;
	unsigned char mib_value[100];
};

struct wl3501_join_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	unsigned char operational_rate_set[10];
	u16 reserved2;
	u16 timeout;
	u16 probe_delay;
	unsigned char timestamp[8];
	unsigned char local_time[8];
	u16 beacon_period;
	u16 dtim_period;
	u16 cap_info;
	unsigned char bss_type;
	unsigned char bssid[6];
	unsigned char ssid[34];
	unsigned char phy_pset[3];
	unsigned char cf_pset[8];
	unsigned char ibss_pset[4];
	unsigned char bss_basic_rate_set[10];
};

struct wl3501_join_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 status;
};

struct wl3501_scan_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char bss_type;
	u16 probe_delay;
	u16 min_chan_time;
	u16 max_chan_time;
	unsigned char chan_list[14];
	unsigned char bssid[6];
	unsigned char ssid[34];
	unsigned char scan_type;
};

struct wl3501_scan_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 status;
	unsigned char timestamp[8];
	unsigned char localtime[8];
	u16 beacon_period;
	u16 dtim_period;
	u16 cap_info;
	unsigned char bss_type;
	unsigned char bssid[6];
	unsigned char ssid[34];
	unsigned char phy_pset[3];
	unsigned char cf_pset[8];
	unsigned char ibss_pset[4];
	unsigned char bss_basic_rate_set[10];
	unsigned char rssi;
};

struct wl3501_start_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 status;
};

struct wl3501_md_req {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char routing;
	u16 data;
	u16 size;
	unsigned char pri;
	unsigned char service_class;
	unsigned char daddr[6];
	unsigned char saddr[6];
};

struct wl3501_md_ind {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char routing;
	u16 data;
	u16 size;
	unsigned char reception;
	unsigned char pri;
	unsigned char service_class;
	unsigned char daddr[6];
	unsigned char saddr[6];
};

struct wl3501_md_conf {
	u16 next_blk;
	unsigned char sig_id;
	unsigned char reserved;
	u16 data;
	unsigned char status;
	unsigned char pri;
	unsigned char service_class;
};

struct wl3501_resync_req {
	u16 next_blk;
	unsigned char sig_id;
};

/* For rough constant delay */
#define WL3501_NOPLOOP(n) { int x = 0; while (x++ < n) WL3501_SLOW_DOWN_IO; }

/* Ethernet MAC addr, BSS_ID, or ESS_ID */
/* With this, we may simply write "x=y;" instead of "memcpy(x, y, 6);" */
/* It's more efficiency with compiler's optimization and more clearly  */
struct wl3501_mac_addr {
	u8 b0;
	u8 b1;
	u8 b2;
	u8 b3;
	u8 b4;
	u8 b5;
} __attribute__ ((packed));

/* Definitions for supporting clone adapters. */
/* System Interface Registers (SIR space) */
#define WL3501_NIC_GCR ((u8)0x00)	/* SIR0 - General Conf Register */
#define WL3501_NIC_BSS ((u8)0x01)	/* SIR1 - Bank Switching Select Reg */
#define WL3501_NIC_LMAL ((u8)0x02)	/* SIR2 - Local Mem addr Reg [7:0] */
#define WL3501_NIC_LMAH ((u8)0x03)	/* SIR3 - Local Mem addr Reg [14:8] */
#define WL3501_NIC_IODPA ((u8)0x04)	/* SIR4 - I/O Data Port A */
#define WL3501_NIC_IODPB ((u8)0x05)	/* SIR5 - I/O Data Port B */
#define WL3501_NIC_IODPC ((u8)0x06)	/* SIR6 - I/O Data Port C */
#define WL3501_NIC_IODPD ((u8)0x07)	/* SIR7 - I/O Data Port D */

/* Bits in GCR */
#define WL3501_GCR_SWRESET ((u8)0x80)
#define WL3501_GCR_CORESET ((u8)0x40)
#define WL3501_GCR_DISPWDN ((u8)0x20)
#define WL3501_GCR_ECWAIT  ((u8)0x10)
#define WL3501_GCR_ECINT   ((u8)0x08)
#define WL3501_GCR_INT2EC  ((u8)0x04)
#define WL3501_GCR_ENECINT ((u8)0x02)
#define WL3501_GCR_DAM     ((u8)0x01)

/* Bits in BSS (Bank Switching Select Register) */
#define WL3501_BSS_FPAGE0 ((u8)0x20)	/* Flash memory page0 */
#define WL3501_BSS_FPAGE1 ((u8)0x28)
#define WL3501_BSS_FPAGE2 ((u8)0x30)
#define WL3501_BSS_FPAGE3 ((u8)0x38)
#define WL3501_BSS_SPAGE0 ((u8)0x00)	/* SRAM page0 */
#define WL3501_BSS_SPAGE1 ((u8)0x08)
#define WL3501_BSS_SPAGE2 ((u8)0x10)
#define WL3501_BSS_SPAGE3 ((u8)0x18)

/* Define Driver Interface */
/* Refer IEEE 802.11 */
/* Tx packet header, include PLCP and MPDU */
/* Tx PLCP Header */
struct wl3501_80211_tx_plcp_hdr {
	u8 sync[16];
	u16 sfd;
	u8 signal;
	u8 service;
	u16 len;
	u16 crc16;
} __attribute__ ((packed));

/*
 * Data Frame MAC Header (IEEE 802.11)
 * FIXME: try to use ieee_802_11_header (see linux/802_11.h)
 */
struct wl3501_80211_data_mac_hdr {
	u16 frame_ctrl;
	u16 duration_id;
	struct wl3501_mac_addr addr1;
	struct wl3501_mac_addr addr2;
	struct wl3501_mac_addr addr3;
	u16 seq_ctrl;
	struct wl3501_mac_addr addr4;
} __attribute__ ((packed));

struct wl3501_80211_tx_hdr {
	struct wl3501_80211_tx_plcp_hdr pclp_hdr;
	struct wl3501_80211_data_mac_hdr mac_hdr;
} __attribute__ ((packed));

/*
   Reserve the beginning Tx space for descriptor use.

   TxBlockOffset -->	*----*----*----*----* \
	(TxFreeDesc)	|  0 |  1 |  2 |  3 |  \
			|  4 |  5 |  6 |  7 |   |
			|  8 |  9 | 10 | 11 |   TX_DESC * 20
			| 12 | 13 | 14 | 15 |   |
			| 16 | 17 | 18 | 19 |  /
   TxBufferBegin -->	*----*----*----*----* /
   (TxBufferHead)	| 		    |
   (TxBufferTail)	| 		    |
			|    Send Buffer    |
			| 		    |
			|		    |
			*-------------------*
   TxBufferEnd    -------------------------/

*/

struct wl3501_card {
	int base_addr;
	struct wl3501_mac_addr mac_addr;
	u16 tx_buffer_size;
	u16 tx_buffer_head;
	u16 tx_buffer_tail;
	u16 tx_buffer_cnt;
	u16 esbq_req_start;
	u16 esbq_req_end;
	u16 esbq_req_head;
	u16 esbq_req_tail;
	u16 esbq_confirm_start;
	u16 esbq_confirm_end;
	u16 esbq_confirm;
	struct wl3501_mac_addr bssid;
	u8 llc_type;
	u8 net_type;
	u8 essid[34];
	u8 keep_essid[34];
	u8 ether_type;
	u8 chan;
	u8 def_chan;
	u16 start_seg;
	u16 bss_cnt;
	u16 join_sta_bss;
	u8 cap_info;
	u8 adhoc_times;
	int card_start;
	struct wl3501_scan_conf bss_set[20];
	u8 driver_state;
	u8 freq_domain;
	u8 version[2];
	struct net_device_stats stats;
	struct iw_statistics wstats;
	dev_node_t node;
};

/*
 * wl3501_ioctl_blk is put into ifreq.ifr_data which is a union (16 bytes)
 * sizeof(wl3501_ioctl_blk) must be less than 16 bytes.
 */
struct wl3501_ioctl_blk {
	u16 cmd;		/* Command to run */
	u16 len;		/* Length of the data buffer */
	unsigned char *data;	/* Pointer to the data buffer */
};

struct wl3501_ioctl_parm {
	u8 def_chan;
	u8 chan;
	u8 net_type;
	u8 essid[34];
	u8 keep_essid[34];
	u8 version[2];
	u8 freq_domain;
};

#define WL3501_IOCTL_GET_PARAMETER   0	/* Get parameter */
#define WL3501_IOCTL_SET_PARAMETER   1	/* Get parameter */
#define WL3501_IOCTL_WRITE_FLASH     2	/* Write firmware into Flash ROM */
#define WL3501_IOCTL_SET_RESET       3	/* Reset the firmware */

#endif				/* __WL3501_H__ */
