#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <iccom.h>

static void callback(enum Iccom_channel_number ch, uint32_t sz, uint8_t *buf)
{
	int i;

	printf("Received %u bytes: ", sz);

	for (i = 0; i < sz; i++) {
		if (buf[i] >= 0x20 && buf[i] < 0x80)
			printf("%c", buf[i]);
		else
			printf("\\x%02x", buf[i]);
	}

	printf("\n");
}

static uint8_t rbuf[ICCOM_BUF_MAX_SIZE], sbuf[ICCOM_BUF_MAX_SIZE];

int main(int argc, char *argv[])
{
	int ch, ret, len;
	Iccom_channel_t pch;
	Iccom_init_param ip;
	Iccom_send_param sp;

	if (argc > 1)
		ch = strtoul(argv[1], NULL, 0);
	else
		ch = 0;

	printf("ICCOM SAMPLE start, channel %d\n", ch);

	ip.channel_no = ch;
	ip.recv_buf = rbuf;
	ip.recv_cb = callback;

	ret = Iccom_lib_Init(&ip, &pch);
	if (ret != ICCOM_OK) {
		printf("Iccom_lib_Init error %d\n", ret);
		return 1;
	}

	len = snprintf((char *)sbuf, sizeof(sbuf), "Linux-ICCOM-TEST-SAMPLE-data");
	sp.send_size = len;
	sp.send_buf = sbuf;
	sp.channel_handle = pch;

	ret = Iccom_lib_Send(&sp);
	if (ret != ICCOM_OK) {
		printf("Iccom_lib_Send error %d\n", ret);
		return 1;
	}

	sleep(1);
	return 0;
}
