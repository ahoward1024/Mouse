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

global bool Global_Running = true;
global bool Global_Paused = false;

global SDL_Window *window;
global SDL_Renderer *renderer;

struct VideoClip
{
	AVFormatContext    *avFormatCtx;
	AVCodecContext     *videoCodecCtx;
	AVCodec            *videoCodec;
	AVFrame            *avFrame;
	AVPacket            packet;
	struct SwsContext  *swsCtx;
	int                 videoStreamIndex;
	SDL_Texture        *texture;
	SDL_Rect            srcRect;
	SDL_Rect            destRect;
	int32               yPlaneSz;
	int32               uvPlaneSz; 
	uint8              *yPlane;
	uint8              *uPlane; 
	uint8              *vPlane;
	int32               uvPitch;
	bool								loop;
	const char         *filename;
	float               framerate;
	int                 currentFrame;
	int                 frameCount;
};

void initVideoClip(VideoClip *clip, const char *file, bool loop)
{
	clip->filename = file;
	clip->avFormatCtx = NULL;
	if(avformat_open_input(&clip->avFormatCtx, file, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", file);
		exit(-1);
	}

	avformat_find_stream_info(clip->avFormatCtx, NULL);

	av_dump_format(clip->avFormatCtx, 0, file, 0); // DEBUG

	clip->videoStreamIndex = -1;

	for(int i = 0; i < clip->avFormatCtx->nb_streams; ++i)
	{
		if(clip->avFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			clip->videoStreamIndex = i;
			break;
		}
	}
	AVCodecContext *videoCodecCtxOrig = 
		clip->avFormatCtx->streams[clip->videoStreamIndex]->codec;
	clip->videoCodec = avcodec_find_decoder(videoCodecCtxOrig->codec_id);
	
	clip->videoCodecCtx	= avcodec_alloc_context3(clip->videoCodec);
	avcodec_copy_context(clip->videoCodecCtx, videoCodecCtxOrig);

	avcodec_open2(clip->videoCodecCtx, clip->videoCodec, NULL);

	clip->avFrame = NULL;
	clip->avFrame = av_frame_alloc();

	clip->swsCtx = sws_getContext(clip->videoCodecCtx->width,
	                              clip->videoCodecCtx->height,
	                              clip->videoCodecCtx->pix_fmt,
	                              clip->videoCodecCtx->width,
	                              clip->videoCodecCtx->height,
	                              AV_PIX_FMT_YUV420P,
	                              SWS_BILINEAR,
	                              NULL,
	                              NULL,
	                              NULL);

	clip->yPlaneSz = clip->videoCodecCtx->width * clip->videoCodecCtx->height;
	clip->uvPlaneSz = clip->videoCodecCtx->width * clip->videoCodecCtx->height / 4;
	clip->yPlane = (uint8 *)malloc(clip->yPlaneSz);
	clip->uPlane = (uint8 *)malloc(clip->uvPlaneSz);
	clip->vPlane = (uint8 *)malloc(clip->uvPlaneSz);

	clip->uvPitch = clip->videoCodecCtx->width / 2;

	clip->srcRect;
	clip->srcRect.x = 0;
	clip->srcRect.y = 0;
	clip->srcRect.w = clip->videoCodecCtx->width;
	clip->srcRect.h = clip->videoCodecCtx->height;

	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	SDL_Rect videoDestRect;
	clip->destRect.x = (windowWidth / 2) - (clip->srcRect.w / 2);
	clip->destRect.y = (windowHeight / 2) - (clip->srcRect.h / 2);
	clip->destRect.w = clip->srcRect.w;
	clip->destRect.h = clip->srcRect.h;

	clip->texture = SDL_CreateTexture(renderer,
	                                  SDL_PIXELFORMAT_YV12,
	                                  SDL_TEXTUREACCESS_STREAMING,
	                                  clip->videoCodecCtx->width,
	                                  clip->videoCodecCtx->height);

	AVRational rat = clip->videoCodecCtx->framerate;
	clip->framerate = rat.num / rat.den;

	clip->loop = loop;

	clip->currentFrame = -1;

	clip->frameCount = clip->avFormatCtx->streams[clip->videoStreamIndex]->nb_frames;

	avcodec_close(videoCodecCtxOrig);
}

int playVideoClip(void *data)
{
	VideoClip *clip = (VideoClip *)data;
	int frameFinished;
	if(av_read_frame(clip->avFormatCtx, &clip->packet) >= 0)
	{
		if(clip->packet.stream_index == clip->videoStreamIndex)
		{
			avcodec_decode_video2(clip->videoCodecCtx, clip->avFrame, 
			                      &frameFinished, &clip->packet);
			if(frameFinished)
			{
				AVPicture pict;
				pict.data[0] = clip->yPlane;
				pict.data[1] = clip->uPlane;
				pict.data[2] = clip->vPlane;
				pict.linesize[0] = clip->videoCodecCtx->width;
				pict.linesize[1] = clip->uvPitch;
				pict.linesize[2] = clip->uvPitch;

				sws_scale(clip->swsCtx, (uint8 const * const *)clip->avFrame->data, 
				          clip->avFrame->linesize, 
				          0, clip->videoCodecCtx->height, pict.data, pict.linesize);

				SDL_UpdateYUVTexture(clip->texture, NULL, clip->yPlane, 
				                     clip->videoCodecCtx->width, clip->uPlane,
				 										 clip->uvPitch, clip->vPlane, clip->uvPitch);

				clip->currentFrame = clip->avFrame->coded_picture_number;
			}
		}
	}
	else
	{
		av_free_packet(&clip->packet);
		if(clip->loop)
		{
			// TODO: This is bad. Real bad. Find a better way to do this.
			initVideoClip(clip, clip->filename, true);
		}
	}
	return 0;
}

void setClipPosition(SDL_Window *window, VideoClip *clip, int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if(x < 0 || y < 0 || x > width || y > height) return;
	clip->destRect.x = x;
	clip->destRect.y = y;
}

internal void HandleEvents(SDL_Event event, VideoClip *clip)
{
	if(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT) Global_Running = false;
		if(event.type == SDL_KEYDOWN)
		{
			SDL_Keycode key = event.key.keysym.sym;
			switch(key)
			{
				case SDLK_ESCAPE:
				{
					Global_Running = false;
				} break;
				case SDLK_SPACE:
				{
					if(!Global_Paused) Global_Paused = true;
					else Global_Paused = false;
				} break;
			}
		}
	}
}

