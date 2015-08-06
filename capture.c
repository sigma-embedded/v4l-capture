#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sysexits.h>
#include <unistd.h>

#include <getopt.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/media.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>

#include "capture.h"

#define ARRAY_SIZE(_a)	(sizeof(_a)/sizeof(_a)[0])

#define xHAVE_FMT_Y16	1

enum {
	CMDLINE_OPT_HELP = 0x1000,
};

static struct option const	CMDLINE_OPTIONS[] = {
	{ "mode",          required_argument, 0, 'm' },
	{ "out-fd",        required_argument, 0, 'O' },
	{ "filter",        required_argument, 0, 'f' },
	{ "stream-format", required_argument, 0, 'S' },
	{ "help",          no_argument,       0, CMDLINE_OPT_HELP },
	{ 0, 0, 0, 0}
};


static char const	NAME_IPU_ENTITY[] = "";
static char const	NAME_VIDEO_ENTITY[] = "";
static char const	NAME_SENSOR_ENTITY[] = "";

static void show_help(void)
{
	printf("Usage:  test-capture  [-m|--mode <mode>] [-O|--out-fd <fd>]\n"
	       "           [-f|--filter <filter>] [-S|--stream-format <fmt>]\n"
	       "           <display-mode>\n"
	       "\n"
	       "-m|--mode <mode>    0 - streaming (default)\n"
               "                    1 - snapshot\n"
	       "-O|--out-fd <fd>    filedescriptor for image data (default: 3)\n"
	       "<filter>            0 - none (default)\n"
               "                    1 - png (unimplemented)\n"
               "                    2 - jpeg\n"
	       "                    (useful for snapshot mode only)\n"
	       "-S|--stream-format <fmt>\n"
	       "                    0 - gstreamer-0.10 format\n"
	       "                    1 - gstreamer-1.x format\n"
	       "\n"
	       "<display-mode>      format <width>[ip]<freq>@<depth>\n"
	       "\n\n"
	       "Example:\n"
	       "  test-capture 1080p60@8  3>/dev/tcp/$ip/$port0\n");

	exit(0);
}

static int xclose(int fd)
{
	if (fd < 0)
		return 0;

	return close(fd);
}

static void freec(void const *p)
{
	free((void *)p);
}

struct gdp_header {
	uint8_t			ver_maj;
	uint8_t			ver_min;
	uint8_t			flags;
	uint8_t			pad;
	uint16_t		type;
	uint32_t		size;
	uint64_t		timestamp;
	uint64_t		duration;
	uint64_t		offset;
	uint64_t		end;
	uint16_t		buffer_flags; /* 42 */

	uint8_t			abi[14];

	uint16_t		crc_hdr; /* 58 */
	uint16_t		crc_payload; /* 60 */
} __attribute__((__packed__));

/** Writes all data
 *
 *  \retval true  all data have been written
 *  \retval false fd is not open */
static bool write_all(int fd, void const *buf, size_t len)
{
	while (len > 0) {
		ssize_t		l = write(fd, buf, len);

		assert(l != 0);
//		assert(l > 0 || errno == EINTR || errno == EBADF);

		if (l > 0) {
			buf += l;
			len -= l;
		} else if (l < 0 && errno == EINTR) {
			continue;
		} else if (l < 0 && (errno == EBADF || errno == EINVAL)) {
			return false;
		} else {
			perror("write()");
		}
	}

	return true;
}

static void fill_gdp_header(struct gdp_header *hdr, int type, size_t sz)
{
	_Static_assert(sizeof *hdr == 62, "bad gdp structure");

	memset(hdr, 0, sizeof *hdr);

	hdr->ver_maj = 1;
	hdr->ver_min = 0;
	hdr->flags = 0;

	hdr->type = htobe16(type);	/* GST_DP_PAYLOAD_BUFFER */
	hdr->size = htobe32(sz);
	hdr->end  = htobe64(sz);
}

