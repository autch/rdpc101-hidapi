/*
 * test program for SUNTAC RDPC101.
 * See http://suntac.jp/products/usb/rdpc101.html
 *
 * required: hidapi
 * http://www.signal11.us/oss/hidapi/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rdpc101.h"

struct dev_info *get_dev_info(void);

__RCSID("$Id: rdpc-test.c,v 1.2 2009/07/07 13:33:53 nishio Exp $");

int flag_verbose = 0;
const char *program_name;

int main(int argc, char **argv)
{
	int ret;
	struct dev_info *dev_info;
	struct rdpc101_dev *rdpc101_list;
	struct rdpc101_dev *rp;
	int i;
	int size;
	void usage(void);
	unsigned char buf[64];

	if ((dev_info = get_dev_info()) == NULL )
	{
		perror("make_dev_info");
		exit(1);
	}

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

	if (!(rp = rdpc101_device(rdpc101_list, 0)))
	{
		fprintf(stderr, "invalid dev_index\n");
		rdpc101_cleanup(dev_info);
		exit(1);
	}

	memset(buf, 0, sizeof buf);
	for (i = 0; argv[i + 1] && i < sizeof buf; i++)
	{
		int tmp;

		sscanf(argv[i + 1], "%x", &tmp);
		buf[i] = tmp & 0xff;
	}
	size = i;
	for (i = 0; i < size; i++)
	{
		printf("%2.2x ", buf[i]);
	}
	ret = rdpc101_set_report(rp, buf, size);
	printf("=> %d\n", ret);
	if (ret < 0)
	{
		error_hidapi("ret", rp->handle);
	}
	rdpc101_update_state(rp);
	rdpc101_cleanup(dev_info);
	exit(ret < 0 ? 1 : 0);
}

struct dev_info *
get_dev_info(void)
{
	static struct dev_info dev_info =
	{ NULL, NULL };

	return &dev_info;
}
