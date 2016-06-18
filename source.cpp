// (C) 2016 by www.vanheusden.com
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "error.h"
#include "source.h"

#ifdef __GNUG__
	#define likely(x)       __builtin_expect((x),1)
	#define unlikely(x)     __builtin_expect((x),0)
#else
	#define likely(x)
	#define unlikely(x)
#endif

#define min(x, y)	((y) < (x) ? (y) : (x))
#define max(x, y)	((y) > (x) ? (y) : (x))

typedef struct
{
	int pixelformat;
	unsigned char *io_buffer;
	int fd;
	source_t *src;
	text_pos_t textpos;
} record_thread_parameters_t;

pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;

inline unsigned char clamp_to_uchar(int in)
{
	if (unlikely(in > 255))
		return 255;

	if (unlikely(in < 0))
		return 0;

	return in;
}

void image_yuv420_to_rgb(unsigned char *yuv420, unsigned char *rgb, int width, int height)
{
	int pos = width * height, outpos = 0;
	unsigned char *y_ptr = yuv420;
	unsigned char *u_ptr = yuv420 + pos + (pos >> 2);
	unsigned char *v_ptr = yuv420 + pos;

	for (int y=0; y<height; y += 2)
	{
		int u_pos = y * width >> 2;
		int y_w = y * width;
		int wm1 = width - 1;

		for (int x=0; x<width; x += 2)
		{
			int y_pos = y_w + x;

			/*first*/
			int v_pos = u_pos;
			int Y = y_ptr[y_pos];
			int U = ((u_ptr[u_pos] - 127) * 1865970) >> 20;
			int V = ((v_ptr[v_pos] - 127) * 1477914) >> 20;
			int R = V + Y;
			int B = U + Y;
			int G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			rgb[outpos++] = clamp_to_uchar(R);
			rgb[outpos++] = clamp_to_uchar(G);
			rgb[outpos++] = clamp_to_uchar(B);

			/*second*/
			y_pos++;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			rgb[outpos++] = clamp_to_uchar(R);
			rgb[outpos++] = clamp_to_uchar(G);
			rgb[outpos++] = clamp_to_uchar(B);

			/*third*/
			y_pos += wm1;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			rgb[outpos++] = clamp_to_uchar(R);
			rgb[outpos++] = clamp_to_uchar(G);
			rgb[outpos++] = clamp_to_uchar(B);

			/*fourth*/
			y_pos++;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			rgb[outpos++] = clamp_to_uchar(R);
			rgb[outpos++] = clamp_to_uchar(G);
			rgb[outpos++] = clamp_to_uchar(B);

			u_pos++;
		}
	}
}

void image_yuyv2_to_rgb(const unsigned char* src, int width, int height, unsigned char* dst)
{
	int len = width * height * 2;
	const unsigned char *end = &src[len];

	for(; src != end;)
	{
		int C = 298 * (*src++ - 16);
		int D = *src++ - 128;
		int D1 = 516 * D + 128;
		int C2 = 298 * (*src++ - 16);
		int E = *src++ - 128;

		int Ex = E * 409 + 128;
		int Dx = D * 100 + E * 208 - 128;

		*dst++ = clamp_to_uchar((C + Ex) >> 8);
		*dst++ = clamp_to_uchar((C - Dx) >> 8);
		*dst++ = clamp_to_uchar((C + D1) >> 8);

		*dst++ = clamp_to_uchar((C2 + Ex) >> 8);
		*dst++ = clamp_to_uchar((C2 - Dx) >> 8);
		*dst++ = clamp_to_uchar((C2 + D1) >> 8);
	}
}