static bool write_gdp_caps(int fd, struct media_info const *img)
{
	struct gdp_header	gdp_hdr;
	size_t			l = strlen(img->gst_cap) + 1;

	fill_gdp_header(&gdp_hdr, 2, l);
	return (write_all(fd, &gdp_hdr, sizeof gdp_hdr) &&
		write_all(fd, img->gst_cap, l));
}

static int open_by_devid(unsigned int major, unsigned int minor)
{
	char	fname[sizeof("/sys/dev/char/:") + 64];
	char	lname[256];
	char	*p;
	int	rc;

	sprintf(fname, "/sys/dev/char/%u:%u", major, minor);

	rc = readlink(fname, lname, sizeof lname - 1);
	if (rc < 0) {
		perror("readlink()");
		return -1;
	}
	lname[rc] = '\0';

	p = strrchr(lname, '/');
	if (!p || p < lname + sizeof("/dev")) {
		fprintf(stderr, "bad link for %u:%u: '%s'\n",
			major, minor, lname);
		return -1;
	}

	p -= 4;
	memcpy(p, "/dev", 4);

	rc = open(p, O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (rc < 0) {
		perror("open(<by-devid>)");
		return -1;
	}

	return rc;
}

static int open_entity(struct media_entity_desc	const *entity,
		       char const *name, int *fd)
{
	int	rc;

	if (strcmp(entity->name, name) != 0)
		return 0;

	if (*fd != -1) {
		fprintf(stderr, "entity %s already opened\n", name);
		return -1;
	}

	rc = open_by_devid(entity->v4l.major, entity->v4l.minor);
	if (rc < 0)
		return rc;

	*fd = rc;
	return 1;
}

static bool media_init(struct media_info *info)
{
	int				fd;
	int				rc;
	struct media_device_info	dev_info;
	unsigned int			last_id = 0;

	info->fd_ipu_dev = -1;
	info->fd_video_dev = -1;
	info->fd_sensor_dev = -1;

	fd = open("/dev/media0", O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("open(/dev/media0)");
		return -1;
	}

	rc = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &dev_info);
	if (rc < 0) {
		perror("ioctl(MEDIA_IOC_DEVICE_INFO)");
		goto out;
	}

	for (;;) {
		struct media_entity_desc	entity = {
			.id	= MEDIA_ENT_ID_FLAG_NEXT | last_id,
		};

		rc = ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
		if (rc < 0 && errno == EINVAL)
			break;

		if (rc < 0) {
			perror("ioctl(MEDIA_IOC_ENUM_ENTITIES)");
			goto out;
		}

		rc = open_entity(&entity, NAME_IPU_ENTITY, &info->fd_ipu_dev);
		if (rc == 0)
			rc = open_entity(&entity, NAME_VIDEO_ENTITY,
					 &info->fd_video_dev);

		if (rc == 0)
			rc = open_entity(&entity, NAME_SENSOR_ENTITY,
					 &info->fd_sensor_dev);

		if (rc < 0)
			goto out;

		last_id = entity.id;
	}

	rc = 0;

out:
	if (rc < 0) {
		xclose(info->fd_sensor_dev);
		xclose(info->fd_video_dev);
		xclose(info->fd_ipu_dev);
	}

	return rc == 0;
};

static bool streq(char const *a, char const *b)
{
	return strcmp(a, b) == 0;
}

