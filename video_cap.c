
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <asm/types.h>

#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define CAP_BUFS	5

struct testbuffer
{
    unsigned char*	start;
    size_t 			offset;
    unsigned int 	length;
};

struct testbuffer cbufer[CAP_BUFS];

void init_cap(int fd)
{
	//v4l2_std_id std;
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	
	if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0)
        perror("VIDIOC_QUERYCAP");
	
	printf("=== CAP ===\n");
	printf("\t driver    [%s]\n"
			"\t card      [%s]\n"
			"\t bus_info  [%s]\n", 
			vcap.driver, vcap.card, vcap.bus_info);
		
	memset(&vfmt, 0, sizeof(vfmt));
	vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if (ioctl(fd, VIDIOC_G_FMT, &vfmt) < 0)
		perror("VIDIOC_G_FMT");
		
	printf("=== FMT ===\n");
	printf("\t Width  = %d\n", vfmt.fmt.pix.width);
	printf("\t Height = %d\n", vfmt.fmt.pix.height);
	printf("\t Image size = %d\n", vfmt.fmt.pix.sizeimage);
	printf("\t Pixel format = %c%c%c%c\n",
			(char)(vfmt.fmt.pix.pixelformat & 0xFF),
			(char)((vfmt.fmt.pix.pixelformat & 0xFF00) >> 8),
			(char)((vfmt.fmt.pix.pixelformat & 0xFF0000) >> 16),
			(char)((vfmt.fmt.pix.pixelformat & 0xFF000000) >> 24));
	
	struct v4l2_input input;
    input.index = 0;
    if (ioctl(fd, VIDIOC_ENUMINPUT, &input) != 0)
        printf("No matching index %d found\n", input.index);
    else
		printf("Name of input channel[%d] is %s\n", input.index, input.name);
	
#if 0
	//vfmt.fmt.pix.width = CAP_CX;
	//vfmt.fmt.pix.height = CAP_CY;
	vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
	if (ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0) {
		perror("VIDIOC_S_FMT");
		exit(1);
	}
#endif	
	struct v4l2_streamparm parm;
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 5;
	parm.parm.capture.timeperframe.denominator = 1;
	parm.parm.capture.capturemode = 0;
	if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0)
		perror("VIDIOC_S_PARM");
		
	if (ioctl(fd, VIDIOC_G_PARM, &parm) < 0)
		perror("VIDIOC_G_PARM");
		
	printf("=== FPS === %u/%u\n", parm.parm.capture.timeperframe.numerator,	
								  parm.parm.capture.timeperframe.denominator);
	
	struct v4l2_cropcap cropcap;
	memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_CROPCAP, &cropcap) < 0)
        perror("VIDIOC_CROPCAP");
/*	
	printf("=== cropcap ===\n");
	printf("  bounds.top    = %d\n", cropcap.bounds.top);
	printf("  bounds.left   = %d\n", cropcap.bounds.left);
	printf("  bounds.width  = %d\n", cropcap.bounds.width);
	printf("  bounds.height = %d\n", cropcap.bounds.height);
	printf("  defrect.top    = %d\n", cropcap.defrect.top);
	printf("  defrect.left   = %d\n", cropcap.defrect.left);
	printf("  defrect.width  = %d\n", cropcap.defrect.width);
	printf("  defrect.height = %d\n", cropcap.defrect.height);
*/
	
#if 0
	struct v4l2_crop crop;
	memset(&crop, 0, sizeof(crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.top = 0;
    crop.c.left = 0;
    crop.c.width = 320;
    crop.c.height = 240;
    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0)
        perror("VIDIOC_S_CROP");
#endif
	
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof (req));
    req.count  = 5;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		perror("VIDIOC_REQBUFS");
		exit(1);
	}
}