void *record_thread(void *arg)
{
	record_thread_parameters_t *tp = (record_thread_parameters_t *)arg;

	int bytes = tp -> src -> width * tp -> src -> height * 3;
	unsigned char *conv_buffer = static_cast<unsigned char *>(malloc(bytes));

	for(;;)
	{
		struct v4l2_buffer buf = { 0 };

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

                if (ioctl(tp -> fd, VIDIOC_DQBUF, &buf) == -1)
			continue;

		pthread_mutex_lock(&tp -> src -> counter_lock);
		int client_counter = tp -> src -> counter;
		assert(client_counter >= 0);
		pthread_mutex_unlock(&tp -> src -> counter_lock);

		if (client_counter)
		{
			struct timeval tv;
			if (gettimeofday(&tv, NULL) == -1)
				error_exit(true, "gettimeofday failed");

			if (tp -> src -> prefer_jpeg)
			{
				int cur_n_bytes = buf.bytesused;

				pthread_mutex_lock(&tp -> src -> img_lock);
				memcpy(tp -> src -> result_buffer, tp -> io_buffer, cur_n_bytes);
				tp -> src -> result_buffer_filled_n = cur_n_bytes;
				pthread_mutex_unlock(&tp -> src -> img_lock);
			}
			else
			{
				if (tp -> pixelformat == V4L2_PIX_FMT_YUV420)
					image_yuv420_to_rgb(tp -> io_buffer, conv_buffer, tp -> src -> width, tp -> src -> height);
				else if (tp -> pixelformat == V4L2_PIX_FMT_YUYV)
					image_yuyv2_to_rgb(tp -> io_buffer, tp -> src -> width, tp -> src -> height, conv_buffer);
				else if (tp -> pixelformat == V4L2_PIX_FMT_RGB24)
					memcpy(conv_buffer, tp -> io_buffer, tp -> src -> width * tp -> src -> height * 3);
				else
					error_exit(false, "video4linux: video source has unsupported pixel format");

				if (tp -> textpos != none)
				{
					struct tm *ptm = localtime(&tv.tv_sec);

					char buffer[128] = { 0 };
					snprintf(buffer, sizeof buffer, "%04d-%02d-%02d %02d:%02d:%02d.%06d",
						ptm -> tm_year + 1900, ptm -> tm_mon + 1, ptm -> tm_mday,
						ptm -> tm_hour, ptm -> tm_min, ptm -> tm_sec,
						(int)tv.tv_usec);

					print_timestamp(conv_buffer, tp -> src -> width, tp -> src -> height, buffer, tp -> textpos, -1, -1);
				}

				pthread_mutex_lock(&tp -> src -> img_lock);
				memcpy(tp -> src -> result_buffer, conv_buffer, bytes);
				tp -> src -> result_buffer_filled_n = bytes;
				pthread_mutex_unlock(&tp -> src -> img_lock);
			}

			pthread_cond_broadcast(&tp -> src -> wait_cond);
		}

                if (ioctl(tp -> fd, VIDIOC_QBUF, &buf) == -1)
                        error_exit(true, "ioctl(VIDIOC_QBUF) failed");
	}

	return NULL;
}

bool try_v4l_configuration(int fd, int *width, int *height, unsigned int *format)
{
	unsigned int prev = *format;

	// determine pixel format: rgb/yuv420/etc
	struct v4l2_format fmt;
	memset(&fmt, 0x00, sizeof(fmt));
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
		error_exit(true, "ioctl(VIDIOC_G_FMT) failed");

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = *width;
	fmt.fmt.pix.height = *height;
	fmt.fmt.pix.pixelformat = *format;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
		return false;

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
		error_exit(true, "ioctl(VIDIOC_G_FMT) failed");

	*width = fmt.fmt.pix.width;
	*height = fmt.fmt.pix.height;

	if (fmt.fmt.pix.pixelformat != prev)
		return false;

	return true;
}