static bool parse_fmt_x(struct v4l2_mbus_framefmt *fmt, char const *s,
			unsigned int *rate)
{
	char		*err;

	fmt->width = strtoul(s, &err, 10);
	if (*err != 'x')
		return false;

	s = err+1;

	fmt->height = strtoul(s, &err, 10);
	if (*err != '@')
		return false;

	s = err+1;

	fmt->field = V4L2_FIELD_NONE;


	if (strncmp(s, "y", 1) == 0) {
		unsigned int	bpp = strtoul(s+1, &err, 10);

		if (*err)
			return false;

		switch (bpp) {
		case 8:
			fmt->code = MEDIA_BUS_FMT_Y8_1X8;
			break;
		case 10:
			fmt->code = MEDIA_BUS_FMT_Y10_1X10;
			break;
		case 12:
			fmt->code = MEDIA_BUS_FMT_Y12_1X12;
			break;
#ifdef HAVE_FMT_Y16
		case 16:
			fmt->code = MEDIA_BUS_FMT_Y16_1X16;
			break;
#endif

		default:
			return false;
		}
	} else {
		return false;
	}

	*rate = 30;			/* todo */
	return true;
}

static bool parse_fmt(struct v4l2_mbus_framefmt *fmt, char const *s,
		      unsigned int *rate)
{
	if (!s)
		s = "1080p60@10";

	if (strchr(s, 'x'))
		return parse_fmt_x(fmt, s, rate);

	if (strncmp(s, "1080", 4) == 0) {
		fmt->width = 1920;
		fmt->height = 1080;
		s += 4;
	} else if (strncmp(s, "720", 3) == 0) {
		fmt->width = 1280;
		fmt->height = 720;
		s += 3;
	} else if (strncmp(s, "576", 3) == 0) {
		fmt->width = 720;
		fmt->height = 576;
		s += 3;
	} else if (strncmp(s, "480", 3) == 0) {
		fmt->width  = 720;
		fmt->height = 480;
		s += 3;
	} else {
		return false;
	}

	if (*s == 'p')
		fmt->field = V4L2_FIELD_NONE;
	else if (*s == 'i')
		fmt->field = V4L2_FIELD_SEQ_TB; /* TODO: really? */
	else
		return false;

	++s;

	if (*s == '@')
		*rate = 0;
	else
		*rate = atoi(s);

	s = strchr(s, '@');
	if (!s)
		s = "@10";

	++s;

	if (streq(s, "10"))
		fmt->code = V4L2_MBUS_FMT_YUYV10_1X20; /* TODO: verify whether
							* YUYV or YVYU */
	else if (streq(s, "8"))
		fmt->code = V4L2_MBUS_FMT_YUYV8_1X16; /* TODO: verify whether
						       * YUYV or YVYU */
	else
		return false;

	fmt->colorspace = V4L2_COLORSPACE_REC709;
	return true;
}

static bool set_format_on_pad(int fd, int pad,
			      struct v4l2_subdev_format const *tmpl)
{
	struct v4l2_subdev_format	fmt = *tmpl;
	int				rc;

	fmt.pad = pad;
	rc = ioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	if (rc < 0) {
		perror("ioctl(VIDIOC_SUBDEV_S_FMT)");
		return false;
	}

	return true;
}

static char const *gst_cap_grey(struct media_info const *info,
				struct v4l2_subdev_format const *fmt,
				unsigned int bpp)
{
	int		rc;
	char		*gst_cap = NULL;

	switch (info->out_fmt) {
	case OUTPUT_FMT_GST0:
		abort();		/* tood */
	case OUTPUT_FMT_GST1: {
		/* TODO */
		char const	*gst_fmt;

		gst_fmt = "video/x-raw, format=(string)";

		rc = asprintf(&gst_cap,
			      "%s%s, "
			      "width=(int)%u, height=(int)%u, "
			      "framerate=(fraction)%u/1, "
			      "bpp=(int)%d, depth=(int)%d",
			      gst_fmt,
			      bpp > 8 ? "GRAY16_BE" : "GRAY8",
			      fmt->format.width, fmt->format.height,
			      info->rate,
			      (bpp + 7) / 8 * 8, bpp);
		break;
	};

	default:
		rc = -1;
		break;
	}

	if (rc < 0)
		return NULL;

	return gst_cap;
}

