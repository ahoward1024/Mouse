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
}

#include "xtrace.h" // NOTE: See note inside xtrace.h (for Visual Studio debugging)

#include "datatypes.h"
#include "colors.h"

global bool Global_Running = true;

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

	AVCodecContext  *codecCtx = NULL;
	AVCodecContext  *videoCodecCtx = NULL;
	AVCodec 			  *codec = NULL;

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

	videoCodecCtx = avFormatCtx->streams[videoStreamIndex]->codec;
	codec = avcodec_find_decoder(videoCodecCtx->codec_id);

	AVRational rat = videoCodecCtx->framerate;
	float32 framerate = (float32)rat.num / (float32)rat.den;
	printf("VIDEO FRAMERATE: %f\n", framerate);

	if(codec == NULL)
	{
		printf("ERROR: Unsupported codec!\nExiting.\n");
		return -1;
	}

	codecCtx = avcodec_alloc_context3(codec);
	if(avcodec_copy_context(codecCtx, videoCodecCtx) != 0)
	{
		printf("ERROR: Could not copy codec context!\nExiting.\n");
		return -1;
	}

	if(avcodec_open2(codecCtx, codec, NULL) < 0)
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

	sws_ctx = sws_getContext(codecCtx->width,
	                         codecCtx->height,
	                         codecCtx->pix_fmt,
	                         codecCtx->width,
	                         codecCtx->height,
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
	                                         codecCtx->width, codecCtx->height);

	int32 yPlaneSz = codecCtx->width * codecCtx->height;
	int32 uvPlaneSz = codecCtx->width * codecCtx->height / 4;
	uint8 *yPlane = (uint8 *)malloc(yPlaneSz);
	uint8 *uPlane = (uint8 *)malloc(uvPlaneSz);
	uint8 *vPlane = (uint8 *)malloc(uvPlaneSz);
	if(!yPlane || !uPlane || !vPlane)
	{
		printf("Could not allocate pixel buffers!\nExiting\n");
		return -1;
	}

	int32 uvPitch = codecCtx->width / 2;

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
				avcodec_decode_video2(codecCtx, pFrame, &frameFinished, &packet);

				if(frameFinished)
				{
					AVPicture pict;
					pict.data[0] = yPlane;
					pict.data[1] = uPlane;
					pict.data[2] = vPlane;
					pict.linesize[0] = codecCtx->width;
					pict.linesize[1] = uvPitch;
					pict.linesize[2] = uvPitch;

					sws_scale(sws_ctx, (uint8 const * const *) pFrame->data, pFrame->linesize, 0, codecCtx->height, pict.data, pict.linesize);

					SDL_UpdateYUVTexture(texture, NULL, yPlane, codecCtx->width, uPlane, uvPitch, vPlane, uvPitch);
					
					SDL_RenderClear(renderer);
					SDL_RenderCopy(renderer, texture, &videoSrcRect, &videoDestRect);
					SDL_RenderPresent(renderer);

					endClock = SDL_GetTicks();

					float32 ticksElapsed = (float32)endClock - (float32)startClock;
					float32 sleepms = framerate - ticksElapsed;
					printf("end: %d, start %d\nelapsed: %f\n", endClock, startClock, ticksElapsed);
					printf("Sleep: %f\n", sleepms);
					if(sleepms > 0) SDL_Delay(sleepms);

					startClock = SDL_GetTicks();
				}
			}
		}

		av_free_packet(&packet);

		
	}


	printf("\n\tGoodbye!\n");

	SDL_Quit();

	av_free(buffer);
	av_frame_free(&pFrameRGB);

	av_frame_free(&pFrame);

	avcodec_close(codecCtx);
	avcodec_close(videoCodecCtx);

	avformat_close_input(&avFormatCtx);

	return 0;
}