int begin_cap(int fd)
{
    int i;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;

    for (i = 0; i < CAP_BUFS; i++)
    {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return -1;
        }

        cbufer[i].length = buf.length;
        cbufer[i].offset = (size_t) buf.m.offset;
        cbufer[i].start  = mmap(NULL, cbufer[i].length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, cbufer[i].offset);
			
		memset(cbufer[i].start, 0xFF, cbufer[i].length);
    }

    for (i = 0; i < CAP_BUFS; i++)
    {
        memset(&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
		buf.m.offset = cbufer[i].offset;
        if (ioctl (fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return -1;
    }
    return 0;
}

struct v4l2_buffer cb;

void *get_cap(int fd)
{
	cb.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cb.memory 	= V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_DQBUF, &cb) < 0) {
		perror("VIDIOC_DQBUF");
		return NULL;
	}
	
	printf("DQBUF : [%d] [%p] [%d]\n", cb.index, cbufer[cb.index].start, cbufer[cb.index].length);
	
	return cbufer[cb.index].start;
}

void put_cap(int fd)
{
	if(ioctl(fd, VIDIOC_QBUF, &cb) < 0)
		perror("VIDIOC_QBUF");
}

void end_cap(int fd)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd,VIDIOC_STREAMOFF, &type) < 0)
		perror("VIDIOC_STREAMOFF");
}


void fb_show(int *fbm, int *rgb, int x, int y, int cx, int cy)
{
	int i;
	int off = y * 800 + x;
	int bs = 0;
	int len = cx << 2;
	
	for(i = 0; i < cy; i++) {
		memcpy(fbm + off, rgb + bs, len);
		off += 800;
		bs += cx;
	}
}

void ff_init()
{
	avcodec_register_all();
	av_register_all();
	avformat_network_init();
}

