#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <math.h>
#include <alsa/asoundlib.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>  
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#define INBUF_SIZE 			4096
#define AUDIO_INBUF_SIZE 	20480
#define AUDIO_REFILL_THRESH 4096

void audio_decode_file(const char *outfilename, const char *filename)
{
    AVCodec *codec;
    AVCodecContext *c= NULL;
    int len;
    FILE *f, *outfile;
    uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    AVPacket avpkt;
    AVFrame *decoded_frame = NULL;

    av_init_packet(&avpkt);

    printf("Decode audio file %s to %s\n", filename, outfilename);

    /* find the mpeg audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (!codec) {
        printf("Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        printf("Could not allocate audio codec context\n");
        exit(1);
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        printf("Could not open codec\n");
        exit(1);
    }

    f = fopen(filename, "rb");
    if (!f) {
        printf("Could not open %s\n", filename);
        exit(1);
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(c);
        exit(1);
    }

    /* decode until eof */
    avpkt.data = inbuf;
    avpkt.size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);

    while (avpkt.size > 0) {
        int got_frame = 0;

        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
                printf("Could not allocate audio frame\n");
                exit(1);
            }
        }

        len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &avpkt);
        if (len < 0) {
            printf("Error while decoding\n");
            exit(1);
        }
        if (got_frame) {
            /* if a frame has been decoded, output it */
            int data_size = av_samples_get_buffer_size(NULL, c->channels,
                                                       decoded_frame->nb_samples,
                                                       c->sample_fmt, 1);
            if (data_size < 0) {
                /* This should not occur, checking just for paranoia */
                printf("Failed to calculate data size\n");
                exit(1);
            }
            fwrite(decoded_frame->data[0], 1, data_size, outfile);
        }
        avpkt.size -= len;
        avpkt.data += len;
        avpkt.dts =
        avpkt.pts = AV_NOPTS_VALUE;
        if (avpkt.size < AUDIO_REFILL_THRESH) {
            /* Refill the input buffer, to avoid trying to decode
             * incomplete frames. Instead of this, one could also use
             * a parser, or use a proper container format through
             * libavformat. */
            memmove(inbuf, avpkt.data, avpkt.size);
            avpkt.data = inbuf;
            len = fread(avpkt.data + avpkt.size, 1,
                        AUDIO_INBUF_SIZE - avpkt.size, f);
            if (len > 0)
                avpkt.size += len;
        }
    }

    fclose(outfile);
    fclose(f);

    avcodec_close(c);
    av_free(c);
    av_frame_free(&decoded_frame);
}

int snd_ctl_set_volume(int val)
{
	int err;   
	int orig_volume = 0;
	//unsigned int count;
	static snd_ctl_t *handle = NULL;   
	snd_ctl_elem_info_t *info;  
	snd_ctl_elem_id_t *id;  
	snd_ctl_elem_value_t *control;  
	//snd_ctl_elem_type_t type;

	snd_ctl_elem_info_alloca(&info);   
	snd_ctl_elem_id_alloca(&id);   
	snd_ctl_elem_value_alloca(&control);    

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, "Headphone Playback Volume");
	// snd_ctl_elem_id_set_name(id, "PCM Playback Volume");
	// snd_ctl_elem_id_set_index(id, 1);  // "Mic Capture Volume" does not have index.   

	if ((err = snd_ctl_open(&handle, "default", 0)) < 0) { 
		printf("Control open error: %s\n", snd_strerror(err));
		return -1; 
	}  

	snd_ctl_elem_info_set_id(info, id);   
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {  
		printf("Cannot find the given element from control: %s\n", snd_strerror(err));   
		snd_ctl_close(handle);
		return -1; 
	} 
	
	//type = snd_ctl_elem_info_get_type(info);   
	//count = snd_ctl_elem_info_get_count(info); 
	
	snd_ctl_elem_value_set_id(control, id);   

	if (!snd_ctl_elem_read(handle, control)) {  
		orig_volume = snd_ctl_elem_value_get_integer(control, 0); 
	} 

	if(val != orig_volume) {   
		printf("new_value(%d) orgin_value(%d)\n", val, orig_volume);
		snd_ctl_elem_value_set_integer(control, 0, (long)(val));
		snd_ctl_elem_value_set_integer(control, 1, (long)(val));
		
		if ((err = snd_ctl_elem_write(handle, control)) < 0) {  
			printf("Control element write error: %s\n", snd_strerror(err));   
			snd_ctl_close(handle);   
			return -1; 
		} 
	}  
	snd_ctl_close(handle);  
	return 0;
}

snd_pcm_t *pcm_init(int bitrate) 
{
	int res;
	unsigned int val;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	// snd_pcm_uframes_t frames;

	res = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	//res = snd_pcm_open(&handle, "plug:dmixer", SND_PCM_STREAM_PLAYBACK, 0);
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
	snd_pcm_hw_params_set_rate_near(handle, params, &val, 0);
	
	unsigned period_time = 0;
	unsigned buffer_time = 0;
	snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
	if (buffer_time > 500000) buffer_time = 500000;
	period_time = buffer_time >> 2;
	snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
	snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
	//frames = 32;
	//snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	res = snd_pcm_hw_params(handle, params);
	if (res < 0) {
		printf("unable to set hw parameters: %s\n", snd_strerror(res));
		snd_pcm_close(handle);
		return NULL;
	}
	
	snd_ctl_set_volume(10);
	
	return handle;
}


