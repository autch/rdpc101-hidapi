/*
 * Control program for SUNTAC RDPC101.
 * See http://suntac.jp/products/usb/rdpc101.html
 *
 * compile
 * $ cc rdpc101.c librdpc101.c -o rdpc101 -lusb-1.0
 *
 * required: libusb-1.0
 * http://libusb.wiki.sourceforge.net/
 *
 * $ rdpc101 -h for help.
 *
 * for linux: require root priv or mount usbfs with devgid=GID,devmode=MODE
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "rdpc101.h"

#define ISTRING_MAX	512
#define FREQ_MAX_WIDTH_STR	"108.00 MHz"
#define FREQ_MAX_WIDTH	(sizeof (FREQ_MAX_WIDTH_STR) - 1)
#define FREQSTR_MAX	((FREQ_MAX_WIDTH + 1 + sizeof (int) - 1) & ~(sizeof (int) - 1))

/*
 * Utils
 */
#if !defined(Info)
#define Info(fmt, arg...)						\
    do {								\
	if (flag_verbose > 1)						\
	    fprintf(stderr, __FILE__ "(%d): " fmt "\n", __LINE__, ## arg); \
    } while (0)

#define Info_packet(label, buf, size)					\
    do {								\
	if (flag_verbose > 1)						\
	    dump_packet(label, buf, size);				\
    } while (0)
#define Notice(fmt, arg...)						\
    do {								\
	if (flag_verbose > 0)						\
	    fprintf(stderr, __FILE__ "(%d): " fmt "\n", __LINE__, ## arg); \
    } while (0)
#define Warn(fmt, arg...)						\
    do {								\
	fprintf(stderr, __FILE__ "(%d): " fmt "\n", __LINE__, ## arg);	\
    } while (0)

#define Error(fmt, arg...)						\
    do {								\
	fprintf(stderr, __FILE__ "(%d): " fmt "\n", __LINE__, ## arg);	\
    } while (0)

#if defined(DEBUG)
#define Debug(fmt, arg...)						\
    do {								\
	fprintf(stderr, __FILE__ "(%d): " fmt "\n", __LINE__, ## arg);	\
    } while (0)
#else
#define Debug(fmt, arg...) do {} while (0)
#endif
#endif

struct dev_info *get_dev_info(void);
void rdpc101_list_device(struct rdpc101_dev *rp);
char *sstr_freq(char *buf, int size, int freq);
int rdpc101_scan(struct rdpc101_dev *rp, enum rdpc_band band);
void set_signal_handlers(void);
sigset_t block_sigs();
void unblock_sigs(sigset_t sigs);
void display_freq(struct rdpc101_dev *rp);
void rdpc101_display_seeking(struct rdpc101_dev *rp);
int rdpc101_scan(struct rdpc101_dev *rp, enum rdpc_band band);

__RCSID("$Id: rdpc101.c,v 1.3 2009/07/07 13:33:53 nishio Exp $");

int flag_verbose = 0;
const char *program_name;

int main(int argc, char **argv)
{
	int c;
	int freq = 0;
	int dev_index = 0;
	int flag_list = 0;
	int flag_expert = 0;
	int ret;
	enum rdpc_band flag_scan = RDPC_BAND_UNSPEC;
	enum rdpc_ma flag_ma = RDPC_MA_UNSPEC;
	enum rdpc_seek flag_seek = RDPC_SEEK_UNSPEC;
	struct dev_info *dev_info;
	struct rdpc101_dev *rdpc101_list;
	struct rdpc101_dev *rp;
	void set_signal_handlers(void);
	void usage(void);

	program_name = argv[0];
	opterr = 0;
	while ((c = getopt(argc, argv, "Dd:lmsS:vUx")) != -1)
		switch (c)
		{
		case 'D':
			flag_seek = RDPC_SEEK_DOWN;
			break;
		case 'd':
			if (!isdigit(*optarg))
			{
				fprintf(stderr, "-d require device number.\n\n");
				usage();
				exit(1);
			}
			dev_index = atoi(optarg);
			break;
		case 'l':
			flag_list++;
			break;
		case 'm':
			flag_ma = RDPC_MA_MONO;
			break;
		case 'S':
			switch (tolower(*optarg))
			{
			case 'a':
				flag_scan = RDPC_BAND_AM;
				break;
			case 'f':
				flag_scan = RDPC_BAND_FM;
				break;
			default:
				fprintf(stderr, "invalid arg: %s\n", optarg);
				usage();
				exit(1);
				break;
			}
			break;
		case 's':
			flag_ma = RDPC_MA_STEREO;
			break;
		case 'U':
			flag_seek = RDPC_SEEK_UP;
			break;
		case 'v':
			flag_verbose++;
			break;
		case 'x':
			flag_expert++;
			break;
		default:
			usage();
			exit(1);
			break;
		}

	argc -= optind;
	argv += optind;

	if (argc > 0 && isdigit(**argv))
	{
		freq = atoi(*argv);
		if (rdpc101_band(freq * 100) == RDPC_BAND_FM)
		{
			int step = rdpc101_step(freq * 100);

			if (flag_expert)
				freq = (int) (atof(*argv) * 100.0);
			else
				freq = (((int) (atof(*argv) * 100.0) + (step >> 1)) / step)
						* step;
			if (flag_ma == RDPC_MA_UNSPEC)
				flag_ma = RDPC_MA_STEREO;
		}
		else if (rdpc101_band(freq) == RDPC_BAND_AM)
		{
			int step = rdpc101_step(freq);

			if (!flag_expert)
				freq = ((freq + (step >> 1)) / step) * step;
			if (flag_ma == RDPC_MA_UNSPEC)
				flag_ma = RDPC_MA_MONO;
		}
		else
		{
			fprintf(stderr, "invalid freq range: %s\n", *argv);
			exit(1);
		}
	}

	if ((dev_info = get_dev_info()) == NULL )
	{
		perror("make_dev_info");
		exit(1);
	}

	set_signal_handlers();

	if (hid_init() < 0)
	{
		fprintf(stderr, "cannot init\n");
		exit(1);
	}

	if ((rdpc101_list = rdpc101_get_list(dev_info)) == NULL )
	{
		fprintf(stderr, "Cannot found rdpc101.\n");
		rdpc101_cleanup(dev_info);
		exit(1);
	}

	if (!(rp = rdpc101_device(rdpc101_list, dev_index)))
	{
		fprintf(stderr, "invalid dev_index\n");
		rdpc101_cleanup(dev_info);
		exit(1);
	}

	if (flag_list)
	{
		rdpc101_list_device(rdpc101_list);
	}
	else
	{
		int ret;
		if (rdpc101_update_state(rp) < 0)
		{
			fprintf(stderr, "Cannot stat dev: %d", dev_index);
			rdpc101_cleanup(dev_info);
			exit(1);
		}
	}

	if (freq > 0)
	{
		sigset_t prev_sigset;
		enum rdpc_band band = rdpc101_band(freq);

		if (band != rdpc101_band(rp->cur.freq)
				&& rdpc101_set_band(rp, band) < 0)
		{
			fprintf(stderr, "Cannot set band to %s\n",
					(band == RDPC_BAND_AM) ? "AM" : "FM");
			rdpc101_cleanup(dev_info);
			exit(1);
		}
		if (freq != rp->cur.freq && rdpc101_set_freq(rp, freq) < 0)
		{
			char freqstr[FREQSTR_MAX];

			sstr_freq(freqstr, sizeof freqstr, freq);
			fprintf(stderr, "Cannot set freq to %s\n", freqstr);
			rdpc101_cleanup(dev_info);
			exit(1);
		}
#if 1
		prev_sigset = block_sigs();
		rdpc101_display_seeking(rp);
		unblock_sigs(prev_sigset);
#endif
	}
	else if (flag_seek != RDPC_SEEK_UNSPEC)
	{
		int ret;
		sigset_t prev_sigset;

		if (rdpc101_band_index(rp->cur.freq) < 0)
		{
			fprintf(stderr, "unknown band.\n");
			rdpc101_cleanup(dev_info);
			exit(1);
		}
		else
		{
			prev_sigset = block_sigs();
			ret = rdpc101_mute(rp, RDPC_MUTE_ON);
			if ((ret = rdpc101_seek(rp, flag_seek)) < 0)
			{
				fprintf(stderr, "Cannot seek\n");
				rdpc101_cleanup(dev_info);
				exit(1);
			}
			rdpc101_display_seeking(rp);
			rdpc101_mute(rp, RDPC_MUTE_OFF);
			unblock_sigs(prev_sigset);
		}
	}
	else if (flag_scan != RDPC_BAND_UNSPEC)
	{
		if (rdpc101_scan(rp, flag_scan) < 0)
		{
			fprintf(stderr, "Cannot scan\n");
			rdpc101_cleanup(dev_info);
			exit(1);
		}
	}
	if (flag_ma != RDPC_MA_UNSPEC)
	{
		if (flag_ma != rp->cur.ma && rdpc101_set_ma(rp, flag_ma) < 0)
			fprintf(stderr, "Cannot set ma to %d\n", flag_ma);
	}
}

/* utils */
void usage(void)
{
	fprintf(stderr, "Usage: %s [options] freq\n", program_name);
	fprintf(stderr, "  -d dev_index\tspecify rdpc101#\n"
			"  -l\t\tlist rdpc101 devices\n"
			"  -m\t\tmonaural\n"
			"  -s\t\tstereo\n"
			"  -S am|fm\tscan\n"
			"  -v\t\tincrement verbose level\n"
			"  -D\t\tseek down\n"
			"  -U\t\tseek up\n"
			"freq\t\t 900 ... am  900 Khz\n"
			"\t\t86.0 ... fm 86.0 Mhz\n");
}

struct dev_info *
get_dev_info(void)
{
	static struct dev_info dev_info =
	{ NULL, NULL };

	return &dev_info;
}

void rdpc101_sighand(int sig)
{
	rdpc101_cleanup(get_dev_info());
	fprintf(stderr, "\nCaught sig %d\n", sig);
	exit(1);
}

char *
sstr_freq(char *buf, int size, int freq)
{
	switch (rdpc101_band_index(freq))
	{
	case RFD_FM:
	case RFD_TV:
		snprintf(buf, size, "%3d.%2.2d MHz", freq / 100, freq % 100);
		break;
	case RFD_AM:
		snprintf(buf, size, "%6d KHz", freq);
		break;
	default:
		snprintf(buf, size, "---- _Hz");
		break;
	}
	return buf;
}

char const *
str_ma(enum rdpc_ma ma)
{
	char const *p = "unspec";

	switch (ma)
	{
	case RDPC_MA_MONO:
		p = "Monaural";
		break;
	case RDPC_MA_STEREO:
		p = "Stereo";
		break;
	default:
		break;
	}
	return p;
}

void rdpc101_list_device(struct rdpc101_dev *rp)
{
	struct rdpc101_dev *p;
	char freqstr[FREQSTR_MAX];
	int i;

	printf("No Serial  Station    Audio    Int\n");
	for (p = rp, i = 0; p; p = p->next, i++)
	{
		int ret;

		rdpc101_update_state(p);
		sstr_freq(freqstr, sizeof freqstr, p->cur.freq);
		printf("%2d %s  %10s %-8s %2d\n", i,
				p->dev->serial_number,
				freqstr, str_ma(p->cur.ma),
				p->cur.sig_intensity);
	}
}

void set_signal_handlers(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof act);
	act.sa_flags = SA_RESTART;
	act.sa_handler = rdpc101_sighand;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGINT);
	sigaddset(&act.sa_mask, SIGQUIT);
	sigaddset(&act.sa_mask, SIGTERM);

	sigaction(SIGINT, &act, NULL );
	sigaction(SIGQUIT, &act, NULL );
	sigaction(SIGTERM, &act, NULL );
}

sigset_t block_sigs()
{
	sigset_t new, old;

	memset(&new, 0, sizeof new);

	sigaddset(&new, SIGTSTP);
	sigaddset(&new, SIGINT);
	sigaddset(&new, SIGQUIT);
	sigaddset(&new, SIGTERM);
	sigprocmask(SIG_BLOCK, &new, &old);

	return old;
}

void unblock_sigs(sigset_t sigs)
{
	sigprocmask(SIG_SETMASK, &sigs, NULL );
}

void display_freq(struct rdpc101_dev *rp)
{
	char freqstr[FREQSTR_MAX];
	static char fmt[16];
	int width = FREQ_MAX_WIDTH;

	if (*fmt == '\0')
	{
		snprintf(fmt, sizeof(fmt) - 1, "%%%ds", width);
	}

	sstr_freq(freqstr, sizeof freqstr, rp->cur.freq);
	printf(fmt, freqstr);
	fflush(stdout);
}

void rdpc101_display_seeking(struct rdpc101_dev *rp)
{
	struct timespec t = { 0, 200 * 1000 * 1000 };
	int ret;
	int tty = isatty(1);

	do
	{
		if(tty)
		{
			putchar('\r');
			display_freq(rp);
		}
		if (nanosleep(&t, NULL) != 0)
		{
			perror("nanosleep");
			return;
		}
		ret = rdpc101_update_state(rp);
	} while ((rp->cur.ma & RDPC_MA_SEEKING_MASK) && ret == 0);
	if(ret)
		Error("seek failed:(%d)", ret);
	else
	{
		if (tty)
			putchar('\r');
		display_freq(rp);
		printf("  %3d\n", rp->cur.sig_intensity);
	}
}

int rdpc101_scan(struct rdpc101_dev *rp, enum rdpc_band band)
{
	int ofreq, freq;
	int oband;
	int ret;
	char freqstr[FREQSTR_MAX];
	int freq_min = rdpc101_freq_min(band == RDPC_BAND_AM ? RFD_AM : RFD_FM);
	int freq_max = rdpc101_freq_max(band == RDPC_BAND_AM ? RFD_AM : RFD_FM);

	ofreq = rp->cur.freq;
	oband = rdpc101_band(ofreq);

	if (oband != band && (ret = rdpc101_set_band(rp, band)) < 0)
	{
		Error("Cannot set band: %x", band);
		return ret;
	}
	if ((ret = rdpc101_set_freq(rp, freq_min)) < 0)
	{
		sstr_freq(freqstr, sizeof freqstr, freq_min);
		Error("Cannot set freq to %s", freqstr);
		return ret;
	}
	for (freq = freq_min; freq < freq_max; freq = rp->cur.freq)
	{
		sigset_t prev_sigs = block_sigs();

		rdpc101_mute(rp, RDPC_MUTE_ON);
		if ((ret = rdpc101_seek(rp, RDPC_SEEK_UP)) < 0)
		{
			Error("Cannot seek: UP");
			return ret;
		}
		rdpc101_display_seeking(rp);
		rdpc101_mute(rp, RDPC_MUTE_OFF);
		unblock_sigs(prev_sigs);
	}

	if ((oband != band) && (ret = rdpc101_set_band(rp, oband)) < 0)
	{
		Error("Cannot set band: %d", oband);
		return ret;
	}
	if ((ret = rdpc101_set_freq(rp, ofreq)) < 0)
	{
		char freqstr[FREQSTR_MAX];

		sstr_freq(freqstr, sizeof freqstr, ofreq);
		Error("Cannot set freq to %s", freqstr);
		return ret;
	}
	return ret;
}

#if 0
char *
get_istring(libusb_device_handle *h, int index)
{
	char buf[ISTRING_MAX];

	if (libusb_get_string_descriptor_ascii(h, index, buf, sizeof buf) < 0)
	{
		return strerror(errno);
	}
	return strdup(buf);
}
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
 */
