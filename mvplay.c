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

#include <pthread.h>

#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <math.h>
#include <alsa/asoundlib.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

// avcodec_decode_video2 : 14ms
// sws_scale and avpicture_layout : 27ms

struct av_cc {
	AVCodecContext * 	pCodecCtx;
	AVCodec * 			pCodec;
	AVFrame * 			pFrame;
	AVPacket *			pPacket;
	void *				sw;
};

#define AUDIO_MAX	30
struct audio_dec {
	AVPacket	pkt[AUDIO_MAX];
	int 	 	count;
	int 		pos;
};

unsigned char *	fb_mem;
uint8_t **		aa_des;

snd_pcm_t *handle;

pthread_t 		aa_ptid;
pthread_t 		vv_ptid;
pthread_cond_t 	aa_cond;
pthread_mutex_t aa_mutex;

struct av_cc a;
struct av_cc v;
struct audio_dec adc;

void * vv_display_thread(void * param)
{
	AVPicture *p = (AVPicture *)param;
	
	while(1) {
		sws_scale((struct SwsContext *)v.sw, (const uint8_t * const*)v.pFrame->data, 
				v.pFrame->linesize, 0, v.pFrame->width, p->data, p->linesize);
	
	}
	pthread_exit(NULL);
}

void * aa_playback_thread(void * param) 
{
	int len, got_frame, res;
	while(1) {
		while(adc.count <= 0) usleep(2000);
		
		while(adc.count) {
			
			while (adc.pkt[adc.pos].size > 0) {
				got_frame = 0;
				
				len = avcodec_decode_audio4(a.pCodecCtx, a.pFrame, &got_frame, &adc.pkt[adc.pos]);
					
				if (len < 0) {
					printf("avcodec_decode_audio4 error\n");
					break;
				}
				
				if (got_frame > 0) {
					res = swr_convert((struct SwrContext*)a.sw, aa_des, a.pFrame->nb_samples, 
						(const uint8_t **)a.pFrame->data, a.pFrame->nb_samples);
					res = snd_pcm_writei(handle, aa_des[0], a.pFrame->nb_samples);
					if (res == -EPIPE) {
						printf("underrun occurred : %s\n", strerror(res));
						snd_pcm_prepare(handle);
					} else if (res < 0) {
						printf("error from writei: %s\n", snd_strerror(res));
						break;
					}
				}
				
				adc.pkt[adc.pos].size -= len;
				adc.pkt[adc.pos].data += len;
			}
			
			av_free_packet(&adc.pkt[adc.pos]);
			
			adc.pos++;
			adc.pos %= AUDIO_MAX;
			adc.count--;
		
		}
	}
	pthread_exit(NULL);

}