static char const *gst_cap_yuyv(struct media_info const *info,
				struct v4l2_subdev_format const *fmt,
				unsigned int bpp)
{
	int		rc;
	char		*gst_cap = NULL;

	switch (info->out_fmt) {
	case OUTPUT_FMT_GST0:
	case OUTPUT_FMT_GST1: {
		char const	*gst_fmt;

		if (info->out_fmt == OUTPUT_FMT_GST0)
			gst_fmt = "video/x-raw-yuv, format=(fourcc)";
		else
			gst_fmt = "video/x-raw, format=(string)";

		rc = asprintf(&gst_cap,
			      "%s%s, "
			      "width=(int)%u, height=(int)%u, "
			      "framerate=(fraction)%u/1, "
			      "bpp=(int)%d, depth=(int)%d",
			      gst_fmt, "YUY2",
			      fmt->format.width, fmt->format.height,
			      info->rate,
			      (bpp + 7) / 8 * 8, bpp);
		break;
	}

	default:
		rc = -1;
		break;
	}

	if (rc < 0)
		return NULL;

	return gst_cap;
}

static bool set_format(struct media_info *info,
		       struct v4l2_subdev_format const *fmt)
{
	bool			rc;
	unsigned int		bpp;
	unsigned int		stride;
	char const *		(*gst_cap_fn)(struct media_info const *,
					      struct v4l2_subdev_format const *,
					      unsigned int bpp);
	char const		*gst_cap;

	struct v4l2_format	v4l_fmt = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt  = {
			.pix  = {
				/* there is some magic in the driver which
				 * translates INTERLACED to a matching
				 * INTERLACED_TB/BT */
				.field	= (fmt->format.field == V4L2_FIELD_NONE ?
					   V4L2_FIELD_NONE :
					   V4L2_FIELD_INTERLACED),
				.width	= fmt->format.width,
				.height	= fmt->format.height,
				/* TODO: remove hardcoded value */
				.pixelformat = info->fourcc,
			}
		}
	};

	switch (fmt->format.code) {
	case MEDIA_BUS_FMT_Y8_1X8:
		bpp    = 8;
		stride = 1;
		gst_cap_fn = gst_cap_grey;
		break;

	case MEDIA_BUS_FMT_Y10_1X10:
		bpp    = 10;
		stride = 1;
		gst_cap_fn = gst_cap_grey;
		break;

	case MEDIA_BUS_FMT_Y12_1X12:
		bpp    = 12;
		stride = 1;
		gst_cap_fn = gst_cap_grey;
		break;

#ifdef HAVE_FMT_Y16
	case MEDIA_BUS_FMT_Y16_1X16:
		bpp    = 16;
		stride = 1;
		gst_cap_fn = gst_cap_grey;
		break;
#endif

	case MEDIA_BUS_FMT_YUYV8_1X16:
		bpp    = 8;
		stride = 2;
		gst_cap_fn = gst_cap_yuyv;
		break;

	case MEDIA_BUS_FMT_YUYV10_1X20:
		bpp    = 10;
		stride = 2;
		gst_cap_fn = gst_cap_yuyv;
		break;

	default:
		assert(false);
	}

	stride *= ((bpp + 7) / 8) * fmt->format.width;

	v4l_fmt.fmt.pix.bytesperline = stride;
	v4l_fmt.fmt.pix.sizeimage = stride * fmt->format.height;

	rc = (set_format_on_pad(info->fd_ipu_dev, 0, fmt) &&
	      set_format_on_pad(info->fd_ipu_dev, 1, fmt));

	/* TODO: set format on sensor and video too? */

	if (!rc)
		return false;

	if (ioctl(info->fd_video_dev, VIDIOC_S_FMT, &v4l_fmt) < 0) {
		perror("ioctl(<VIDIOC_S_FMT>");
		return false;
	}

	if (!gst_cap_fn) {
		gst_cap = NULL;
	} else {
		gst_cap = gst_cap_fn(info, fmt, bpp);
		if (!gst_cap)
			return false;
	}

	info->width = fmt->format.width;
	info->height = fmt->format.height;
	info->bpp = bpp;
	info->stride = stride;
	info->size = stride * fmt->format.height;

	freec(info->gst_cap);
	info->gst_cap = gst_cap;

	return true;
}

