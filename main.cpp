// (C) 2016-2019 by www.vanheusden.com
#include <algorithm>
#include <math.h>
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
	printf("\n");
}

int main(int argc, char *argv[])
{
	const char *cam = "/dev/video0";
	const char *ip = "192.168.64.124";

	int pw = 64, ph = 32;
	int w = 640, h = 480;
	int xo = 0, yo = 0;
	bool tcp = false;
	int port = 5004;

	int c = -1;
	while((c = getopt(argc, argv, "d:W:H:x:y:t:Tp:h")) != -1) {
		switch(c) {
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


	source_t *s = start_v4l2_thread(cam, &w, &h, none, false, false, 75);
	inc_users(s);

	unsigned char *bytes = (unsigned char *)malloc(w * h * 3);

	unsigned char *result = (unsigned char *)malloc(pw * ph * 3);

	double d1 = double(w) / pw;
	double d2 = double(h) / ph;

	double div = std::max(d1, d2);
	int divi = ceil(div) / 2;

	int fd = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(ip);

	bool connected = false;

	char buffer[65536];
	memset(buffer, 0x00, sizeof buffer);

	for(;;) {
		int len = 0;
		get_frame(s, bytes, &len);

		for(int y=0; y<h; y += divi) {
			for(int x=0; x<w; x += divi) {
				int posx = x / div;
				int posy = y / div;

				unsigned char *tgt = &result[posy * pw * 3 + posx * 3];

				int r = 0, g = 0, b = 0;
				for(int Y=y; Y<y+divi; Y++)  {
					for(int X=x; X<x+divi; X++) {
						unsigned char *src = &bytes[Y * w * 3 + X * 3];

						if (X >= w)
							break;

						if (Y >= h)
							break;

						r += src[0];
						g += src[1];
						b += src[2];
					}
				}

				r /= divi * divi;
				g /= divi * divi;
				b /= divi * divi;

				tgt[0] = r;
				tgt[1] = g;
				tgt[2] = b;
			}
		}

		if (tcp) {
			if (!connected)
				connected = connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0;

			if (connected) {
				for(int y=0; y<h / div; y++) {
					for(int x=0; x<w / div; x++) {
						unsigned char *p = &result[y * pw * 3 + x * 3];

						int X = xo + x;
						int Y = yo + y;

						char buffer[128];
						int len = snprintf(buffer, sizeof buffer, "PX %d %d %02x%02x%02x\n",
								X, Y, p[0], p[1], p[2]);

						char *bp = buffer;
						while(len > 0) {
							int rc = write(fd, bp, len);

							if (rc <= 0) {
								close(fd);
								connected = false;
								break;
							}

							len -= rc;
							bp += rc;
						}
					}
				}
			}
		}
		else {
			buffer[0] = 0;
			buffer[1] = 0;
			int o = 2;
			for(int y=0; y<h / div; y++) {
				for(int x=0; x<w / div; x++) {
					unsigned char *p = &result[y * pw * 3 + x * 3];

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
						sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
						o = 2;
					}
				}
			}

			if (o > 2)
				sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
		}
	}

	return 0;
}
