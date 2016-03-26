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

// FIXME: Window will hang completely if resized AFTER A CLIP LOOPS.
// This has something to do with freeing the clips texture.

// TODO: List of view rectangles to pass around
global SDL_Rect currentView, currentViewBack, compositeView, compositeViewBack, 
       timelineView, browserView, effectsView;

// Video only clips
// NOTE:IMPORTANT: Video clips MUST be initalized to zero. (STC)
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
	AVRational          aspectRatio;
	float               aspectRatioF;
	int                 currentFrame;
	int                 frameCount;
};

struct AudioClip
{
	// TODO: Create audio only clips
};

struct AVClip
{
	// TODO: Create audio and video clips
};

// This will free the clip for reinitalization, we do not free the texture
// as SDL still needs it for video resizing.
void freeVideoClip(VideoClip *clip)
{
	av_frame_free(&clip->avFrame);
	avcodec_close(clip->videoCodecCtx);
	avformat_close_input(&clip->avFormatCtx);
	free(clip->avFormatCtx);
	free(clip->yPlane);
	free(clip->uPlane);
	free(clip->vPlane);
}

// This will free the clip entirely
void freeVideoClipFull(VideoClip *clip)
{
	freeVideoClip(clip);
	SDL_free(clip->texture);
}

void initVideoClip(VideoClip *clip, const char *file, bool loop)
{
	*clip = {}; // MUST be initalized to zero (STC)
	clip->avFormatCtx = NULL;
	if(avformat_open_input(&clip->avFormatCtx, file, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", file);
		exit(-1);
	}

	clip->filename = av_strdup(clip->avFormatCtx->filename);
	printf("FILENAME: %s\n", clip->filename);

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

	assert(clip->videoStreamIndex != -1);
	AVCodecContext *videoCodecCtxOrig = 
		clip->avFormatCtx->streams[clip->videoStreamIndex]->codec;
	clip->videoCodec = avcodec_find_decoder(videoCodecCtxOrig->codec_id);
	
	clip->videoCodecCtx	= avcodec_alloc_context3(clip->videoCodec);
	avcodec_copy_context(clip->videoCodecCtx, videoCodecCtxOrig);

	avcodec_close(videoCodecCtxOrig);

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

	if(!clip->texture) 
	{
		clip->texture = SDL_CreateTexture(renderer,
	                                  	SDL_PIXELFORMAT_YV12,
	                                  	SDL_TEXTUREACCESS_STREAMING,
	                                  	clip->videoCodecCtx->width,
	                                  	clip->videoCodecCtx->height);
	}

	AVRational fr = clip->videoCodecCtx->framerate;
	clip->framerate = (float)fr.num / (float)fr.den;
	printf("Framerate: %.2ffps\n", clip->framerate);

	av_reduce(&clip->aspectRatio.num, &clip->aspectRatio.den,
	          clip->videoCodecCtx->width,
	          clip->videoCodecCtx->height,
	          16);
	clip->aspectRatioF = (float)clip->aspectRatio.num / (float)clip->aspectRatio.den;
	printf("Aspect Ratio: (%f), [%d:%d]\n", clip->aspectRatioF, 
	       clip->aspectRatio.num, clip->aspectRatio.den);

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
		av_free_packet(&clip->packet);
	}
	else
	{
		av_free_packet(&clip->packet);
		av_frame_free(&clip->avFrame);
		if(clip->loop)
		{
			// TODO: This is bad. Real bad. Find a better way to do this.
			freeVideoClip(clip);
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

void resizeAllWindowElements(SDL_Window *window, 
                             SDL_Rect *currentViewBack, SDL_Rect *compositeViewBack,
                             SDL_Rect *currentView, SDL_Rect *compositeView, 
                             SDL_Rect *browserView,
                             SDL_Rect *timelineView,
                             SDL_Rect *effectsView,
                             VideoClip *clip)
{
	const int bufferSpace = 10;

	int windowWidth, windowHeight, hwidth, hheight, backViewW, backViewH, 
	currentViewW, currentViewH, compositeViewW, compositeViewH;

	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	hwidth = windowWidth / 2;
	hheight = windowHeight / 2;

	// Find the nearest 16x9 (STC) resolution to fit the current and composite views into
	// STC: Need to make the aspect ratio the ar of the output composition.
	for(int w = 0, h = 0; 
	    w < (hwidth - (bufferSpace * 2)) && h < (hheight - (bufferSpace * 2));
	    w += 16, h += 9)
	{
		backViewW = w;
		backViewH = h;
	}

	// Fit the current view background into an output aspect ratio rectangle on the left side
	currentViewBack->w = backViewW;
	currentViewBack->h = backViewH;	
	currentViewBack->x = bufferSpace + 
	                     (((hwidth - (bufferSpace * 2)) - currentViewBack->w) / 2);
	currentViewBack->y = bufferSpace + 
	                     (((hheight - (bufferSpace * 2)) - currentViewBack->h) / 2);

	// Fit the current view background into an output aspect ratio rectangle on the right side
	compositeViewBack->w = backViewW;
	compositeViewBack->h = backViewH;
	compositeViewBack->x = (windowWidth / 2) + bufferSpace + 
	                       (((hwidth - (bufferSpace * 2)) - compositeViewBack->w) / 2);
	compositeViewBack->y = bufferSpace + (((hheight - (bufferSpace * 2)) - 
	                                       compositeViewBack->h) / 2);

	// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
	for(int w = 0, h = 0; 
	    w <= currentViewBack->w && h <= currentViewBack->h; 
	    w += clip->aspectRatio.num, h += clip->aspectRatio.den)
	{
		currentViewW = w;
		currentViewH = h;
	}

	// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
	for(int w = 0, h = 0; 
	    w <= compositeViewBack->w && h <= compositeViewBack->h; 
	    w += clip->aspectRatio.num, h += clip->aspectRatio.den)
	{
		compositeViewW = w;
		compositeViewH = h;
	}

	// Put the current clip view on the right hand side and center it
	// in the current view background
	currentView->w = currentViewW;
	currentView->h = currentViewH;
	currentView->x = currentViewBack->x + ((currentViewBack->w - currentView->w) / 2);
	currentView->y = currentViewBack->y + ((currentViewBack->h - currentView->h) / 2);

	// Put the composite clip view on the left hand side and center it
	// in the composite view background
	compositeView->w = compositeViewW;
	compositeView->h = compositeViewH;
	compositeView->x = compositeViewBack->x + ((compositeViewBack->w - compositeView->w) / 2);
	compositeView->y = compositeViewBack->y + ((compositeViewBack->h - compositeView->h) / 2);

#if 0
	// Put the file browser view in the bottom right portion of the screen
	browserView->x = bufferSpace;
	browserView->y = (windowHeight / 2) + bufferSpace;
	browserView->w = (windowHeight / 3);
	browserView->h = (windowHeight / 2) - (bufferSpace * 2);

	// Put the track effects view in the bottom portion of the screen to the
	// left of the timeline view
	effectsView->x = (((windowWidth / 6) - bufferSpace) * 5);
	effectsView->y = (windowHeight / 2) + bufferSpace;
	effectsView->w = browserView->w;
	effectsView->h = browserView->h;

	// Put the timeline clips view in the center bottom portion of the screen
	// between the file browser view and the track effects view
	timelineView->x = browserView->x + browserView->w + bufferSpace;
	timelineView->y = (windowHeight / 2) + bufferSpace;
	timelineView->w = effectsView->x - timelineView->x - bufferSpace;
	timelineView->h = (windowHeight / 2) - (bufferSpace * 2);
#endif
}

void testBufferRects(SDL_Renderer *renderer, int windowWidth, int windowHeight)
{
	// DEBUG <
	SDL_SetRenderDrawColor(renderer, tcGreen.r, tcGreen.g, tcGreen.b, tcGreen.a);
	SDL_Rect testrect;
	int hrectb = 10;
	
	testrect.x = 0;
	testrect.y = hrectb;
	testrect.w = hrectb;
	testrect.h = (windowHeight / 2) - (hrectb * 2);
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = hrectb;
	testrect.y = 0;
	testrect.w = (windowWidth / 2) - (hrectb * 2);
	testrect.h = hrectb;
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = hrectb;
	testrect.y = (windowHeight / 2) - hrectb;
	testrect.w = (windowWidth / 2) - (hrectb * 2);
	testrect.h = hrectb;
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = (windowWidth / 2) - hrectb;
	testrect.y = hrectb;
	testrect.w = hrectb;
	testrect.h = (windowHeight / 2) - (hrectb * 2);
	SDL_RenderFillRect(renderer, &testrect);
	testrect.x = (windowWidth / 2) + 1;
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = (windowWidth / 2) + 1 + hrectb;
	testrect.y = 0;
	testrect.w = (windowWidth / 2) - 1 - (hrectb * 2);
	testrect.h = hrectb;
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = (windowWidth / 2) + 1 + hrectb;
	testrect.y = (windowHeight / 2) - hrectb;
	SDL_RenderFillRect(renderer, &testrect);

	testrect.x = (windowWidth) - hrectb;
	testrect.y = hrectb;
	testrect.w = hrectb;
	testrect.h = (windowHeight / 2) - (hrectb * 2);
	SDL_RenderFillRect(renderer, &testrect);

	SDL_SetRenderDrawColor(renderer, tcRed.r, tcRed.g, tcRed.b, tcRed.a);
	SDL_RenderDrawLine(renderer, windowWidth / 2, 0, windowWidth / 2, windowHeight / 2);
	SDL_RenderDrawLine(renderer, 0, windowHeight / 2, windowWidth, windowHeight / 2);
	// DEBUG >
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
		if(event.type == SDL_WINDOWEVENT)
		{
			switch(event.window.event)
			{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				{
					resizeAllWindowElements(window, 
					                        &currentViewBack, &compositeViewBack,
					                        &currentView, &compositeView,
					                        &browserView,
					                        &timelineView,
					                     		&effectsView,
					                        clip);
				} break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
				{
					Global_Paused = true;
				} break;
			}
		}
	}
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

	const char *fname;
	if(argv[1]) fname = argv[1];
	else fname = "../res/nggyu.mp4";

	// TODO: List of clips to pass around
	VideoClip clip0;
	initVideoClip(&clip0, fname, true);

	int frameFinished;
	bool signalFinished = true;

	printf("Framecount: %d\n", clip0.frameCount);

	resizeAllWindowElements(window, 
	                        &currentViewBack, &compositeViewBack,
	                        &currentView, &compositeView,
	                        &browserView,
	                        &timelineView,
	                        &effectsView,
	                        &clip0);

	// Global_Paused = true; // DEBUG

	while(Global_Running)
	{
		HandleEvents(event, &clip0);
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		if(!Global_Paused)
		{
			playVideoClip(&clip0);
		}

		//printf("current frame: %d\n", clip0.currentFrame);

		SDL_SetRenderDrawColor(renderer, tcBgc.r, tcBgc.g, tcBgc.b, tcBgc.a);
		SDL_RenderClear(renderer);

		//testBufferRects(renderer, windowWidth, windowHeight);

		SDL_SetRenderDrawColor(renderer, tcBlack.r, tcBlack.g, tcBlack.b, tcBlack.a);
		SDL_RenderFillRect(renderer, &currentViewBack);
		SDL_RenderFillRect(renderer, &compositeViewBack);

		SDL_SetRenderDrawColor(renderer, tcRed.r, tcRed.g, tcRed.b, tcRed.a);
		SDL_RenderFillRect(renderer, &currentView);
		SDL_SetRenderDrawColor(renderer, tcBlue.r, tcBlue.g, tcBlue.b, tcBlue.a);
		SDL_RenderFillRect(renderer, &compositeView);

		// TODO: Render copy list of videos....
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &currentView);
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &compositeView);

		SDL_SetRenderDrawColor(renderer, tcView.r, tcView.g, tcView.b, tcView.a);
		//SDL_RenderFillRect(renderer, &browserView);
		//SDL_RenderFillRect(renderer, &timelineView);
		//SDL_RenderFillRect(renderer, &effectsView);
		
		SDL_RenderPresent(renderer);

	}

	SDL_Quit();

	freeVideoClipFull(&clip0);
	
	printf("Goodbye.\n");

	return 0;
}