struct v4lbuf {
	void			*mem;
	size_t			mem_len;
	struct v4l2_buffer	b;

	bool			queued;
};


static bool enqueue(int fd, struct v4lbuf *buf)
{
	int			rc;

	rc = ioctl(fd, VIDIOC_QBUF, &buf->b);
	if (rc < 0) {
		perror("ioctl(<VIDIOC_QBUF>)");
		return false;
	}

	return true;
}

static bool dequeue(int fd, struct v4l2_buffer *buf)
{
	int		rc;
	fd_set		fds;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	FD_SET(0,  &fds);

	select(fd + 1, &fds, NULL, NULL, NULL);

	if (FD_ISSET(fd, &fds)) {
		rc = ioctl(fd, VIDIOC_DQBUF, buf);
		if (rc < 0) {
			perror("ioctl(<VIDIOC_DQBUF>)");
			return false;
		}

		return true;
	}

	fprintf(stderr,
		"ioctl(<VIDIOC_DQBUF>) aborted by key; returning random data\n");
	buf->memory = 0;

	return true;
}

static bool stream_enable(int fd, bool ena)
{
	enum v4l2_buf_type	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int			rc;

	rc = ioctl(fd, ena ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type);

	if (rc < 0)
		perror(ena ? "ioctl(<VIDIOC_STREAMON>)" :
		       "ioctl(<VIDIOC_STREAMOFF>)");

	return rc >= 0;;
}

struct xmit_buffer {
	struct gdp_header	gdp;
	struct v4lbuf		*buf;
	size_t			pos;
	bool			hdr_sent;
};

static int send_buffer(int fd, struct xmit_buffer *buf)
{
	assert(buf->buf != NULL);

	for (;;) {
		void const	*mem;
		size_t		len;
		ssize_t		l;

		if (!buf->hdr_sent) {
			mem = &buf->gdp;
			len = sizeof buf->gdp;
		} else {
			mem = buf->buf->mem;
			len = buf->buf->mem_len;
		}

		assert(buf->pos < len);

		mem += buf->pos;
		len -= buf->pos;

		l = write(fd, mem, len);
		if (l < 0 && (errno == EBADF || errno == EINVAL)) {
			return 1;
		} else if (l < 0 && errno == EAGAIN) {
			return 0;
		} else if (l < 0) {
			perror("write(<stream-fd>)");
			return -1;
		} else if ((size_t)l < len) {
			buf->pos += l;
		} else if (buf->hdr_sent) {
			/* we were in payload which has been sent
			 * completely */
			return 1;
		} else {
			/* we were in hdr data; reset index counter and
			 * continue */
			buf->hdr_sent = true;
			buf->pos = 0;
		}
	}
}

static bool allocate_buffers(int fd, struct media_info const *info,
			     struct v4lbuf *buffers, size_t *cnt)
{
	size_t				i;

	struct v4l2_requestbuffers	req = {
		.count	= *cnt,
		.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory	= V4L2_MEMORY_MMAP,
	};

	if (ioctl(fd, VIDIOC_REQBUFS, &req)==-1) {
		perror("ioctl(<VIDIOC_REQBUFS>");
		return false;
	}

	for (i = 0; i < req.count; ++i) {
		struct v4l2_buffer		buf = {
			.type	= req.type,
			.memory	= req.memory,
			.index	= i,
		};

		if (ioctl(fd, VIDIOC_QUERYBUF, &buf)==-1) {
			perror("ioctl(<VIDIOC_QUERYBUF>)");
			break;
		}

		buffers[i].b = buf;
		/* TODO: calculate real value */
		buffers[i].mem_len = info->size;
		buffers[i].mem = mmap(0, buffers[i].mem_len, PROT_READ,
				      MAP_SHARED, fd, buf.m.offset);

		if (buffers[i].mem == MAP_FAILED) {
			perror("mmap()");
			break;
		}

		if (!enqueue(fd, &buffers[i]))
			break;
	}

