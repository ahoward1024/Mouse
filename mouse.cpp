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

// TODO: Make these a user setting
global int Global_AspectRatio_W = 16;
global int Global_AspectRatio_H = 9;

// TODO: List of view rectangles to pass around
global SDL_Rect currentViewBack, compositeViewBack, currentView, compositeView,
timelineView, browserView, effectsView;

struct Border
{
	SDL_Rect top;
	SDL_Rect bot;
	SDL_Rect left;
	SDL_Rect right;
};      

global Border currentBorder, compositeBorder,
timelineBorder, browserBorder, effectsBorder;

// Video only clips
// NOTE:IMPORTANT: Video clips MUST be initalized to zero.
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
	int                 aspectRatioW;
	int                 aspectRatioH;
	float               aspectRatioF;
	int                 currentFrame;
	int                 frameCount;
	int                 width;
	int                 height;
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

// Window will hang completely if resized AFTER A CLIP LOOPS.
// This has something to do with freeing the clips texture.
// This will free the clip entirely so it CANNOT be used again.
void freeVideoClipFull(VideoClip *clip)
{
	freeVideoClip(clip);
	SDL_free(clip->texture);
}

void initVideoClip(VideoClip *clip, const char *file, bool loop)
{
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

	clip->width = clip->videoCodecCtx->width;
	clip->height = clip->videoCodecCtx->height;

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
	clip->destRect.x = 0;
	clip->destRect.y = 0;
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

	// AVRational returns a numerator and denomimator to divide to get a fractional value
	AVRational fr = clip->videoCodecCtx->framerate;
	clip->framerate = (float)fr.num / (float)fr.den;
	printf("Framerate: %.2ffps\n", clip->framerate);

	// Use av_reduce() to reduce a fraction to get the width and height of the clip's aspect ratio
	av_reduce(&clip->aspectRatioW, &clip->aspectRatioH,
	          clip->videoCodecCtx->width,
	          clip->videoCodecCtx->height,
	          24);
	clip->aspectRatioF = (float)clip->aspectRatioW / (float)clip->aspectRatioH;
	printf("Aspect Ratio: (%f), [%d:%d]\n", clip->aspectRatioF, 
	       clip->aspectRatioW, clip->aspectRatioH);

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

void drawBorder(SDL_Renderer *renderer, Border b)
{
	SDL_RenderFillRect(renderer, &b.top);
	SDL_RenderFillRect(renderer, &b.bot);
	SDL_RenderFillRect(renderer, &b.left);
	SDL_RenderFillRect(renderer, &b.right);
}

enum BorderSide
{
	BORDER_SIDE_INSIDE,
	BORDER_SIDE_OUTSIDE,
};

// This creates a border who's top and bottom are designated on the specified border side
void buildBorder(Border *b, SDL_Rect *r, BorderSide bs)
{
	int height = 20;
	int width = 4;

	if(bs == BORDER_SIDE_INSIDE)
	{
		b->top.x = r->x;
		b->top.y = r->y;
		b->top.w = r->w;
		b->top.h = height;

		b->bot.x = r->x;
		b->bot.y = r->y + r->h - height;
		b->bot.w = r->w;
		b->bot.h = height; 

		b->left.x = r->x - width;
		b->left.y = r->y;
		b->left.w = width;
		b->left.h = r->h;

		b->right.x = r->x + r->w;
		b->right.y = r->y;
		b->right.w = width;
		b->right.h = r->h;
	}
	else if(bs == BORDER_SIDE_OUTSIDE)
	{
		b->top.x = r->x - width;
		b->top.y = r->y - height;
		b->top.w = r->w + (width * 2);
		b->top.h = height;

		b->bot.x = r->x - width;
		b->bot.y = r->y + r->h;
		b->bot.w = r->w + (width * 2);
		b->bot.h = height; 

		b->left.x = r->x - width;
		b->left.y = r->y;
		b->left.w = width;
		b->left.h = r->h;

		b->right.x = r->x + r->w;
		b->right.y = r->y;
		b->right.w = width;
		b->right.h = r->h;
	}
}

// TODO: Still a bit janky, including borders
void resizeAllWindowElements(SDL_Window *window, 
                             SDL_Rect *currentViewBack, SDL_Rect *compositeViewBack,
                             SDL_Rect *currentView, SDL_Rect *compositeView, 
                             SDL_Rect *browserView,
                             SDL_Rect *timelineView,
                             SDL_Rect *effectsView,
                             Border   *currentBorder,
                             Border   *compositeBorder,
                             Border *timelineBorder,
                             Border *browserBorder,
                             Border *effectsBorder,
                             VideoClip *clip)
{
	const int bufferSpace = 5;

	int windowWidth, windowHeight, hwidth, hheight, backViewW, backViewH, 
	currentViewW, currentViewH, compositeViewW, compositeViewH;

	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	hwidth = windowWidth / 2;
	hheight = windowHeight / 2;

	// Find the nearest 16x9 (STC) resolution to fit the current and composite views into
	// STC: Need to make the aspect ratio the ar of the output composition.
	for(int w = 0, h = 0; 
	    w < (hwidth - (bufferSpace * 8)) && h < (hheight - (bufferSpace * 8));
	    w += 16, h += 9)
	{
		backViewW = w;
		backViewH = h;
	}

	// Create the view rectangles for the background of the current and composite views
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

  // (??? STC ???) <
	// If the clip is bigger than the back views then we just resize it to be the back view
	// and "fit it to frame". Otherwise we keep the clip at it's original size.
	// NOTE: We may want to test this, perhaps clips that are bigger than the size of the wanted
	// composite should be clipped outside of the composite... 
	if(clip->width > backViewW || clip->height > backViewH)
	{
		// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
		for(int w = 0, h = 0; 
		    w <= currentViewBack->w && h <= currentViewBack->h; 
		    w += clip->aspectRatioW, h += clip->aspectRatioH)
		{
			currentViewW = w;
			currentViewH = h;
		}

		// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
		for(int w = 0, h = 0; 
		    w <= compositeViewBack->w && h <= compositeViewBack->h; 
		    w += clip->aspectRatioW, h += clip->aspectRatioH)
		{
			compositeViewW = w;
			compositeViewH = h;
		}
	}
	else
	{
		currentViewW = clip->videoCodecCtx->width;
		currentViewH = clip->videoCodecCtx->height;

		compositeViewW = clip->videoCodecCtx->width;
		compositeViewH = clip->videoCodecCtx->height;
	}
	// (??? STC ???)) >

	// Put the current clip view on the right hand side and center it in the current view background
	currentView->w = currentViewW;
	currentView->h = currentViewH;
	currentView->x = currentViewBack->x + ((currentViewBack->w - currentView->w) / 2);
	currentView->y = currentViewBack->y + ((currentViewBack->h - currentView->h) / 2);
	buildBorder(currentBorder, currentViewBack, BORDER_SIDE_OUTSIDE);

	// Put the composite clip view on the left hand side and center it in the composite 
	// view background
	compositeView->w = compositeViewW;
	compositeView->h = compositeViewH;
	compositeView->x = compositeViewBack->x + ((compositeViewBack->w - compositeView->w) / 2);
	compositeView->y = compositeViewBack->y + ((compositeViewBack->h - compositeView->h) / 2);
	buildBorder(compositeBorder, compositeViewBack, BORDER_SIDE_OUTSIDE);

	// Put the file browser view in the bottom right portion of the screen
	browserView->x = bufferSpace;
	browserView->y = (windowHeight / 2) + bufferSpace;
	browserView->w = (windowWidth / 6) - (bufferSpace * 2);
	browserView->h = (windowHeight / 2) - (bufferSpace * 2);
	buildBorder(browserBorder, browserView, BORDER_SIDE_INSIDE);

	// Put the timeline clips view in the center bottom portion of the screen between the file 
	// browser view and the track effects view
	timelineView->x = (windowWidth / 6) + bufferSpace;
	timelineView->y = (windowHeight / 2) + bufferSpace;
	timelineView->w = ((windowWidth / 6) * 4) - (bufferSpace * 2);
	timelineView->h = (windowHeight / 2) - (bufferSpace * 2);
	buildBorder(timelineBorder, timelineView, BORDER_SIDE_INSIDE);

	// Put the track effects view in the bottom portion of the screen to the left of the 
	// timeline view
	effectsView->x = ((windowWidth / 6) * 5) + bufferSpace;
	effectsView->y = (windowHeight / 2) + bufferSpace;
	effectsView->w = (windowWidth / 6) - (bufferSpace * 2);
	effectsView->h = (windowHeight / 2) - (bufferSpace * 2);
	buildBorder(effectsBorder, effectsView, BORDER_SIDE_INSIDE);
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
				case SDL_WINDOWEVENT_RESIZED:
				{
					resizeAllWindowElements(window, 
					                        &currentViewBack, &compositeViewBack,
					                        &currentView, &compositeView,
					                        &browserView,
					                        &timelineView,
					                        &effectsView,
					                        &currentBorder,
					                        &compositeBorder,
					                        &timelineBorder,
					                        &browserBorder,
					                        &effectsBorder,
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

void setRenderColor(SDL_Renderer *renderer, tColor color, uint8 alpha)
{
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void setRenderColor(SDL_Renderer *renderer, tColor color)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

const char *Anime404mp4 = "../res/Anime404.mp4";
const char *Anime404webm= "../res/Anime404.webm";
const char *dance = "../res/dance.mp4";
const char *froggy = "../res/froggy.gif";
const char *groggy = "../res/groggy.gif";
const char *h2b = "../res/h2b.mkv";
const char *kiloshelos = "../res/kiloshelos.mp4";
const char *nggyu = "../res/nggyu.mp4";
const char *pepsimanmvk = "../res/PEPSI-MAN.mkv";
const char *pepsimanmp4 = "../res/PEPSI-MAN.mp4";
const char *ruskie = "../res/ruskie.webm";
const char *trump = "../res/trump.webm";
const char *vapor = "../res/vapor.webm";
const char *watamote = "../res/watamote.webm";

int main(int argc, char **argv)
{
	printf("Hello world.\n");

	av_register_all();

	SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow("Mouse", 
	                          SDL_WINDOWPOS_CENTERED, 
	                          SDL_WINDOWPOS_CENTERED, 
	                          1920, 1080, 
	                          SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_MAXIMIZED);
	int windowWidth, windowHeight;

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Event event = {};

	const char *fname;
	if(argv[1]) fname = argv[1];
	else fname = Anime404mp4;

	// TODO: List of clips to pass around
	VideoClip clip0 = {};
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
	                        &currentBorder,
	                        &compositeBorder,
	                        &timelineBorder,
	                        &browserBorder,
	                        &effectsBorder,
	                        &clip0);

	Global_Paused = true; // DEBUG
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // DEBUG

	while(Global_Running)
	{
		HandleEvents(event, &clip0);
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		if(!Global_Paused)
		{
			playVideoClip(&clip0);
		}

		//printf("current frame: %d\n", clip0.currentFrame);

		SDL_SetRenderDrawColor(renderer, 
		                       tcBackground.r, tcBackground.g, tcBackground.b, tcBackground.a);
		SDL_RenderClear(renderer);

		//testBufferRects(renderer, windowWidth, windowHeight);

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &currentViewBack);
		SDL_RenderFillRect(renderer, &compositeViewBack);

		setRenderColor(renderer, tcRed);
		SDL_RenderFillRect(renderer, &currentView);
		setRenderColor(renderer, tcBlue);
		SDL_RenderFillRect(renderer, &compositeView);

		// TODO: Render copy list of videos....
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &currentView);
		SDL_RenderCopy(renderer, clip0.texture, &clip0.srcRect, &compositeView);

		setRenderColor(renderer, tcView);
		SDL_RenderFillRect(renderer, &browserView);
		SDL_RenderFillRect(renderer, &timelineView);
		SDL_RenderFillRect(renderer, &effectsView);

		
		setRenderColor(renderer, tcBorder);
		drawBorder(renderer, currentBorder);
		drawBorder(renderer, compositeBorder);
		drawBorder(renderer, browserBorder);
		drawBorder(renderer, timelineBorder);
		drawBorder(renderer, effectsBorder);

		SDL_RenderPresent(renderer);
	}

	SDL_Quit();

	freeVideoClipFull(&clip0);
	
	printf("Goodbye.\n");

	return 0;
}