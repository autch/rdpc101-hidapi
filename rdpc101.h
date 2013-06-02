/*
 * $Id: rdpc101.h,v 1.3 2009/07/07 13:33:53 nishio Exp $
 */
#if !defined(__RDPC101_H)
#define __RDPC101_H
#include <libusb-1.0/libusb.h>

#if !defined __RCSID
#define __RCSID(_s)	__asm(".section rcsid\n\t.asciz \"" _s "\"\n\t.previous")
#endif

#define RDPC101_VENDORID	0x10c4
#define RDPC101_PRODUCTID	0x818a

#ifndef FALSE
#define FALSE		0
#define TRUE		(!FALSE)
#endif

#define RDPC101_TIMEOUT 3000

/*
 * RDPC101 HID cmd etc
 * command & subcommand is 0x00 - 0xff
 */

enum rdpc_cmd {
    RDPC_SETFREQ = 0x02,
    RDPC_MUTE = 0x05,
    RDPC_MA = 0x06,
    RDPC_SEEK = 0x09,
    RDPC_BAND = 0x0a,
    RDPC_UNKNOWN2 = 0x13
};

enum rdpc_mute {
    RDPC_MUTE_OFF = 0,
    RDPC_MUTE_ON
};
enum rdpc_seek {
    RDPC_SEEK_UNSPEC = -3,
    RDPC_SEEK_UNKNOWN = -2,
    RDPC_SEEK_ERROR = -1,
    RDPC_SEEK_UP = 0x01,
    RDPC_SEEK_DOWN = 0x02
};
enum rdpc_band {
    RDPC_BAND_UNSPEC = -3,
    RDPC_BAND_UNKNOWN = -2,
    RDPC_BAND_ERROR = -1,
    RDPC_BAND_FM = 0x02,
    RDPC_BAND_AM = 0x80
};

enum rdpc_ma {
    RDPC_MA_UNSPEC = -3,
    RDPC_MA_UNKNOWN = -2,
    RDPC_MA_ERROR = -1,
    RDPC_MA_MONO = 0x00,
    RDPC_MA_STEREO = 0x01
};

#define RDPC_MA_SEEKING_MASK (1 << 4)

enum hid_bRequest {
    HID_GET_REPORT = 0x01,
    HID_GET_IDLE = 0x02,
    HID_GET_PROTOCOL = 0x03,
    HID_SET_REPORT = 0x09,
    HID_SET_IDLE = 0x0a,
    HID_SET_PROTOCOL = 0x0b
};
enum bmRequestType_direction {
    BMRT_IN = 1 << 7,		/* device to host */
    BMRT_OUT = 0 << 7
};

enum hid_report_type {
    HID_RT_INPUT = 0x01,
    HID_RT_OUTPUT = 0x02,
    HID_RT_FEATURE = 0x03
};

#define RDPC101_STATE_PACKET_SIZE 13
enum rdpc_state_index {
    RDPC_STATE_INDEX_MA = 1,
    RDPC_STATE_INDEX_SIGINTENSITY,
    RDPC_STATE_INDEX_FREQ_HI,
    RDPC_STATE_INDEX_FREQ_LO,
    RDPC_STATE_INDEX_MAX
};

enum rdpc101_hid_ep {
    RDPC_EP1 = LIBUSB_ENDPOINT_IN | 1,
    RDPC_EP2 = LIBUSB_ENDPOINT_OUT | 2
};

enum radio_freq_desc_index {
    RFD_ERROR = -1,
    RFD_AM = 0,
    RFD_FM,
    RFD_TV
};

struct rdpc_state {
    int ma;
    int sig_intensity;
    int freq;
};

struct rdpc101_dev {
    struct rdpc101_dev *next;
    libusb_device *dev;
    libusb_device_handle *handle;
    int hid_index;
    struct rdpc_state prev;
    struct rdpc_state cur;
};

struct radio_freq_desc {
    enum rdpc_band band;
    int min;
    int max;
    int step;
};

struct dev_info {
    libusb_device **devs;
    struct rdpc101_dev *rp;
};

const char const *strlibusb_error(enum libusb_error errnum);
int error_libusb(const char *label, enum libusb_error errnum);
void rdpc101_cleanup(struct libusb_context *ctx, struct dev_info *dev_info);
enum rdpc_band rdpc101_band(int freq);
enum radio_freq_desc_index rdpc101_band_index(int freq);
struct rdpc101_dev *rdpc101_device(struct rdpc101_dev *rp, int index);
struct rdpc101_dev *rdpc101_get_list(struct dev_info *dip);
int rdpc101_update_state(struct rdpc101_dev *rp);
int rdpc101_set_report(struct rdpc101_dev *rp, unsigned char *data, int data_size);
int rdpc101_set_ma(struct rdpc101_dev *rp, enum rdpc_ma ma);
int rdpc101_mute(struct rdpc101_dev *rp, enum rdpc_mute mute);
int rdpc101_set_band(struct rdpc101_dev *rp, enum rdpc_band band);
int rdpc101_set_freq(struct rdpc101_dev *rp, int freq);
int rdpc101_seek(struct rdpc101_dev *rp, enum rdpc_seek seek_dir);
int rdpc101_step(int freq);
int rdpc101_freq_min(enum radio_freq_desc_index i);
int rdpc101_freq_max(enum radio_freq_desc_index i);
int rdpc101_claim_hid(struct rdpc101_dev *rp);
int rdpc101_release_hid(struct rdpc101_dev *rp);
#endif

/*- 
 * Copyright (c) 2009 NISHIO Yasuhiro <nishio@hh.iij4u.or.jp>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the authors nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *
 */
