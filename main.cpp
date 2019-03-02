// (C) 2016 by www.vanheusden.com
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

int main(int argc, char *argv[])
{
	const char *cam = "/dev/video0";
	if (argc == 2)
		cam = argv[1];

	int pw = 64, ph = 32;
	int w = 320, h = 240;
	source_t *s = start_v4l2_thread(cam, &w, &h, none, false, false, 75);
	inc_users(s);

	unsigned char *bytes = (unsigned char *)malloc(w * h * 3);

	unsigned char *result = (unsigned char *)malloc(pw * ph * 3);

	double d1 = double(w) / pw;
	double d2 = double(h) / ph;

	double div = std::max(d1, d2);
	int divi = ceil(div) / 2;

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(5004);
	//servaddr.sin_addr.s_addr = inet_addr("10.208.42.159");
	servaddr.sin_addr.s_addr = inet_addr("192.168.64.124");

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

		buffer[0] = 1;
		buffer[1] = 0;
		int o = 2;
		for(int y=0; y<ph; y++) {
			for(int x=0; x<pw; x++) {
				unsigned char *p = &result[y * pw * 3 + x * 3];

				buffer[o++] = x;
				buffer[o++] = x >> 8;
				buffer[o++] = y;
				buffer[o++] = y >> 8;
				buffer[o++] = p[0];
				buffer[o++] = p[1];
				buffer[o++] = p[2];
			}
		}

		if (o) {
			// printf("%d\n", o);
			//			printf("%s\n", buffer);
			sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
		}
	}

	return 0;
}
