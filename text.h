// (C) 2016 by www.vanheusden.com
#ifndef __TEXT_H__
#define __TEXT_H__

typedef enum
{
	none,
	xypos,
	upper_left,
	upper_center,
	upper_right,
	center_left,
	center_center,
	center_right,
	lower_left,
	lower_center,
	lower_right
} text_pos_t;

void add_text(unsigned char *img, int width, int height, char *text, int xpos, int ypos);
void print_timestamp(unsigned char *img, int width, int height, char *text, text_pos_t npos, int x, int y);

#endif
