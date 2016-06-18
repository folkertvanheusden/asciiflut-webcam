// (C) 2016 by www.vanheusden.com
#ifndef __SOURCE_H__
#define __SOURCE_H__

#include <pthread.h>
#include "text.h"

typedef struct
{
	int width, height;
	pthread_t t;
	unsigned char *result_buffer;
	int result_buffer_filled_n;
	pthread_mutex_t img_lock;
	int counter;
	pthread_mutex_t counter_lock;
	pthread_cond_t wait_cond;
	bool prefer_jpeg;
} source_t;

source_t * start_v4l2_thread(const char *dev, int *width, int *height, text_pos_t textpos, bool prefer_jpeg, bool rpi_workaround, int jpeg_quality);
void inc_users(source_t *s);
void dec_users(source_t *s);
void get_frame(source_t *s, unsigned char *dest, int *dest_size);
void get_frame_hls(source_t *s, unsigned char *dest);
void wait_for_frame(source_t *s);

#endif
