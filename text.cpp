// (C) 2016 by www.vanheusden.com
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error.h"
#include "font.h"
#include "text.h"

void add_text(unsigned char *img, int width, int height, char *text, int xpos, int ypos)
{
	int loop, x, y;
	int len = strlen(text);

	for(loop=0; loop<len; loop++)
	{
		for(y=0; y<8; y++)
		{
			for(x=0; x<8; x++)
			{
				int cur_char = text[loop];
				int realx = xpos + x + 8 * loop, realy = ypos + y;
				int offset = (realy * width * 3) + (realx * 3);

				if (realx >= width || realx < 0 || realy >= height || realy < 0)
					break;

				if (cur_char < 32 || cur_char > 126)
					cur_char = 32;

				img[offset + 0] = font[cur_char][y][x];
				img[offset + 1] = font[cur_char][y][x];
				img[offset + 2] = font[cur_char][y][x];
			}
		}
	}
}

void print_timestamp(unsigned char *img, int width, int height, char *text, text_pos_t n_pos, int x, int y)
{
	time_t now = time(NULL);
	struct tm *ptm = localtime(&now);
	int len = strlen(text), new_len;
	int bytes = len + 4096;
	char *text_out = (char *)malloc(bytes);
	if (!text_out)
		error_exit(true, "out of memory while allocating %d bytes", bytes);

	strftime(text_out, bytes, text, ptm);
	new_len = strlen(text_out);

	if (n_pos == upper_left || n_pos == upper_center || n_pos == upper_right)
		y = 1;
	else if (n_pos == center_left || n_pos == center_center || n_pos == center_right)
		y = (height / 2) - 4;
	else if (n_pos == lower_left || n_pos == lower_center || n_pos == lower_right)
		y = height - 9;

	if (n_pos == upper_left || n_pos == center_left || n_pos == lower_left)
	{
		x = 1;
	}
	else if (n_pos == upper_center || n_pos == center_center || n_pos == lower_center)
	{
		x = (width / 2) - (new_len / 2) * 8;
	}
	else if (n_pos == upper_right || n_pos == center_right || n_pos == lower_right)
	{
		x = width - new_len * 8;
	}

	add_text(img, width, height, text_out, x, y);

	free(text_out);
}