int main(int argc, char **argv)
{
	int res, got_output;
	int fds = open("/dev/fb0", O_RDWR);
	int fd = open("/dev/video3", O_RDWR);
	
	if(fd < 0) {
		perror("open /dev/video3 err");
		close(fds);
		exit(1);
	}
	
	struct fb_var_screeninfo vinfo;
	if (ioctl(fds, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("FBIOGET_VSCREENINFO");
		close(fds);
		close(fd);
		exit(1);
	}
	
	printf("framebuffer device info : %3d X %3d - %dbpp\n",
		vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	
	int *fbmem = (int *)mmap(0, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);	
	
	
	ff_init();
	
	AVFormatContext *fmtctx;
	AVStream *video_st;
	AVCodec *video_codec;
	AVCodecContext *video_cc;

	fmtctx = avformat_alloc_context();

	fmtctx->oformat = av_guess_format("rtp", NULL, NULL);
	sprintf(fmtctx->filename,"rtp://192.168.1.78:5810");
	avio_open(&fmtctx->pb, fmtctx->filename, AVIO_FLAG_WRITE);

	video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if(video_codec == NULL) {
		printf("Can't find H264 encoder\n");
		exit(1);
	}
	video_st = avformat_new_stream(fmtctx, video_codec);
	video_cc = video_st->codec;
	avcodec_get_context_defaults3(video_cc, video_codec);

	video_cc->codec_id = AV_CODEC_ID_H264;
	video_cc->bit_rate = 100000;
	//===========================
#if 0
	video_cc->rc_max_rate = 100000;
	video_cc->rc_min_rate = 100000;
	video_cc->bit_rate_tolerance = 100000;
	video_cc->rc_buffer_size = 100000;
	video_cc->rc_initial_buffer_occupancy = video_cc->rc_buffer_size * 3 / 4;
	video_cc->rc_buffer_aggressivity = (float)1.0;
	video_cc->rc_initial_cplx= 0.5; 
#else
	video_cc->flags |= CODEC_FLAG_QSCALE;
	video_cc->rc_min_rate = 10000;
	video_cc->rc_max_rate = 500000; 
	//video_cc->bit_rate = br;
#endif
	//===========================
	video_cc->width = 320;
	video_cc->height = 240;
	video_cc->time_base.num = 1;
	video_cc->time_base.den = 25;
	video_cc->gop_size = 12;
	video_cc->max_b_frames = 1;
	video_cc->pix_fmt = PIX_FMT_YUV420P;
	video_cc->qmin=3;
	video_cc->qmax=31;

	if(fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
		video_cc->flags|= CODEC_FLAG_GLOBAL_HEADER;

	av_opt_set(video_cc->priv_data, "preset", "superfast", 0);
	av_opt_set(video_cc->priv_data, "tune","zerolatency",0);
	//av_opt_set(video_cc->priv_data, "tune","stillimage,fastdecode,zerolatency",0);
	av_opt_set(video_cc->priv_data, "x264opts","crf=26:vbv-maxrate=728:vbv-bufsize=364:keyint=25",0);

	res = avcodec_open2(video_cc, video_codec, NULL);
	if(res < 0) {
		printf("Could not open codec : %d\n", res);
		exit(1);
	}
	
	AVFrame *yuv422Frame = av_frame_alloc();
	AVFrame *yuv420Frame = av_frame_alloc();
	AVPicture rgbPic;
	
	avpicture_alloc(&rgbPic, PIX_FMT_RGB32, 800, 480);
	
	yuv422Frame->format = AV_PIX_FMT_YUYV422;
	yuv422Frame->width  = 320;
	yuv422Frame->height = 240;
	//yuv422Frame->data[0] = pBuffer;
	yuv422Frame->linesize[0] = 320 * 2;
	
	yuv420Frame->format = PIX_FMT_YUV420P;
	yuv420Frame->width  = 320;
	yuv420Frame->height = 240;

	res = av_image_alloc(yuv420Frame->data, yuv420Frame->linesize, 320, 240, PIX_FMT_YUV420P, 32);
	if (res < 0) {
		printf("Could not allocate raw picture buffer\n");
		exit(1);
	}
	
	struct SwsContext *sws1 = sws_getContext(320, 240, AV_PIX_FMT_YUYV422, 320, 240, PIX_FMT_YUV420P, 2, NULL, NULL, NULL);
	struct SwsContext *sws2 = sws_getContext(320, 240, AV_PIX_FMT_YUYV422, 800, 480, PIX_FMT_RGB32, 2, NULL, NULL, NULL);
	
	AVPacket pkt;
	
	avformat_write_header(fmtctx, NULL);
	
	init_cap(fd);
	begin_cap(fd);
	//if(1) {
	while(1) {
		
		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;
		pkt.pts = AV_NOPTS_VALUE;
		pkt.dts = AV_NOPTS_VALUE;
		
		yuv422Frame->data[0] = get_cap(fd);
		
		sws_scale(sws1, yuv422Frame->data, yuv422Frame->linesize, 0, 240, yuv420Frame->data, yuv420Frame->linesize);
		sws_scale(sws2, yuv422Frame->data, yuv422Frame->linesize, 0, 240, rgbPic.data, rgbPic.linesize);

		yuv420Frame->pts = video_cc->frame_number;
		
		avpicture_layout(&rgbPic, PIX_FMT_RGB32, 800, 480, (unsigned char *)fbmem, 800 * 480 * 4);
		
		res = avcodec_encode_video2(video_cc, &pkt, yuv420Frame, &got_output);
		if (res < 0) {
			printf("Error encoding video frame : %d\n", res);
			av_free_packet(&pkt);
			break;
			
		}
		// If size is zero, it means the image was buffered.
		if (got_output) 
		{
			if (video_cc->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;

			pkt.stream_index = video_st->index;
			if (pkt.pts != AV_NOPTS_VALUE )
				pkt.pts = av_rescale_q(pkt.pts,video_st->codec->time_base, video_st->time_base);

			if(pkt.dts !=AV_NOPTS_VALUE )
				pkt.dts = av_rescale_q(pkt.dts,video_st->codec->time_base, video_st->time_base);

			// Write the compressed frame to the media file.
			res = av_interleaved_write_frame(fmtctx, &pkt);
		} 
		else {
			res = 0;
		}

		av_free_packet(&pkt);
		
		put_cap(fd);
		
		//printf(".");
		//fflush(stdout);
		
	}
	
	printf("\n");
	
	av_write_trailer(fmtctx);

	char sdp[2048];
	av_sdp_create(&fmtctx, 1, sdp, sizeof(sdp));
	printf("%s\n",sdp);
	
	sws_freeContext(sws1);
	sws_freeContext(sws2);
	
	avpicture_free(&rgbPic);
	
	end_cap(fd);
		
	munmap(fbmem, 800 * 480 * 4);

	close(fds);
	close(fd);
	return 0;
}
