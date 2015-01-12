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

pthread_t 		aa_ptid;
pthread_cond_t 	aa_cond;
pthread_mutex_t aa_mutex;

struct av_cc {
	AVCodecContext * 	pCodecCtx;
	AVCodec * 			pCodec;
	AVFrame * 			pFrame;
	AVPacket *			pPacket;
};

void * audio_decoder_thread(void * param)
{
#if 0
	int len, got_frame, data_size;
	struct av_cc *pa = (struct av_cc *)param;
	
	while(1) {
		pthread_mutex_lock(&aa_mutex);
		pthread_cond_wait(&aa_cond, &aa_mutex);
		pthread_mutex_unlock(&aa_mutex);
		
		got_frame = 0;
	
		while (pa->pPacket->size > 0) {
			int len = avcodec_decode_audio4(pa->pCodecCtx, pa->pFrame, &got_frame, pa->pPacket);
				
			data_size = av_samples_get_buffer_size(NULL, pa->pCodecCtx->channels, 
					pa->pFrame->nb_samples, pa->pCodecCtx->sample_fmt, 1);
			
			if (len < 0) {
				printf("error\n");
				break;
			}
			
			if (got_frame > 0) {
			
			
			}
			
			if (pa->pPacket->data) {
				pa->pPacket->size -= len;
				pa->pPacket->data += len;
			}
		}
	}
#endif
	pthread_exit(NULL);
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
	
	int *fbmem = (int *)mmap(0, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);	
	
	avcodec_register_all();
	av_register_all();

	int i, videoStream, audioStream;
	
	struct av_cc aa_cc;
	struct av_cc vv_cc;
	
	AVFormatContext * 	pFormatCtx;
	AVPacket 			packet;
	AVPicture 			rgbPic;

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
	
	vv_cc.pCodecCtx = pFormatCtx->streams[videoStream]->codec; 
	vv_cc.pCodec = avcodec_find_decoder(vv_cc.pCodecCtx->codec_id);	
	if (vv_cc.pCodec == NULL) {
		printf("not find video decoder error\n");
		exit (1);
	}
	
	aa_cc.pCodecCtx = pFormatCtx->streams[audioStream]->codec; 
	aa_cc.pCodec = avcodec_find_decoder(aa_cc.pCodecCtx->codec_id);	
	if (vv_cc.pCodec == NULL) {
		printf("not find audio decoder error\n");
		exit (1);
	}

	// Inform the codec that we can handle truncated bitstreams
	// bitstreams where frame boundaries can fall in the middle of packets
	if (vv_cc.pCodec->capabilities & CODEC_CAP_TRUNCATED)
		vv_cc.pCodecCtx->flags |= CODEC_CAP_TRUNCATED;
		
	if (aa_cc.pCodec->capabilities & CODEC_CAP_TRUNCATED)
		aa_cc.pCodecCtx->flags |= CODEC_CAP_TRUNCATED;

	// 打开解码器
	if (avcodec_open2(vv_cc.pCodecCtx, vv_cc.pCodec, NULL) < 0) {
		printf("open video codec error\n");
		exit (1);
	}
	
	if (avcodec_open2(aa_cc.pCodecCtx, aa_cc.pCodec, NULL) < 0) {
		printf("open auido codec error\n");
		exit (1);
	}
	
	vv_cc.pFrame = av_frame_alloc();
	if(vv_cc.pFrame == NULL) {
        printf("alloc video frame failed!\n");
        exit (1);
    }
	aa_cc.pFrame = av_frame_alloc();
	if(aa_cc.pFrame == NULL) {
        printf("alloc video frame failed!\n");
        exit (1);
    }
	aa_cc.pPacket					= &packet;
	aa_cc.pFrame->nb_samples       	= aa_cc.pCodecCtx->frame_size;
    aa_cc.pFrame->format         	= aa_cc.pCodecCtx->sample_fmt;
    aa_cc.pFrame->channel_layout 	= aa_cc.pCodecCtx->channel_layout;
	aa_cc.pFrame->sample_rate	   	= aa_cc.pCodecCtx->sample_rate;
	av_frame_set_channels(aa_cc.pFrame, aa_cc.pCodecCtx->channels);
	//printf("======== audio : %s\n", av_get_sample_fmt_name(aa_cc.pCodecCtx->sample_fmt));
	
	// pthread_create(&aa_ptid, NULL, audio_decoder_thread, (void *)&aa_cc);
	
	snd_pcm_t *handle = pcm_init(aa_cc.pFrame->sample_rate);
	
	avpicture_alloc(&rgbPic, PIX_FMT_RGB32, 800, 480);
	
	struct SwsContext *sws = sws_getContext(vv_cc.pCodecCtx->width, 
			vv_cc.pCodecCtx->height,  vv_cc.pCodecCtx->pix_fmt, 
			800, 480, PIX_FMT_RGB32, 2, NULL, NULL, NULL);
			
#if 1
	struct SwrContext *swr;
	swr = swr_alloc();
	av_opt_set_int(swr, "in_channel_layout",  aa_cc.pCodecCtx->channel_layout, 0);
	av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swr, "in_sample_rate",     aa_cc.pCodecCtx->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate",    aa_cc.pCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt",  aa_cc.pCodecCtx->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
	swr_init(swr);
#endif
	
	int aa_desline;
	uint8_t **aa_des;
	av_samples_alloc_array_and_samples(&aa_des, &aa_desline, 2, aa_cc.pFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
	
	int len;
	int got_frame;
	int nframe = 0;
	
	struct timeval t1,t2;
	gettimeofday(&t1, NULL);
	
	while(av_read_frame(pFormatCtx, &packet) >= 0) {
		if (packet.stream_index == audioStream) {
			//pthread_cond_signal(&aa_cond);
			while (packet.size > 0) {
				len = avcodec_decode_audio4(aa_cc.pCodecCtx, aa_cc.pFrame, &got_frame, &packet);
				
				if (len < 0) {
					printf("error\n");
					break;
				}
				
				if (got_frame > 0) {
					
					i = swr_convert(swr, aa_des, aa_cc.pFrame->nb_samples, (const uint8_t **)aa_cc.pFrame->data, aa_cc.pFrame->nb_samples);
					i = snd_pcm_writei(handle, aa_des[0], aa_cc.pFrame->nb_samples);
					if (i == -EPIPE) {
						// EPIPE means underrun
						printf("underrun occurred\n");
						snd_pcm_prepare(handle);
					} else if (i < 0) {
						printf("error from writei: %s\n", snd_strerror(i));
						break;
					}
				}
				
				if (packet.data) {
					packet.size -= len;
					packet.data += len;
				}
			}
		} else if (packet.stream_index == videoStream) {

			while (packet.size > 0) {
				
				len = avcodec_decode_video2(vv_cc.pCodecCtx, vv_cc.pFrame, &got_frame, &packet);
				if (len < 0) {
					printf("Error while decoding frame %d\n", -1);
					continue;
				}
				if (got_frame) {
					nframe++;
					sws_scale(sws, (const uint8_t * const*)vv_cc.pFrame->data, vv_cc.pFrame->linesize, 0, 
						vv_cc.pFrame->width, rgbPic.data, rgbPic.linesize);
								
					avpicture_layout(&rgbPic, PIX_FMT_RGB32, 800, 480, 
						(unsigned char *)fbmem, 800 * 480 * 4);
						
					//printf(".");
					//fflush(stdout);
				}
				
				if (packet.data) {
					packet.size -= len;
					packet.data += len;
				}
			}
		}
	}
	
	gettimeofday(&t2, NULL);
	float ss = (t2.tv_usec - t1.tv_usec) / 1000000;
	ss += t2.tv_sec - t1.tv_sec;
	
	printf("Frames     : %d\n", nframe);
	printf("Start Time : %ld.%lds\n", t1.tv_sec, t1.tv_usec / 1000);
	printf("End Time   : %ld.%lds\n", t2.tv_sec, t2.tv_usec / 1000);
	printf("fps        : %.3f\n", (float)nframe / ss);
	
	//pthread_cancel(aa_ptid);
	//pthread_join(aa_ptid, NULL);
	
	av_freep(aa_des);
	swr_free(&swr);
	sws_freeContext(sws);
	
	avpicture_free(&rgbPic);
	
	munmap(fbmem, 800 * 480 * 4);
	close(fds);
	
	return 0;
}