#define AVCODEC_MAX_AUDIO_FRAME_SIZE	512
// uint8_t inbuf[AVCODEC_MAX_AUDIO_FRAME_SIZE * 100];
int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("Usage : %s [mp3 file]\n", argv[0]);
		exit(1);
	}
	
	av_register_all();

	int i, audioStream, res;
	AVCodecContext * 	pCodecCtx;
	AVFormatContext * 	pFormatCtx;
	AVCodec * 			pCodec;
	AVFrame * 			pFrame;
	AVPacket 			packet;
	
	uint8_t * pktdata;
	int pktsize;

	pFormatCtx = avformat_alloc_context();
	
	// 打开输入文件
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		printf("can,t open file");
		return -1;
	}
	
	//if (av_find_stream_info(pFormatCtx) < 0)
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1;

	av_dump_format(pFormatCtx, 0, argv[1], 0); // 显示pfmtctx->streams里的信息 测试时用到
	
	
	// 找到第一个音频流
	audioStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; ++i)  //找到音频、视频对应的stream
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			break;
		}
	}
	
	if (audioStream == -1) {
		printf("no audio stream\n");
		return -1;
	}
	
	// 获得音频流的解码器上下文
	pCodecCtx = pFormatCtx->streams[audioStream]->codec; 
	
	printf(" codec_id   = 0x%08X\n", pCodecCtx->codec_id);
	printf(" nb_samples = %d\n", pCodecCtx->frame_size);
	printf(" format     = %d\n", pCodecCtx->sample_fmt);
	
	// 根据解码器上下文找到解码器
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);	
	if (pCodec == NULL) {
		printf("avcodec_find_decoder error\n");
		return -1;
	}

	
	// Inform the codec that we can handle truncated bitstreams
	// bitstreams where frame boundaries can fall in the middle of packets
	if (pCodec->capabilities & CODEC_CAP_TRUNCATED)
	{
		pCodecCtx->flags |= CODEC_CAP_TRUNCATED;
	}

	// 打开解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("avcodec_open error\n");
		return -1;
	}

	struct SwrContext *swr = swr_alloc();
	av_opt_set_int(swr, "in_channel_layout",  pCodecCtx->channel_layout, 0);
	av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO,  0);
	av_opt_set_int(swr, "in_sample_rate",     pCodecCtx->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate",    pCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt",  pCodecCtx->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
	swr_init(swr);

	// Hack to correct wrong frame rates that seem to be generated by some 
	// codecs

	pFrame = av_frame_alloc();
	
	pFrame->nb_samples     = pCodecCtx->frame_size;
    pFrame->format         = pCodecCtx->sample_fmt;
    pFrame->channel_layout = pCodecCtx->channel_layout;
	pFrame->sample_rate	   = pCodecCtx->sample_rate;
	av_frame_set_channels(pFrame, pCodecCtx->channels);
	
	snd_pcm_t *handle = pcm_init(pFrame->sample_rate);
	
	printf(" == PCM init ==\n");
 
	int got_frame;
	//int data_size;
	int dst_linesize = 0;
	uint8_t **dst_data = NULL;
	
	av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, 2, 
			pFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
			
	printf("dst_data = %p\tnb_samples = %d\n", dst_data, pFrame->nb_samples);
	 
	while(av_read_frame(pFormatCtx, &packet) >= 0) //pFormatCtx中调用对应格式的packet获取函数
	{
		if (packet.stream_index == audioStream)
		{
			pktdata = packet.data;
			pktsize = packet.size;
			
			
			while (pktsize > 0)
			{
				got_frame = 0;
				int len = avcodec_decode_audio4(pCodecCtx, pFrame, &got_frame, &packet);
				//data_size = av_samples_get_buffer_size(NULL, pCodecCtx->channels, 
				//	pFrame->nb_samples, pCodecCtx->sample_fmt, 1);
					
				if (len < 0) {
					printf("error\n");
					break;
				}
				
				if (got_frame > 0) {
					i = swr_convert(swr, dst_data, pFrame->nb_samples, (const uint8_t **)pFrame->data, pFrame->nb_samples);
					// printf("i = %d\n", i);
					res = snd_pcm_writei(handle, dst_data[0], pFrame->nb_samples);
					if (res == -EPIPE) {
						// EPIPE means underrun
						printf("underrun occurred\n");
						snd_pcm_prepare(handle);
					} else if (res < 0) {
						printf("error from writei: %s\n", snd_strerror(res));
						break;
					}
				}
				
				pktsize -= len;
				pktdata += len;
			}
			av_free_packet(&packet);
		}
	}
	
	av_freep(&dst_data[0]);
	
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	
	return 0;
}
