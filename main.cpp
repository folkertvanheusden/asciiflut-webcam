// (C) 2016 by www.vanheusden.com
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "source.h"

int main(int argc, char *argv[])
{
	int w = 320, h = 240;
	source_t *s = start_v4l2_thread("/dev/video0", &w, &h, none, false, false, 75);
	inc_users(s);

	unsigned char *bytes = (unsigned char *)malloc(w * h * 3);

	unsigned char *result = (unsigned char *)malloc(64 * 32 * 3);
	unsigned char *prev = (unsigned char *)malloc(64 * 32 * 3);

	double d1 = double(w) / 64;
	double d2 = double(h) / 32;

	double div = std::max(d1, d2);
	int divi = ceil(div) / 2;

	for(;;) {
		get_frame_hls(s, bytes);

		for(int y=0; y<h; y += divi) {
			for(int x=0; x<w; x += divi) {
				int posx = x / div;
				int posy = y / div;

				result[posy * 64 * 3 + posx * 3 + 0] = bytes[y * w * 3 + x * 3 + 0];
				result[posy * 64 * 3 + posx * 3 + 1] = bytes[y * w * 3 + x * 3 + 1];
				result[posy * 64 * 3 + posx * 3 + 2] = bytes[y * w * 3 + x * 3 + 2];
			}
		}

		for(int y=0; y<32; y++) {
			for(int x=0; x<64; x++) {
				unsigned char *p = &result[y * 64 * 3 + x * 3];
				unsigned char *pp = &prev[y * 64 * 3 + x * 3];

				if (memcmp(p, pp, 3))
					printf("PX %d %d %02x%02x%02x\n", x, y, p[0], p[1], p[2]);
			}
		}

		memcpy(prev, result, 64 * 32 * 3);

		//sleep(1);
		usleep(1000000/3);
	}

	return 0;
}
