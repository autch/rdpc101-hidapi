/*
 * Control library for SUNTAC RDPC101.
 * See http://suntac.jp/products/usb/rdpc101.html
 *
 * require: libusb-1.0
 * http://libusb.wiki.sourceforge.net/
 */

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include "rdpc101.h"

__RCSID("$Id: librdpc101.c,v 1.3 2009/07/07 13:33:53 nishio Exp $");

static struct radio_freq_desc rfd[] =
{
#if 0
		{	RDPC_BAND_AM, 531, 1710, 9},
#else
		{ RDPC_BAND_AM, 522, 1629, 9 },
#endif
		{ RDPC_BAND_FM, 7600, 9000, 10 },
		{ RDPC_BAND_FM, 9005, 10800, 5 } };

#define NRFDS	(sizeof (rfd) / sizeof (struct radio_freq_desc))

const char const *
strlibusb_error(enum libusb_error errnum)
{
	static const char const *errstr[] =
	{ "libusb: Success", "libusb: Input/output error",
			"libusb: Invalid parameter",
			"libusb: Access denied (insufficient permissions)",
			"libusb: No such device (it may have been disconnected)",
			"libusb: Entity not found", "libusb: Resource busy",
			"libusb: Operation timed out", "libusb: Overflow",
			"libusb: Pipe error",
			"libusb: System call interrupted (perhaps due to signal)",
			"libusb: Insufficient memory",
			"libusb: Operation not supported or unimplemented on this platform",
			NULL };
	static const int maxno = (sizeof errstr / sizeof *errstr);

	if (0 <= -errnum && -errnum < maxno)
		return errstr[-errnum];
	else if (errnum == LIBUSB_ERROR_OTHER)
		return "libusb: Other error";
	else
		return "libusb: unknown";
}
;

int error_libusb(const char *label, enum libusb_error errnum)
{
	fprintf(stderr, "%s: %s\n", label, strlibusb_error(errnum));

	return errnum;
}

void rdpc101_cleanup(struct libusb_context *ctx, struct dev_info *dev_info)
{
	struct rdpc101_dev *cp;

	for (cp = dev_info->rp; cp;)
	{
		struct rdpc101_dev *pp;

		if (cp->handle)
		{
			if (cp->hid_index >= 0)
				libusb_release_interface(cp->handle, cp->hid_index);
			libusb_close(cp->handle);
		}
		pp = cp;
		cp = cp->next;
		free(pp);
	}
	libusb_free_device_list(dev_info->devs, 1);
	libusb_exit(ctx);
}

enum radio_freq_desc_index rdpc101_band_index(int freq)
{
	int i;

	for (i = 0; i < NRFDS; i++)
		if (rfd[i].min <= freq && freq <= rfd[i].max)
			return i;
	return RFD_ERROR;
}

enum rdpc_band rdpc101_band(int freq)
{
	int ind = rdpc101_band_index(freq);

	if (ind < 0)
	{
		switch (freq)
		{
		case 0x0000:
		case 0xffff:
			return RDPC_BAND_UNSPEC;
		default:
			return RDPC_BAND_ERROR;
		}
	}
	else
		return rfd[ind].band;
}

int rdpc101_step(int freq)
{
	int ind = rdpc101_band_index(freq);

	if (ind < 0)
		return -1;
	return rfd[ind].step;
}

int rdpc101_freq_min(enum radio_freq_desc_index i)
{
	return rfd[i].min;
}

int rdpc101_freq_max(enum radio_freq_desc_index i)
{
	return rfd[i].max;
}

struct rdpc101_dev *
rdpc101_device(struct rdpc101_dev *rp, int index)
{
	struct rdpc101_dev *p;
	int i;

	for (p = rp, i = 0; p; p = p->next, i++)
		if (i == index)
			return p;
	return NULL ;
}

static int rdpc101_hid_interface(libusb_device *dev)
{
	struct libusb_config_descriptor *config;
	struct libusb_interface *intf;
	int n;
	int ret;

	if ((ret = libusb_get_config_descriptor(dev, 0, &config)) < 0)
		return LIBUSB_ERROR_NO_DEVICE;

	n = config->bNumInterfaces;
	for (intf = (struct libusb_interface *) config->interface; n > 0;
			n--, intf++)
	{
		if (intf->altsetting->bInterfaceClass == LIBUSB_CLASS_HID)
			return intf->altsetting->bInterfaceNumber;
	}
	return LIBUSB_ERROR_NO_DEVICE;
}

static struct rdpc101_dev *
rdpc101_new_node(void)
{
	struct rdpc101_dev *p = malloc(sizeof(struct rdpc101_dev));

	if (!p)
	{
		return NULL ;
	}
	p->next = NULL;
	p->dev = NULL;
	p->handle = NULL;
	p->hid_index = -1;
	p->cur.ma = RDPC_MA_UNSPEC;
	p->cur.sig_intensity = 0;
	p->cur.freq = 0;
	return p;
}

static struct rdpc101_dev *
rdpc101_append_node(struct rdpc101_dev *cur, libusb_device *dev)
{
	struct rdpc101_dev *p;
	if (cur->next != NULL )
	{
		return NULL ;
	}
	p = rdpc101_new_node();
	p->dev = dev;
	p->hid_index = rdpc101_hid_interface(dev);
	cur->next = p;
	return p;
}

struct rdpc101_dev *
rdpc101_get_list(struct dev_info *dip)
{
	struct rdpc101_dev head;
	struct rdpc101_dev *cur = &head;
	libusb_device *dev, **devs;
	int n = libusb_get_device_list(NULL, &dip->devs);

	if (n < 0)
		return NULL ;

	head.next = NULL;

	devs = dip->devs;
	for (dev = *devs; dev; dev = *++devs)
	{
		struct libusb_device_descriptor desc;

		if (libusb_get_device_descriptor(dev, &desc) < 0)
			continue;
		if (desc.idVendor == RDPC101_VENDORID
				&& desc.idProduct == RDPC101_PRODUCTID)
			if (!(cur = rdpc101_append_node(cur, dev)))
				return NULL ;
	}
	return (dip->rp = head.next);
}