int getGCD(int w, int h)
{
	int num = h;
	if(w > h) num = w;

	for(int  i = num - 1; i > 0; --i)
	{
		if((w % i == 0) && (h % i == 0)) return i;
	}
	return 0;
}

int main(int argc, char **argv)
{
	printf("Hello world.\n");

	av_register_all();

	SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow("Mouse", 
	                          SDL_WINDOWPOS_CENTERED, 
	                          SDL_WINDOWPOS_CENTERED, 
	                          1920, 1080, 
	                          SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	int windowWidth, windowHeight;
	

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Event event = {};

	tColor bgc = tColorFromHex(COLOR_BACKGROUNDCOLOR);

	const int bufferSpace = 10;

	int hwidth, hheight, sbnw, sbnh;

	SDL_Rect currentClipView;
	tColor currentClipViewColor = tColorFromHex(COLOR_RED);

	SDL_Rect compositeView;
	tColor compositeViewColor = tColorFromHex(COLOR_BLUE);

	SDL_Rect timelineView;
	tColor timelineViewColor = tColorFromHex(COLOR_TIMELINECOLOR);

	VideoClip clip0;
	initVideoClip(&clip0, "../res/Anime404.mp4", false);

	int frameFinished;
	bool signalFinished = true;

	printf("Framecount: %d\n", clip0.frameCount);

	while(Global_Running)
	{
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		hwidth = windowWidth / 2;
		hheight = windowHeight / 2;

		// Find the nearest 16 x 9 resolution to fit the windows into
		for(int w = 0, h = 0; 
		    w < (hwidth - (bufferSpace * 2)) && h < hheight - bufferSpace; 
		    w += 16, h += 9)
		{
			sbnw = w;
			sbnh = h;
		}

		// Put the current clip view on the right hand side
		currentClipView.x = bufferSpace;
		currentClipView.y = bufferSpace;
		currentClipView.w = sbnw;
		currentClipView.h = sbnh;

		// Put the composite clip view on the left hand side
		compositeView.w = sbnw + bufferSpace;
		compositeView.h = sbnh;
		compositeView.x = windowWidth - bufferSpace - compositeView.w;
		compositeView.y = bufferSpace;

		// Put the timeline clips view
		timelineView.x = bufferSpace;
		timelineView.y = (windowHeight / 2);
		timelineView.w = windowWidth - (bufferSpace * 2);
		timelineView.h = (windowHeight / 2) - bufferSpace;

		HandleEvents(event, &clip0);
		if(!Global_Paused)
		{
			playVideoClip(&clip0);
		}

		printf("current frame: %d\n", clip0.currentFrame);

		SDL_SetRenderDrawColor(renderer, bgc.r, bgc.g, bgc.b, bgc.a);
		SDL_RenderClear(renderer);

		SDL_SetRenderDrawColor(renderer, 
		                       currentClipViewColor.r, 
		                       currentClipViewColor.g, 
		                       currentClipViewColor.b,
		                       currentClipViewColor.a);
		SDL_RenderFillRect(renderer, &currentClipView);
		SDL_SetRenderDrawColor(renderer, 
		                       compositeViewColor.r, 
		                       compositeViewColor.g, 
		                       compositeViewColor.b,
		                       compositeViewColor.a);
		SDL_RenderFillRect(renderer, &compositeView);

		// TODO: Render copy list of videos....
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &currentClipView);
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &compositeView);


		SDL_SetRenderDrawColor(renderer, 
		                       timelineViewColor.r, 
		                       timelineViewColor.g, 
		                       timelineViewColor.b, 
		                       timelineViewColor.a);
		SDL_RenderFillRect(renderer, &timelineView);
		SDL_RenderPresent(renderer);

	}


	SDL_Quit();

	av_frame_free(&clip0.avFrame);

	avcodec_close(clip0.videoCodecCtx);

	avformat_close_input(&clip0.avFormatCtx);
	
	printf("Goodbye.\n");

	return 0;
}