	if (i != req.count) {
		/* TODO: free buffers */
		return false;
	}

	*cnt = req.count;

	return true;
}

static bool stream_v4l(int out_fd, struct media_info const *info)
{
	/* TODO: remove hardcoded variable */
	struct v4lbuf			buffers[10];
	int				fd = info->fd_video_dev;
	uint64_t			first_tm = 0;
	struct xmit_buffer		xmit;
	size_t				buf_cnt = ARRAY_SIZE(buffers);
	bool				dump_next = false;

	if (!allocate_buffers(fd, info, buffers, &buf_cnt))
		return false;

	write_gdp_caps(out_fd, info);

	stream_enable(fd, true);

	xmit.buf = NULL;

	for (;;) {
		struct v4l2_buffer	buf = {
			.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory	= V4L2_MEMORY_MMAP,
		};
		fd_set			fds_in;
		fd_set			fds_out;
		struct v4lbuf		*in_buf;

		FD_ZERO(&fds_in);
		FD_ZERO(&fds_out);
		FD_SET(0,  &fds_in);
		FD_SET(fd, &fds_in);

		if (xmit.buf)
			FD_SET(out_fd, &fds_out);

		select(fd+out_fd+1, &fds_in, &fds_out, NULL, NULL);

		if (FD_ISSET(0, &fds_in))
			break;

		if (FD_ISSET(out_fd, &fds_out)) {
			int	rc;

			rc = send_buffer(out_fd, &xmit);
			if (rc < 0)
				break;
			else if (rc == 0)
				; /* noop */
			else if (!enqueue(fd, xmit.buf))
				break;
			else
				xmit.buf = NULL;
		}

		if (!FD_ISSET(fd, &fds_in))
			in_buf = NULL;
		else if (!dequeue(fd, &buf))
			break;
		else
			in_buf = &buffers[buf.index];

		if (dump_next && in_buf) {
			uint16_t const	*d = in_buf->mem;

			printf("\n[%04x, %04x |  %04x, %04x | %04x, %04x]\n",
			       d[0], d[1],
			       d[2*info->stride/2], d[2*info->stride/2+1],
			       d[3*info->stride/2], d[3*info->stride/2+1]);

			dump_next = false;
		}

		if (in_buf == NULL)
			;		/* noop */
		else if (!xmit.buf)
			xmit.buf = in_buf;
		else if (!enqueue(fd, in_buf))
			break;
		else
			write(1, "-", 1);

		if (xmit.buf == in_buf && in_buf) {
			uint64_t			tm;

			fill_gdp_header(&xmit.gdp, 1, xmit.buf->mem_len);

			tm = buf.timestamp.tv_sec;
			tm *= 1000000ul;
			tm += buf.timestamp.tv_usec;
			tm *= 1000ul;

			if (first_tm == 0)
				first_tm = tm;

			xmit.hdr_sent = false;
			xmit.pos = 0;
			xmit.gdp.timestamp = htobe64(tm - first_tm);
			write(1, ".", 1);
		}
	}

	stream_enable(fd, false);

	return true;
}

static bool snapshot_v4l(int out_fd, struct media_info const *info)
{
	/* HACK: driver requires at least two buffers */
	struct v4lbuf			buffers[2];
	size_t				buf_cnt = ARRAY_SIZE(buffers);
	struct v4l2_buffer		buf = {
		.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory	= V4L2_MEMORY_MMAP,
	};

	int				fd = info->fd_video_dev;
	bool				rc;

	if (!allocate_buffers(fd, info, buffers, &buf_cnt))
		return false;

	stream_enable(fd, true);
	rc = dequeue(fd, &buf);
	stream_enable(fd, false);

	if (rc)
		rc = write_all(out_fd,
			       buffers[buf.index].mem,
			       buffers[buf.index].mem_len);

	return rc;
}