static libusb_device_handle *
get_handle(struct rdpc101_dev *rp)
{
	int ret;

	if (rp->handle)
		return rp->handle;
	if ((ret = libusb_open(rp->dev, &rp->handle)) < 0)
	{
		error_libusb("open", ret);
		return NULL ;
	}

	switch ((ret = libusb_kernel_driver_active(rp->handle, rp->hid_index)))
	{
	case 0:
		break;
	case 1:
		if ((ret = libusb_detach_kernel_driver(rp->handle, rp->hid_index)) < 0)
			error_libusb("detach_kernel_driver", ret);
		break;
	default:
		error_libusb("kerbel_deriver_active", ret);
		break;
	}
	return rp->handle;
}

static void dump_packet(char *label, unsigned char *p, int size)
{
	fprintf(stderr, "%10s:", label);
	while (size-- > 0)
		fprintf(stderr, " %2.2x", *p++);
	putc('\n', stderr);
}

static int check_all_zero(unsigned char *p, int size)
{
	while (size-- > 0)
	{
		if (*p++)
		{
			fprintf(stderr, "XXX: %2.2x\n", *p);
			return FALSE;
		}
	}
	return TRUE;
}

int rdpc101_claim_hid(struct rdpc101_dev *rp)
{
	return libusb_claim_interface(get_handle(rp), rp->hid_index);
}

int rdpc101_release_hid(struct rdpc101_dev *rp)
{
	return libusb_release_interface(get_handle(rp), rp->hid_index);
}

int rdpc101_update_state(struct rdpc101_dev *rp)
{
	unsigned char *packet = NULL;
	static int size = -1;
	int freq, ma, mma;
	int ret;

	if (size < 0 && (size = libusb_get_max_packet_size(rp->dev, RDPC_EP1)) < 0)
		return size;

	if ((packet = malloc(size)) == NULL )
		return -1;

	if ((ret = libusb_interrupt_transfer(get_handle(rp), RDPC_EP1, packet, size,
			&size, RDPC101_TIMEOUT)) < 0)
	{
		free(packet);
		return ret;
	}

	/* check unknown packet */
	freq = (packet[RDPC_STATE_INDEX_FREQ_HI] << 8
			| packet[RDPC_STATE_INDEX_FREQ_LO]);
	ma = packet[RDPC_STATE_INDEX_MA];
	mma = ma & ~RDPC_MA_SEEKING_MASK;
	if (size != RDPC101_STATE_PACKET_SIZE || packet[0] != 0x12
			|| (mma != RDPC_MA_MONO && mma != RDPC_MA_STEREO
					&& mma != (0x3e & ~RDPC_MA_SEEKING_MASK)&&
					mma != (0x3f & ~RDPC_MA_SEEKING_MASK) &&
					mma != (0xa7 & ~RDPC_MA_SEEKING_MASK))||
					rdpc101_band(freq) == RDPC_BAND_ERROR ||
					!check_all_zero(&packet[RDPC_STATE_INDEX_MAX],
							size - RDPC_STATE_INDEX_MAX))
					dump_packet("stat pkt", packet, size);
	rp->cur.sig_intensity = packet[RDPC_STATE_INDEX_SIGINTENSITY];
	rp->cur.freq = freq;
	rp->cur.ma = ma;

	free(packet);
	return ret;
}

int rdpc101_set_report(struct rdpc101_dev *rp, unsigned char *data,
		int data_size)
{
	uint8_t bmRequestType = BMRT_OUT | LIBUSB_REQUEST_TYPE_CLASS
			| LIBUSB_RECIPIENT_INTERFACE;
	uint8_t bRequest = HID_SET_REPORT;
	uint16_t wValue = (HID_RT_FEATURE << 8) | data[0];
	int ret;

	if ((ret = libusb_control_transfer(get_handle(rp), bmRequestType, bRequest,
			wValue, (uint16_t) rp->hid_index, data, (uint16_t) data_size,
			RDPC101_TIMEOUT)) < 0)
	{
		dump_packet("control_transfer", data, data_size);
		return ret;
	}
	return ret;
}

int rdpc101_set_ma(struct rdpc101_dev *rp, enum rdpc_ma ma)
{
	unsigned char packet[3] =
	{ RDPC_MA, ma, 0x00 };

	return rdpc101_set_report(rp, packet, sizeof packet);
}

int rdpc101_mute(struct rdpc101_dev *rp, enum rdpc_mute mute)
{
	unsigned char packet[3] =
	{ 0x05, mute, 0x00 };

	return rdpc101_set_report(rp, packet, sizeof packet);
}

int rdpc101_set_band(struct rdpc101_dev *rp, enum rdpc_band band)
{
	unsigned char packet[3] =
	{ RDPC_BAND, band, 0x02 };

	return rdpc101_set_report(rp, packet, sizeof packet);
}

int rdpc101_set_freq(struct rdpc101_dev *rp, int freq)
{
	unsigned char packet[3] =
	{ RDPC_SETFREQ, freq >> 8, freq & 0xff };

	return rdpc101_set_report(rp, packet, sizeof packet);
}

int rdpc101_seek(struct rdpc101_dev *rp, enum rdpc_seek seek_dir)
{
	unsigned char packet[3] =
	{ RDPC_SEEK, seek_dir, 0x00 };

	return rdpc101_set_report(rp, packet, sizeof packet);
}

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
 */