void av_convert_init(struct SwrContext **pswr, struct SwsContext **psws)
{
	*pswr = swr_alloc();
	av_opt_set_int(*pswr, "in_channel_layout",  a.pCodecCtx->channel_layout, 0);
	av_opt_set_int(*pswr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(*pswr, "in_sample_rate",     a.pCodecCtx->sample_rate, 0);
	av_opt_set_int(*pswr, "out_sample_rate",    a.pCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(*pswr, "in_sample_fmt",  a.pCodecCtx->sample_fmt, 0);
	av_opt_set_sample_fmt(*pswr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
	swr_init(*pswr);
	
	*psws = sws_getContext( v.pCodecCtx->width, 
							v.pCodecCtx->height, 
							v.pCodecCtx->pix_fmt, 
							800, 480, PIX_FMT_RGB32, 
							2, NULL, NULL, NULL);
	
}

snd_pcm_t *pcm_init(int bitrate)
{
	int res;
	int dir;
	unsigned int val;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;

	res = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (res < 0) {
		printf("unable to open pcm device: %s\n", snd_strerror(res));
		return NULL;
	}

	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(handle, params, 2);

	val = bitrate - 1;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	
	frames = 32;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	res = snd_pcm_hw_params(handle, params);
	if (res < 0) {
		printf("unable to set hw parameters: %s\n", snd_strerror(res));
		snd_pcm_close(handle);
		return NULL;
	}
	
	return handle;
}

int main(int argc, char *argv[])
{
	if(argc != 2) {
		printf("  Usage : %s [mv_file]\n", argv[0]);
		exit(1);
	}
	
	pthread_cond_init(&aa_cond, NULL);
	pthread_mutex_init(&aa_mutex, NULL);

	int fds = open("/dev/fb0", O_RDWR);
	struct fb_var_screeninfo vinfo;
	if (ioctl(fds, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("FBIOGET_VSCREENINFO");
		close(fds);
		exit(1);
	}
	printf("framebuffer device info : %3d X %3d - %dbpp\n",
		vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	
	fb_mem = (unsigned char *)mmap(0, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);	
	
	int i, res;
	
	AVFormatContext * 	pFormatCtx;
	AVPacket 			packet;
	AVPicture 			rgbPic;
	
	struct SwsContext *sws;
	struct SwrContext *swr;
	
	int videoStream;
	int audioStream;
	
	avcodec_register_all();
	av_register_all();

	pFormatCtx = avformat_alloc_context();
	
	// 打开输入文件
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		printf("can,t open file");
		return -1;
	}
	
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1;

	av_dump_format(pFormatCtx, 0, argv[1], 0); // 显示pfmtctx->streams里的信息 测试时用到
	
	audioStream = -1;
	videoStream = -1;
	//找到音频、视频对应的stream
	for (i = 0; i < pFormatCtx->nb_streams; ++i)  
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			videoStream = i;
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			audioStream = i;
	}
	
	if (videoStream < 0 || audioStream < 0) {
		printf("no video or audio stream\n");
		exit (1);
	}
	
	v.pCodecCtx = pFormatCtx->streams[videoStream]->codec; 
	v.pCodec = avcodec_find_decoder(v.pCodecCtx->codec_id);	
	if (v.pCodec == NULL) {
		printf("not find video decoder error\n");
		exit (1);
	}
	
	a.pCodecCtx = pFormatCtx->streams[audioStream]->codec; 
	a.pCodec = avcodec_find_decoder(a.pCodecCtx->codec_id);	
	if (a.pCodec == NULL) {
		printf("not find audio decoder error\n");
		exit (1);
	}

	// Inform the codec that we can handle truncated bitstreams
	// bitstreams where frame boundaries can fall in the middle of packets
	if (v.pCodec->capabilities & CODEC_CAP_TRUNCATED)
		v.pCodecCtx->flags |= CODEC_CAP_TRUNCATED;
		
	if (a.pCodec->capabilities & CODEC_CAP_TRUNCATED)
		a.pCodecCtx->flags |= CODEC_CAP_TRUNCATED;

	// 打开解码器
	if (avcodec_open2(v.pCodecCtx, v.pCodec, NULL) < 0) {
		printf("open video codec error\n");
		exit (1);
	}
	
	if (avcodec_open2(a.pCodecCtx, a.pCodec, NULL) < 0) {
		printf("open auido codec error\n");
		exit (1);
	}
	
	v.pFrame = av_frame_alloc();	
	a.pFrame = av_frame_alloc();
	
	v.pPacket					= &packet;
	//a.pPacket					= &packet;
	a.pFrame->nb_samples       	= a.pCodecCtx->frame_size;
    a.pFrame->format         	= a.pCodecCtx->sample_fmt;
    a.pFrame->channel_layout 	= a.pCodecCtx->channel_layout;
	a.pFrame->sample_rate	   	= a.pCodecCtx->sample_rate;
	av_frame_set_channels(a.pFrame, a.pCodecCtx->channels);
	
	//avpicture_alloc(&rgbPic, PIX_FMT_RGB32, 800, 480);
	rgbPic.data[0] = fb_mem;
	rgbPic.linesize[0] = 3200;
	
	av_samples_alloc_array_and_samples(&aa_des, &res, 2, 
			a.pFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
	
	//handle = pcm_init(a.pCodecCtx->sample_rate);
	av_convert_init(&swr, &sws);
	
	a.sw = (void *)swr;
	v.sw = (void *)sws;
	
	adc.count = 0;
	adc.pos = 0;
	
	//pthread_create(&vv_ptid, NULL, vv_display_thread, (void *)&rgbPic);
	//pthread_create(&aa_ptid, NULL, aa_playback_thread, NULL);
	usleep(50000);
	
	int got_frame, len, idx;
	
	while(av_read_frame(pFormatCtx, &packet) >= 0) {
		if (packet.stream_index == audioStream) {
		
		/*
			//while(adc.count >= AUDIO_MAX) usleep(1000);
			
			//idx = (adc.pos + adc.count) % AUDIO_MAX;
			
			//memcpy(&adc.pkt[idx], &packet, sizeof(packet));
			
			//adc.count++;
		*/
			
		} else if (packet.stream_index == videoStream) {
			
			while (v.pPacket->size > 0) {
			
				len = avcodec_decode_video2(v.pCodecCtx, v.pFrame, &got_frame, v.pPacket);
				
				if (len < 0) {
					printf("Error while decoding frame %d\n", len);
					break;
				}
				
				if (v.pPacket->data) {
					v.pPacket->size -= len;
					v.pPacket->data += len;
				}
				
				if (got_frame) {

					sws_scale((struct SwsContext *)v.sw, (const uint8_t * const*)v.pFrame->data, 
						v.pFrame->linesize, 0, v.pFrame->width, rgbPic.data, rgbPic.linesize);

				}
			}
			
			av_free_packet(&packet);
		}
		
		
	}
	
	//pthread_cancel(vv_ptid);
	//pthread_cancel(aa_ptid);
	//pthread_join(vv_ptid, NULL);
	//pthread_join(aa_ptid, NULL);
	
	//snd_pcm_drain(handle);
	//snd_pcm_close(handle);
	
	//av_freep(aa_des);
	swr_free(&swr);
	sws_freeContext(sws);
	
	munmap(fb_mem, 800 * 480 * 4);
	close(fds);
	
	return 0;
}

