// (C) 2016 by www.vanheusden.com
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "source.h"
#include "utils-gfx.h"

int main(int argc, char *argv[])
{
	const char *cam = "/dev/video0";
	if (argc == 2)
		cam = argv[1];

	int w = 320, h = 240;
	source_t *s = start_v4l2_thread(cam, &w, &h, none, false, false, 75);
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

				unsigned char *tgt = &result[posy * 64 * 3 + posx * 3];

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

#if ADJUST_COLORS
				double H, L, S;
				rgb_to_hls(double(r) / 255, double(g) / 255, double(b) / 255, &H, &L, &S);
				L = 0.5;
				//S = 1.0;
				double R, G, B;
				hls_to_rgb(H, L, S, &R, &G, &B);

				tgt[0] = R * 255.0;
				tgt[1] = G * 255.0;
				tgt[2] = B * 255.0;
#else
				tgt[0] = r;
				tgt[1] = g;
				tgt[2] = b;
#endif
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

		sleep(1);
		//usleep(1000000/3);
	}

	return 0;
}
