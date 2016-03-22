#include <SDL2/SDL.h>
#include <SDL2/SDL_gfxPrimitives.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

extern "C" 
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/avconfig.h>
	#include <libswresample/swresample.h>
}

#include "xtrace.h" // NOTE: See note inside xtrace.h (for Visual Studio debugging)

#include "datatypes.h"
#include "colors.h"

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

global bool Global_Running = true;

struct PacketQueue
{
	AVPacketList *first_pkt;
	AVPacketList *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
};

PacketQueue audioq;

void initPacketQueue(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}

int putPacketQueue(PacketQueue *q, AVPacket *pkt)
{
	AVPacketList * pkt1;
	if(av_dup_packet(pkt) < 0) return -1;
	pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
	if(!pkt1) return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	SDL_LockMutex(q->mutex);

	if(!q->last_pkt) q->first_pkt = pkt1;
	else q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;

	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);

	return 0;
}

static int getPacketQueue(PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int result;

	SDL_LockMutex(q->mutex);

	for(;;)
	{
		if(!Global_Running)
		{
			result = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if(pkt1)
		{
			q->first_pkt = pkt1->next;
			if(!q->first_pkt) q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			result = 1;
			break;
		}
		else if(!block)
		{
			result = 0;
			break;
		}
		else
		{
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return result;
}

int decodeAudioFrame(AVCodecContext *audioCodecCtx, uint8 *audioBuff, int buffSize)
{
	static AVPacket pkt;
	static uint8 *audioPktData = NULL;
	static int audioPktSize = 0;
	static AVFrame frame;

	int len1, data_size = 0;

	for(;;)
	{
		while(audioPktSize > 0)
		{
			int gotFrame = 0;
			len1 = avcodec_decode_audio4(audioCodecCtx, &frame, &gotFrame, &pkt);
			if(len1 < 0)
			{
				// Error, skip frame
				audioPktSize = 0;
				break;
			}

			audioPktData += len1;
			audioPktSize -= len1;
			data_size = 0;
			if(gotFrame)
			{
				data_size = av_samples_get_buffer_size(NULL, 
				                                       audioCodecCtx->channels, 
				                                       frame.nb_samples,
				                                       audioCodecCtx->sample_fmt,
				                                       1);
				assert(data_size <= buffSize);
				memcpy(audioBuff, frame.data[0], data_size);
			}
			if(data_size <= 0)
			{
				// No data yet, get more frames
				continue;
			}
			return data_size;
		}
		if(pkt.data) av_free_packet(&pkt);
		if(!Global_Running) return -1;
		if(getPacketQueue(&audioq, &pkt, 1) < 0) return -1;
		audioPktData = pkt.data;
		audioPktSize = pkt.size;
	}
}

void audioCallback(void *userdata, uint8 *stream, int len)
{
	AVCodecContext *audioCodecCtx = (AVCodecContext *)userdata;
	int len1, audio_size;

	static uint8 audio_buf[(MAX_AUDIO_FRAME_SIZE * 2)];
	static unsigned int audio_buf_size = 0;
	static unsigned int audio_buf_index = 0;

	SDL_memset(stream, 0, len);

	while(len > 0)
	{
		if(audio_buf_index >= audio_buf_size)
		{
			audio_size = decodeAudioFrame(audioCodecCtx, audio_buf, sizeof(audio_buf));
			if(audio_size < 0)
			{
				audio_buf_size = 0;
				memset(audio_buf, 0, audio_buf_size);
			}
			else
			{
				audio_buf_size = audio_size;
			}
			audio_buf_index = 0;
		}
		len1 = audio_buf_size - audio_buf_index;
		if(len1 > len) len1 = len;
		memcpy(stream, (uint8 *)audio_buf + audio_buf_index, len1);
		len -= len1;
		stream += len1;
		audio_buf_index += len1;
	}
}

static  Uint8  *audio_chunk; 
static  Uint32  audio_len; 
static  Uint8  *audio_pos; 

void fillaudio(void *userdata, uint8 *stream, int len)
{
	SDL_memset(stream, 0, len);
	if(audio_len == 0) return;

	len = (len > audio_len ? audio_len : len);

	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}

internal void HandleEvents(SDL_Event event)
{
	if(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT)
		{
			Global_Running = false;
		}

		if(event.type == SDL_KEYDOWN)
		{
			SDL_Keycode key = event.key.keysym.sym;
			switch(key)
			{
				case SDLK_ESCAPE:
				{
					Global_Running = false;
				} break;
			}
		}

		if(event.type == SDL_MOUSEBUTTONDOWN)
		{
			if(event.button.button == SDL_BUTTON_LEFT)
			{
				printf("Pressed left mouse.\n");
			}
		}
	}
}

int main(int argc, char **argv)
{
	AVFormatContext *avFormatCtx = NULL;
	int32 				  videoStreamIndex = -1, audioStreamIndex = -1;
	
	AVFrame 			  *pFrame = NULL;
	AVFrame         *pFrameRGB = NULL;
	AVPacket        packet;
	int32           frameFinished;
	int32           numBytes;
	uint8           *buffer = NULL;

	AVCodecContext  *videoCodecCtxOrig = NULL;
	AVCodecContext  *videoCodecCtx = NULL;
	AVCodecContext  *audioCodecCtxOrig = NULL;
	AVCodecContext  *audioCodecCtx = NULL;
	AVCodec 			  *videoCodec = NULL;
	AVCodec         *audioCodec = NULL;

	struct          SwsContext *sws_ctx = NULL;

	av_register_all(); // FFMpeg register all codecs

	// Init video and audio for SDL
	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0)
	{
		printf("%s\n", SDL_GetError()); // DEBUG
		return -1;
	}

	const char fname[] = "../res/nggyu.mp4"; // DEBUG move file to argument list

	// Open file
	if(avformat_open_input(&avFormatCtx, fname, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", fname); // DEBUG
		return -1;
	}
	else
	{
		printf("Successfully opened file: %s\n", fname); // DEBUG
	}

	if(avformat_find_stream_info(avFormatCtx, NULL) < 0)
	{
		printf("Cound not find stream information.\n"); // DEBUG
	}
	else
	{
		printf("Successfully found stream info.\n"); // DEBUG
	}

	// NOTE: This will send dump the format of the file to sterr(0). VS will not show this in it's 
	// output window so you must run as a console in order to see it. Fucking lame, Microsoft.
	av_dump_format(avFormatCtx, 1, fname, 0);


	for(int i = 0; i < avFormatCtx->nb_streams; ++i)
	{
		if(avFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0)
		{
			videoStreamIndex = i;
		}
		if(avFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0)
		{
			audioStreamIndex = i;
		}	
	}

	if(videoStreamIndex == -1)
	{
		printf("Could not find a video stream.\n"); // DEBUG
		return -1;
	}
	else
	{
		printf("Found video stream at index: %d.\n", videoStreamIndex); // DEBUG
	}

	if(audioStreamIndex == -1)
	{
		printf("Could not find a audio stream.\n"); // DEBUG
		return -1;
	}
	else
	{
		printf("Found audio stream at index: %d.\n", audioStreamIndex); // DEBUG
	}

	audioCodecCtxOrig = avFormatCtx->streams[audioStreamIndex]->codec;
	audioCodec = avcodec_find_decoder(audioCodecCtxOrig->codec_id);
	if(!audioCodec)
	{
		printf("ERROR: Unsupported audio codec!\nExiting.\n");
		return -1;
	}

	audioCodecCtx = avcodec_alloc_context3(audioCodec);
	if(avcodec_copy_context(audioCodecCtx, audioCodecCtxOrig) != 0)
	{
		printf("ERROR: Could not copy codec context!\nExiting.\n");
		return -1;
	}

	SDL_AudioSpec wantedSpec;
	wantedSpec.freq = audioCodecCtx->sample_rate;
	wantedSpec.format = AUDIO_S16SYS;
	wantedSpec.channels = audioCodecCtx->channels;
	wantedSpec.silence = 0;
	wantedSpec.samples = SDL_AUDIO_BUFFER_SIZE;
	wantedSpec.callback = fillaudio;
	wantedSpec.userdata = audioCodecCtx;

	SDL_AudioSpec audioSpec;
	if(SDL_OpenAudio(&wantedSpec, &audioSpec) < 0)
	{
		printf("ERROR: Could not open SDL_Audio: %s\nExiting.\n", SDL_GetError());
		return -1;
	}

	avcodec_open2(audioCodecCtx, audioCodec, NULL);

	SwrContext *au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
	                                    44100, audioCodecCtx->channels, audioCodecCtx->sample_fmt,
	                                    audioCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	initPacketQueue(&audioq);
	SDL_PauseAudio(0);

	videoCodecCtxOrig = avFormatCtx->streams[videoStreamIndex]->codec;
	videoCodec = avcodec_find_decoder(videoCodecCtxOrig->codec_id);
	if(!videoCodec)
	{
		printf("ERROR: Unsupported video codec!\nExiting.\n");
		return -1;
	}
	videoCodecCtx = avcodec_alloc_context3(videoCodec);
	if(avcodec_copy_context(videoCodecCtx, videoCodecCtxOrig) != 0)
	{
		printf("ERROR: Could not copy codec context!\nExiting.\n");
		return -1;
	}

	if(avcodec_open2(videoCodecCtx, videoCodec, NULL) < 0)
	{
		printf("ERROR: Could not open codec!\nExiting.\n");
		return -1;
	}

	pFrame = av_frame_alloc();

	if(pFrame == NULL)
	{
		printf("ERROR: Error in frame allocation.\nExiting.\n");
		return -1;
	}

	AVRational rat = videoCodecCtx->framerate;
	float32 framerate = (float32)rat.num / (float32)rat.den;
	printf("VIDEO FRAMERATE: %f\n", framerate);

	sws_ctx = sws_getContext(videoCodecCtx->width,
	                         videoCodecCtx->height,
	                         videoCodecCtx->pix_fmt,
	                         videoCodecCtx->width,
	                         videoCodecCtx->height,
	                         AV_PIX_FMT_YUV420P,
	                         SWS_BILINEAR,
	                         NULL,
	                         NULL,
	                         NULL);


	

	// STC: Create and SDL window at the center of the main monitor with a size of 1920 x 1080 for now
	SDL_Window *window = SDL_CreateWindow("Window Title", 
	                                      SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
	                                      1920, 1080,
	                                      SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL|
	                                      SDL_WINDOW_MAXIMIZED);


	if(window == NULL)
	{
		printf("ERROR: window could not be created.\nExiting.\n");
		return -1;
	}

	// TODO: Switch to OpenGL for rendering shapes (buttons, backgrounds, etc.)
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, 
	                                         SDL_TEXTUREACCESS_STREAMING,
	                                         videoCodecCtx->width, videoCodecCtx->height);

	int32 yPlaneSz = videoCodecCtx->width * videoCodecCtx->height;
	int32 uvPlaneSz = videoCodecCtx->width * videoCodecCtx->height / 4;
	uint8 *yPlane = (uint8 *)malloc(yPlaneSz);
	uint8 *uPlane = (uint8 *)malloc(uvPlaneSz);
	uint8 *vPlane = (uint8 *)malloc(uvPlaneSz);
	if(!yPlane || !uPlane || !vPlane)
	{
		printf("Could not allocate pixel buffers!\nExiting\n");
		return -1;
	}

	int32 uvPitch = videoCodecCtx->width / 2;

	int mousex = 0, mousey = 0, rad = 10;

	SDL_Event event = {};

	uint32 startClock, endClock;
	startClock = SDL_GetTicks();

	SDL_Rect videoSrcRect;
	videoSrcRect.x = 0;
	videoSrcRect.y = 0;
	videoSrcRect.w = (videoCodecCtx->width * 4);
	videoSrcRect.h = (videoCodecCtx->height * 4);

	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	SDL_Rect videoDestRect;
	videoDestRect.x = (windowWidth / 2) - (videoSrcRect.w / 2);
	videoDestRect.y = (windowHeight / 2) - (videoSrcRect.h / 2);
	videoDestRect.w = videoSrcRect.w;
	videoDestRect.h = videoSrcRect.h;

	tColor bgc = tColorFromHex(COLOR_BACKGROUNDCOLOR);
	SDL_SetRenderDrawColor(renderer, bgc.r, bgc.g, bgc.b, bgc.a);

	uint64 out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = audioCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 441000;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
	uint8 *out_buffer = (uint8 *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	// MAIN LOOP
	while(Global_Running)
	{
		HandleEvents(event);
		SDL_PumpEvents();
		int32 mask = SDL_GetMouseState(&mousex, &mousey);

		int i = 0;
		if(av_read_frame(avFormatCtx, &packet) >= 0)
		{
			if(packet.stream_index == videoStreamIndex)
			{
				avcodec_decode_video2(videoCodecCtx, pFrame, &frameFinished, &packet);

				if(frameFinished)
				{
					AVPicture pict;
					pict.data[0] = yPlane;
					pict.data[1] = uPlane;
					pict.data[2] = vPlane;
					pict.linesize[0] = videoCodecCtx->width;
					pict.linesize[1] = uvPitch;
					pict.linesize[2] = uvPitch;

					sws_scale(sws_ctx, (uint8 const * const *) pFrame->data, pFrame->linesize, 0, videoCodecCtx->height, pict.data, pict.linesize);

					SDL_UpdateYUVTexture(texture, NULL, yPlane, videoCodecCtx->width, uPlane, uvPitch, vPlane, uvPitch);
					
					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, texture, &videoSrcRect, &videoDestRect);
					SDL_RenderPresent(renderer);

					endClock = SDL_GetTicks();

					float32 ticksElapsed = (float32)endClock - (float32)startClock;
					float32 sleepms = framerate - ticksElapsed;
					printf("end: %d, start %d\nelapsed: %f\n", endClock, startClock, ticksElapsed);
					printf("Sleep: %f\n", sleepms);
					// if(sleepms > 0) SDL_Delay(sleepms); // DEBUG Delay video framerate

					startClock = SDL_GetTicks();
				}
			}
			else if(packet.stream_index == audioStreamIndex)
			{
				int result = avcodec_decode_audio4(audioCodecCtx, pFrame, &frameFinished, &packet);
				if(result < 0)
				{
					printf("ERROR: Could not decode audio frame.\nExiting.\n");
					return -1;
				}
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8 **)pFrame->data, pFrame->nb_samples);
				// while(audio_len > 0) SDL_Delay(1.0f); // DEBUG Delay audio play rate
				audio_chunk = (uint8 *) out_buffer;
				audio_len = out_buffer_size;
				audio_pos = audio_chunk;
			}
			else
			{
				av_free_packet(&packet);
			}
		}
	}

	printf("\n\tGoodbye!\n");

	SDL_Quit();

	av_free(buffer);
	av_frame_free(&pFrameRGB);

	av_frame_free(&pFrame);

	avcodec_close(videoCodecCtx);
	avcodec_close(videoCodecCtxOrig);

	avcodec_close(audioCodecCtx);
	avcodec_close(audioCodecCtxOrig);

	avformat_close_input(&avFormatCtx);

	return 0;
}