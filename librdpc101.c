/*
 * Control library for SUNTAC RDPC101.
 * See http://suntac.jp/products/usb/rdpc101.html
 *
 * require: hidapi
 * http://www.signal11.us/oss/hidapi/
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rdpc101.h"
#include <hidapi.h>

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

int error_hidapi(const char *label, hid_device* device)
{
	fprintf(stderr, "%s: %s\n", label, hid_error(device));

	return 0;
}

void rdpc101_cleanup(struct dev_info *dev_info)
{
	struct rdpc101_dev *cp;

	for (cp = dev_info->rp; cp;)
	{
		struct rdpc101_dev *pp;

		if (cp->handle)
		{
			hid_close(cp->handle);
		}
		pp = cp;
		cp = cp->next;
		free(pp);
	}
	hid_free_enumeration(dev_info->devs);
	hid_exit();
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
	p->cur.ma = RDPC_MA_UNSPEC;
	p->cur.sig_intensity = 0;
	p->cur.freq = 0;
	return p;
}

static struct rdpc101_dev *
rdpc101_append_node(struct rdpc101_dev *cur, struct hid_device_info* dev)
{
	struct rdpc101_dev *p;
	if (cur->next != NULL )
	{
		return NULL ;
	}
	p = rdpc101_new_node();
	p->dev = dev;
	cur->next = p;
	return p;
}

struct rdpc101_dev *
rdpc101_get_list(struct dev_info *dip)
{
	struct rdpc101_dev head;
	struct rdpc101_dev *cur = &head;
	struct hid_device_info* dev;

	dip->devs = hid_enumerate(RDPC101_VENDORID, RDPC101_PRODUCTID);
	if (dip->devs == NULL)
		return NULL;

	head.next = NULL;

	for (dev = dip->devs; dev; dev = dev->next)
	{
		if (!(cur = rdpc101_append_node(cur, dev)))
			return NULL ;
	}
	return (dip->rp = head.next);
}

hid_device*
get_handle(struct rdpc101_dev *rp)
{
	int ret;

	if (rp->handle)
		return rp->handle;
	if ((rp->handle = hid_open(RDPC101_VENDORID, RDPC101_PRODUCTID, rp->dev->serial_number)) == NULL)
	{
		error_hidapi("open", rp->handle);
		return NULL;
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

int rdpc101_update_state(struct rdpc101_dev *rp)
{
	uint8_t packet[1024];
	int freq, ma, mma;
	int ret;

    if(get_handle(rp) == NULL) {
        return -1;
    }
    
	if((ret = hid_read(get_handle(rp), packet, sizeof packet)) < 0) {
		return ret;
	}

	/* check unknown packet */
	freq = (packet[RDPC_STATE_INDEX_FREQ_HI] << 8
			| packet[RDPC_STATE_INDEX_FREQ_LO]);
	ma = packet[RDPC_STATE_INDEX_MA];
	mma = ma & ~RDPC_MA_SEEKING_MASK;
	if (ret != RDPC101_STATE_PACKET_SIZE || packet[0] != 0x12
			|| (mma != RDPC_MA_MONO && mma != RDPC_MA_STEREO
					&& mma != (0x3e & ~RDPC_MA_SEEKING_MASK)&&
					mma != (0x3f & ~RDPC_MA_SEEKING_MASK) &&
					mma != (0xa7 & ~RDPC_MA_SEEKING_MASK))||
					rdpc101_band(freq) == RDPC_BAND_ERROR ||
					!check_all_zero(&packet[RDPC_STATE_INDEX_MAX],
							ret - RDPC_STATE_INDEX_MAX))
					dump_packet("stat pkt", packet, ret);
	rp->cur.sig_intensity = packet[RDPC_STATE_INDEX_SIGINTENSITY];
	rp->cur.freq = freq;
	rp->cur.ma = ma;

	return 0;
}

int rdpc101_set_report(struct rdpc101_dev *rp, unsigned char *data,
		int data_size)
{
	int ret;

	if((ret = hid_send_feature_report(get_handle(rp), data, data_size)) < 0) {
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
