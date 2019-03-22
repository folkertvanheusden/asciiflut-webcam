// (C) 2016-2019 by www.vanheusden.com
#include <algorithm>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "source.h"
#include "utils-gfx.h"

void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	*out = (uint8_t *)malloc(wout * hout * 3);

	const int max_offset = wout * hout * 3 - 3;

	const double maxw = std::max(win, wout);
	const double maxh = std::max(hin, hout);

	const double hins = hin / maxh;
	const double wins = win / maxw;
	const double houts = hout / maxh;
	const double wouts = wout / maxw;

	for(int y=0; y<maxh; y++) {
		const int in_scaled_y = y * hins;
		const int in_scaled_o = in_scaled_y * win * 3;
		const int out_scaled_y = y * houts;
		const int out_scaled_o = out_scaled_y * wout * 3;

		for(int x=0; x<maxw; x++) {
			int ino = in_scaled_o + int(x * wins) * 3;
			int outo = out_scaled_o + int(x * wouts) * 3;

			outo = std::min(max_offset, outo);

			(*out)[outo + 0] = in[ino + 0];
			(*out)[outo + 1] = in[ino + 1];
			(*out)[outo + 2] = in[ino + 2];
		}
	}
}

void do_crop(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	*out = (uint8_t *)malloc(wout * hout * 3);

	for(int y=0; y<std::min(hin, hout); y++) {
		for(int x=0; x<std::min(win, wout); x++) {
			int ino = y * win * 3 + x * 3;
			int outo = y * wout * 3 + x * 3;

			(*out)[outo + 0] = in[ino + 0];
			(*out)[outo + 1] = in[ino + 1];
			(*out)[outo + 2] = in[ino + 2];
		}
	}
}

bool WRITE(const int fd, const char *what, int len)
{
	while(len > 0) {
		int rc = write(fd, what, len);

		if (rc <= 0) {
			printf("%s (%d)\n", strerror(errno), errno);
			return false;
		}

		len -= rc;
		what += rc;
	}

	return true;
}

void send_tcp_frame(int *const fd, const struct sockaddr_in & servaddr, const uint8_t *const resized, const int destw, const int desth, const int xo, const int yo)
{
	for(int y=0; y<desth; y++) {
		for(int x=0; x<destw; x++) {
			const uint8_t *p = &resized[y * destw * 3 + x * 3];

			int X = xo + x;
			int Y = yo + y;

			char buffer[128];
			int len = snprintf(buffer, sizeof buffer, "PX %d %d %02x%02x%02x\n", X, Y, p[0], p[1], p[2]);

			if (*fd == -1) {
				*fd = socket(AF_INET, SOCK_STREAM, 0);

				if (connect(*fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
					close(*fd);
					*fd = -1;
					continue;
				}
			}

			if (!WRITE(*fd, buffer, len)) {
				close(*fd);
				*fd = -1;
			}
		}
	}
}

void send_udp_frame(int *const fd, const struct sockaddr_in & servaddr, const uint8_t *const resized, const int destw, const int desth, const int xo, const int yo)
{
	char buffer[65536];

	buffer[0] = 0;
	buffer[1] = 0;

	int o = 2;
	for(int y=0; y<desth; y++) {
		for(int x=0; x<destw; x++) {
			const uint8_t *p = &resized[y * destw * 3 + x * 3];

			int X = xo + x;
			int Y = yo + y;

			buffer[o++] = X;
			buffer[o++] = X >> 8;
			buffer[o++] = Y;
			buffer[o++] = Y >> 8;
			buffer[o++] = p[0];
			buffer[o++] = p[1];
			buffer[o++] = p[2];

			if (o >= 1122 - 6) {
				sendto(*fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
				o = 2;
			}
		}
	}

	if (o > 2)
		sendto(*fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
}

void help()
{
	printf("(C) 2016-2019 by folkert@vanheusden.com\n");
	printf("released under agpl v3.0\n");
	printf("\n");
	printf("-d  video4linux device\n");
	printf("-W  width of the pixelflut device in pixels\n");
	printf("-H  height of the pixelflut device in pixels\n");
	printf("-x  x offset where to put the image in pixelflut\n");
	printf("-y  y offset where to put the image in pixelflut\n");
	printf("-t  IP address(! not a hostname) of the pixelflut server\n");
	printf("-p  port number\n");
	printf("-T  use TCP when sending to pixelflut server\n");
	printf("-I  ignore aspect ratio\n");
	printf("-C  do not resize, crop\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	const char *cam = "/dev/video0";
	const char *ip = nullptr;

	int pw = 64, ph = 32;
	int w = 640, h = 480;
	int xo = 0, yo = 0;
	bool tcp = false;
	int port = 5004;
	bool aspect_ratio = true, crop = false;

	int c = -1;
	while((c = getopt(argc, argv, "CId:W:H:x:y:t:Tp:h")) != -1) {
		switch(c) {
			case 'C':
				crop = true;
				break;

			case 'I':
				aspect_ratio = false;
				break;

			case 'p':
				port = atoi(optarg);
				break;

			case 'T':
				tcp = true;
				break;

			case 'd':
				cam = optarg;
				break;

			case 'W':
				pw = atoi(optarg);
				break;

			case 'H':
				ph = atoi(optarg);
				break;

			case 'x':
				xo = atoi(optarg);
				break;

			case 'y':
				yo = atoi(optarg);
				break;

			case 't':
				ip = optarg;
				break;

			case 'h':
				help();
				return 0;

			default:
				help();
				return 1;
		}
	}

	if (!ip) {
		printf("Please provide an ip-address (maybe even a port number) of your pixelflut display.\n\n");
		help();
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	source_t *s = start_v4l2_thread(cam, &w, &h, none, false, false, 75);
	inc_users(s);

	unsigned char *bytes = (unsigned char *)malloc(w * h * 3);

	double d1 = double(w) / pw;
	double d2 = double(h) / ph;
	double div = std::max(d1, d2);
	int destw = std::min(pw, int(w / div));
	int desth = std::min(ph, int(h / div));

	if (!aspect_ratio) {
		destw = pw;
		desth = ph;
	}

	if (crop) {
		div = std::min(d1, d2);
		destw = int(w / div);
		desth = int(h / div);
	}

	printf("target w/h: %dx%d\n", destw, desth);

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);

	int fd = -1;

	for(;;) {
		int len = 0;
		get_frame(s, bytes, &len);

		if (crop) {
			uint8_t *resized = NULL;
			do_resize(w, h, bytes, destw, desth, &resized);

			uint8_t *cropped = NULL;
			do_crop(destw, desth, resized, std::min(destw, pw), std::min(desth, ph), &cropped);
			free(resized);

			if (tcp)
				send_tcp_frame(&fd, servaddr, cropped, pw, ph, xo, yo);
			else 
				send_udp_frame(&fd, servaddr, cropped, pw, ph, xo, yo);

			free(cropped);
		}
		else {
			uint8_t *resized = NULL;
			do_resize(w, h, bytes, destw, desth, &resized);

			if (tcp)
				send_tcp_frame(&fd, servaddr, resized, destw, desth, xo, yo);
			else 
				send_udp_frame(&fd, servaddr, resized, destw, desth, xo, yo);

			free(resized);
		}
	}

	return 0;
}