source_t * start_v4l2_thread(const char *dev, int *width, int *height, text_pos_t textpos, bool prefer_jpeg, bool rpi_workaround, int jpeg_quality)
{
	int fd = open(dev, O_RDWR);
	if (fd == -1)
		error_exit(true, "Cannot access %s", dev);

	// verify that it is a capture device
	struct v4l2_capability cap;
	memset(&cap, 0x00, sizeof(cap));

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
		error_exit(true, "Cannot VIDIOC_QUERYCAP on %s", dev);

	if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
		error_exit(false, "Device %s can't capture video frames", dev);

	// set capture resolution
	bool ok = false;
	unsigned int pixelformat = -1;

	if (prefer_jpeg)
	{
		pixelformat = V4L2_PIX_FMT_JPEG;
		if (try_v4l_configuration(fd, width, height, &pixelformat))
			ok = true;

		if (!ok)
		{
			pixelformat = V4L2_PIX_FMT_MJPEG;
			ok = try_v4l_configuration(fd, width, height, &pixelformat);
		}
	}
	else
	{
		pixelformat = V4L2_PIX_FMT_YUYV;
		ok = try_v4l_configuration(fd, width, height, &pixelformat);

		if (!ok)
		{
			pixelformat = V4L2_PIX_FMT_RGB24;
			ok = try_v4l_configuration(fd, width, height, &pixelformat);
		}
	}

	if (!ok)
		error_exit(true, "Selected pixel format not available (e.g. JPEG)\n");

	if (pixelformat == V4L2_PIX_FMT_JPEG)
	{
#ifdef V4L2_CID_JPEG_COMPRESSION_QUALITY
		struct v4l2_control par = { 0 };
		par.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
		par.value = jpeg_quality;

		if (ioctl(fd, VIDIOC_S_CTRL, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_S_CTRL) failed");
#else
		struct v4l2_jpegcompression par = { 0 };

		if (ioctl(fd, VIDIOC_G_JPEGCOMP, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_G_JPEGCOMP) failed");

		par.quality = jpeg_quality;

		if (ioctl(fd, VIDIOC_S_JPEGCOMP, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_S_JPEGCOMP) failed");
#endif
	}

	char buffer[5] = { 0 };
	memcpy(buffer, &pixelformat, 4);
	printf("%s\n", buffer);

	printf("%dx%d\n", *width, *height);

	// set how we retrieve data (using mmaped thingy)
	struct v4l2_requestbuffers req;
	memset(&req, 0x00, sizeof(req));
	req.count  = 1;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		error_exit(true, "ioctl(VIDIOC_REQBUFS) failed");

	struct v4l2_buffer buf;
	memset(&buf, 0x00, sizeof(buf));
	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = 0;
	if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
		error_exit(true, "ioctl(VIDIOC_QUERYBUF) failed");

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		error_exit(true, "ioctl(VIDIOC_QBUF) failed");

	enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &buf_type) == -1)
		error_exit(true, "ioctl(VIDIOC_STREAMON) failed");

	///// raspberry pi workaround
	// with 3.10.27+ kernel: image would fade to black due
	// to exposure bug
	if (rpi_workaround)
	{
		int sw = 1;
		if (ioctl(fd, VIDIOC_OVERLAY, &sw) == -1)
			error_exit(true, "ioctl(VIDIOC_OVERLAY) failed");
	}

	record_thread_parameters_t *tp = new record_thread_parameters_t;
	tp -> pixelformat = pixelformat;
	tp -> io_buffer = static_cast<unsigned char *>(mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset));
	tp -> fd = fd;
	tp -> textpos = textpos;

	source_t *s = new source_t;
	s -> result_buffer = static_cast<unsigned char *>(malloc(*width * *height * 3));
	s -> result_buffer_filled_n = 0;
	s -> width = *width;
	s -> height = *height;
	s -> prefer_jpeg = prefer_jpeg;
	pthread_cond_init(&s -> wait_cond, NULL);

	tp -> src = s;

	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

	int rc = -1;
	if ((rc = pthread_create(&s -> t, &tattr, record_thread, tp)) != 0)
	{
		errno = rc;
		error_exit(true, "pthread_create failed (v4l2 thread)");
	}

	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);

	pthread_mutex_init(&s -> img_lock, &mutex_attr);

	s -> counter = 0;
	pthread_mutex_init(&s -> counter_lock, &mutex_attr);

	return s;
}

void inc_users(source_t *s)
{
	pthread_mutex_lock(&s -> counter_lock);
	s -> counter++;
	pthread_mutex_unlock(&s -> counter_lock);
}

void dec_users(source_t *s)
{
	pthread_mutex_lock(&s -> counter_lock);
	s -> counter--;
	pthread_mutex_unlock(&s -> counter_lock);
}

void get_frame(source_t *s, unsigned char *dest, int *dest_size)
{
	pthread_mutex_lock(&s -> img_lock);

	memcpy(dest, s -> result_buffer, s -> result_buffer_filled_n);
	*dest_size = s -> result_buffer_filled_n;

	pthread_mutex_unlock(&s -> img_lock);
}

void get_frame_hls(source_t *s, unsigned char *dest)
{
	int pixels = s -> width * s -> height;

	pthread_mutex_lock(&s -> img_lock);

	unsigned char *in = s -> result_buffer;

	for(int index=0; index<pixels; index++)
	{
		double r = *in++;
		double g = *in++;
		double b = *in++;

		r /= 255.0;
		g /= 255.0;
		b /= 255.0;

		double cmax = max(r, max(g, b));
		double cmin = min(r, min(g, b));

		double L = (cmax + cmin) / 2.0, S = 0, H = 0;

		if (cmax == cmin)
			S = H = 0.0; /* undefined */
		else
		{
			if (L < 0.5)
				S = (cmax - cmin) / (cmax + cmin);
			else
				S = (cmax - cmin) / (2.0 - cmax - cmin);

			double delta = cmax - cmin;

			if (r == cmax)
				H = (g - b) / delta;
			else if (g == cmax)
				H = 2.0 + (b - r) / delta;
			else
				H = 4.0 + (r - g) / delta;

			H /= 6.0;

			if (H < 0.0)
				H += 1.0;
		}

		*dest++ = int(H * 255.0);
		*dest++ = int(L * 255.0);
		*dest++ = int(S * 255.0);
	}

	pthread_mutex_unlock(&s -> img_lock);
}

void wait_for_frame(source_t *s)
{
	pthread_mutex_lock(&condition_mutex);

	pthread_cond_wait(&s -> wait_cond, &condition_mutex);

	pthread_mutex_unlock(&condition_mutex); // not used
}