static int init_forking_filter(struct media_info *info, int out_fd,
			       filter_run_fn fn)
{
	int		fd_pipe[2];
	int		rc;

	rc = pipe2(fd_pipe, O_CLOEXEC);
	if (rc < 0) {
		perror("pipe2()");
		return -1;
	}

	info->pid_filter = fork();
	if (info->pid_filter < 0) {
		perror("fork()");
		close(fd_pipe[0]);
		close(fd_pipe[1]);
		return -1;
	}

	if (info->pid_filter > 0) {
		close(fd_pipe[0]);
		return fd_pipe[1];
	}

	close(fd_pipe[1]);
	close(info->fd_ipu_dev);
	close(info->fd_video_dev);
	close(info->fd_sensor_dev);

	rc = fn(info, out_fd, fd_pipe[0]);
	_exit(rc);
}

enum filter {
	FILTER_NONE,
	FILTER_PNG,
	FILTER_JPG,
};

static int init_filter(struct media_info *info, int out_fd, enum filter filter)
{
	switch (filter) {
	case FILTER_NONE:
		return out_fd;

	case FILTER_PNG:
		return init_forking_filter(info, out_fd, filter_png);

	case FILTER_JPG:
		return init_forking_filter(info, out_fd, filter_jpg);

	default:
		fprintf(stderr, "bad filter %d\n", filter);
		return -1;
	}
}

static enum media_stream_fmt parse_stream_fmt(char const *s)
{
	switch (atoi(s)) {
	case 0:
		return OUTPUT_FMT_GST0;
	case 1:
		return OUTPUT_FMT_GST1;
	default:
		fprintf(stderr, "bad stream format '%s'\n", s);
		exit(EX_USAGE);
	}
}

int main(int argc, char *argv[])
{
	struct media_info		info = { .gst_cap = NULL };
	struct v4l2_subdev_format	fmt = {
		.which	= V4L2_SUBDEV_FORMAT_ACTIVE,
		.pad	= 1,
	};
	int				opt_mode = 0;
	int				opt_fd = 3;
	int				opt_filter = 0;

	int				out_fd;

	while (1) {
		int	c = getopt_long(argc, argv, "m:O:f:S:",
					CMDLINE_OPTIONS, 0);

		if (c == -1)
			break;

		switch (c) {
		case 'm':	opt_mode = atoi(optarg); break;
		case 'O':	opt_fd = atoi(optarg); break;
		case 'f':	opt_filter = atoi(optarg); break;
		case 'S':	info.out_fmt = parse_stream_fmt(optarg); break;
		case CMDLINE_OPT_HELP:	show_help(); break;
		default:
			fprintf(stderr, "Try --help for more information\n");
			return EX_USAGE;
		}
	}

	if (!parse_fmt(&fmt.format, optind == argc ? NULL : argv[optind],
		       &info.rate)) {
		fprintf(stderr, "failed to parse format '%s'\n", argv[1]);
		return EX_USAGE;
	}

	if (!media_init(&info) ||
	    !set_format(&info, &fmt))
		return EX_OSERR;

	out_fd = init_filter(&info, opt_fd, opt_filter);

	fcntl(out_fd, F_SETFL,
	      ((fcntl(out_fd, F_GETFL) & ~(O_RDWR|O_RDONLY)) |
	       ((opt_mode == 0) ? O_NONBLOCK : 0) |
	       O_WRONLY));

	if (out_fd < 0)
		return EX_USAGE;

	if (opt_mode == 0)
		stream_v4l(out_fd, &info);
	else
		snapshot_v4l(out_fd, &info);

	close(out_fd);
}
