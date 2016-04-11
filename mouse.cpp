#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

// TODO: Get rid of all of this DLL nonsense and statically compile all dependencies
// so they are built directly into the .exe with /MT.

#include <SDL2/SDL.h>
#include <SDL2/SDL_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>

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

#include "ui.h"
#include "video.h"


global Border currentBorder, compositeBorder,
timelineBorder, browserBorder, effectsBorder;

struct AudioClip
{
	// TODO: Create audio only clips
	AVFormatContext    *formatCtx;
	AVCodecContext     *codecCtx;
	AVCodec            *codec;
	AVStream           *stream;
	AVFrame            *frame;
	AVPacket            packet;
	SDL_AudioSpec       audioSpec;
	int                 streamIndex;
	const char         *filename;
	int                 bitrate;
	int                 samplerate;
	bool								loop;
};

struct AVClip
{
	// TODO: Create audio and video clips
};

global ViewRects Global_Views = {};

global VideoClip Global_VideoClip = {};

global const char *Anime404mp4 = "../res/video/Anime404.mp4";
global const char *Anime404webm= "../res/video/Anime404.webm";
global const char *dance =       "../res/video/dance.mp4";
global const char *froggy =      "../res/video/froggy.gif";
global const char *groggy =      "../res/video/groggy.gif";
global const char *h2bmkv =      "../res/video/h2b.mkv"; // FIXME: Does not decode correctly
global const char *h2bmp4 =      "../res/video/h2b.mp4";
global const char *kiloshelos =  "../res/video/kiloshelos.mp4";
global const char *nggyu =       "../res/video/nggyu.mp4";
global const char *ruskie =      "../res/audio/ruskie.webm";
global const char *trump =       "../res/video/trump.webm";
global const char *vapor =       "../res/video/vapor.webm";
global const char *watamote =    "../res/video/watamote.webm";

global WindowLayout Global_Layout = LAYOUT_SINGLE;

global bool Global_Running = true;
global bool Global_Paused = false;

global SDL_Window *Global_Window;
global SDL_Renderer *Global_Renderer;

global int Global_mousex = 0, Global_mousey = 0;
global bool Global_mousedown = false;

// TODO: Make these a user setting
global int Global_AspectRatio_W = 16;
global int Global_AspectRatio_H = 9;

global bool Global_Show_Transform_Tool = false;

// TODO: List of view rectangles to pass around


// TODO: DO NOT resizeAllWinowElements() each time a clip is changed
// The reason we do this is because when the clip is reinitialized, the rectangles for the
// views are not updated to hold the original aspect ratio of the clip into a rectangle
// that can be fit inside the backviews of the composite and/or current views.
internal void HandleEvents(SDL_Event event, VideoClip *clip)
{
	SDL_PumpEvents();
	SDL_GetMouseState(&Global_mousex, &Global_mousey);
	if(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT) Global_Running = false;
		if(event.type == SDL_MOUSEMOTION) SDL_GetMouseState(&Global_mousex, &Global_mousey);
		if(event.type == SDL_MOUSEBUTTONDOWN) Global_mousedown = true;
		if(event.type == SDL_MOUSEBUTTONUP) Global_mousedown = false;
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
				case SDLK_RIGHT:
				{
					if(!Global_Paused) Global_Paused = true;
					decodeNextVideoFrame(clip);
					updateVideoClipTexture(clip);
				} break;
				case SDLK_t:
				{
					if(!Global_Show_Transform_Tool) Global_Show_Transform_Tool = true;
					else Global_Show_Transform_Tool = false;
				} break;
				case SDLK_1:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, Anime404mp4, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);

				} break;
				case SDLK_2:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, h2bmp4, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_3:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, kiloshelos, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_4:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, dance, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_5:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, trump, true);
					printVideoClipInfo(Global_VideoClip);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_6:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, nggyu, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_7:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, froggy, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_8:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, groggy, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_9:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, ruskie, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_0:
				{
					freeVideoClip(&Global_VideoClip);
					Global_VideoClip = {};
					initVideoClip(&Global_VideoClip, Global_Renderer, watamote, true);
					printVideoClipInfo(Global_VideoClip);
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
				} break;
				case SDLK_TAB:
				{
					if(Global_Layout != LAYOUT_SINGLE)
					{
						Global_Layout = LAYOUT_SINGLE;
						resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);	
					}
					else if(Global_Layout != LAYOUT_DUAL)
					{
						Global_Layout = LAYOUT_DUAL;
						resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
					}
				} break;
				case SDLK_MINUS:
				{
					
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
					resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);
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
	printf("Hello world.\n\n");

	av_register_all();

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	Global_Window = SDL_CreateWindow("Mouse", 
	                                 SDL_WINDOWPOS_CENTERED, 
	                                 SDL_WINDOWPOS_CENTERED, 
	                                 1024, 576, 
	                                 SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	int windowWidth, windowHeight;

	Global_Renderer = SDL_CreateRenderer(Global_Window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Event event = {};

	const char *fname;
	if(argv[1]) fname = argv[1];
	else fname = Anime404mp4; // DEBUG FILENAME

	// TODO: List of clips to pass around
	initVideoClip(&Global_VideoClip, Global_Renderer, Anime404mp4, true);
	printVideoClipInfo(Global_VideoClip);

	resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);

	Global_Paused = true; // DEBUG
	SDL_SetRenderDrawBlendMode(Global_Renderer, SDL_BLENDMODE_BLEND);

	// TODO: Build a font map
	SDL_Surface *fontSurfaces[94];
	SDL_Texture *fontTextures[94];
	TTF_Font *fontAnaheimRegular = TTF_OpenFont("../res/fonts/Anaheim-Regular.ttf", 16);
	int fontW, fontH;
	SDL_Color color = { 0, 0, 0 }; // SDL_color black
	for(int currentC = 32, currentT = 0; currentC < 126; ++currentC, ++currentT)
	{
		char c = currentC;
		fontSurfaces[currentT] = TTF_RenderGlyph_Blended(fontAnaheimRegular, c, color);
		fontTextures[currentT] = SDL_CreateTextureFromSurface(Global_Renderer, fontSurfaces[currentT]);
	}
	// TTF_SizeText(font, testText, &fontW, &fontH);
	SDL_Surface *textSurface = TTF_RenderText_Blended(fontAnaheimRegular, "Hello world", color);
	SDL_Texture *textTexture = SDL_CreateTextureFromSurface(Global_Renderer, textSurface);
	SDL_Rect textRect;
	textRect.x = 15;
	textRect.y = 10;
	textRect.w = textSurface->w;
	textRect.h = textSurface->h;

	SDL_FreeSurface(textSurface);

	int startTicks = SDL_GetTicks();
	float ticksElapsed = 0.0f;

	while(Global_Running)
	{
		HandleEvents(event, &Global_VideoClip);
		SDL_GetWindowSize(Global_Window, &windowWidth, &windowHeight);

		//printf("current frame: %d\n", Global_VideoClip.currentFrame);

		SDL_SetRenderDrawColor(Global_Renderer, 
		                       tcBackground.r, 
		                       tcBackground.g, 
		                       tcBackground.b, 
		                       tcBackground.a);
		SDL_RenderClear(Global_Renderer);

		setRenderColor(Global_Renderer, tcBlack);
		if(Global_Layout == LAYOUT_DUAL) SDL_RenderFillRect(Global_Renderer, 
		                                                    &Global_Views.currentViewBack);
		SDL_RenderFillRect(Global_Renderer, &Global_Views.compositeViewBack);

		setRenderColor(Global_Renderer, tcRed);
		if(Global_Layout == LAYOUT_DUAL) SDL_RenderFillRect(Global_Renderer, &Global_Views.currentView);
		setRenderColor(Global_Renderer, tcBlue);
		SDL_RenderFillRect(Global_Renderer, &Global_Views.compositeView);

		setRenderColor(Global_Renderer, tcView);
		SDL_RenderFillRect(Global_Renderer, &Global_Views.browserView);
		SDL_RenderFillRect(Global_Renderer, &Global_Views.timelineView);
		SDL_RenderFillRect(Global_Renderer, &Global_Views.effectsView);
		
		setRenderColor(Global_Renderer, tcBorder);
		drawBorder(Global_Renderer, currentBorder);
		drawBorder(Global_Renderer, compositeBorder);
		drawBorder(Global_Renderer, browserBorder);
		drawBorder(Global_Renderer, timelineBorder);
		drawBorder(Global_Renderer, effectsBorder);

		SDL_RenderCopy(Global_Renderer, textTexture, NULL, &textRect);

		if(!Global_Paused)
		{
			int endTicks = SDL_GetTicks();
			ticksElapsed += (float)endTicks - (float)startTicks;
			if(ticksElapsed >= Global_VideoClip.msperframe)
			{
				decodeNextVideoFrame(&Global_VideoClip);
				updateVideoClipTexture(&Global_VideoClip);
				ticksElapsed = 0;
			}
		}
		startTicks = SDL_GetTicks();

		// TODO: Render copy list of videos....
		if(Global_Layout == LAYOUT_DUAL)
		{
			SDL_RenderSetClipRect(Global_Renderer, &Global_Views.currentView);
			SDL_RenderCopy(Global_Renderer, Global_VideoClip.texture, &Global_VideoClip.srcRect, 
			               &Global_Views.currentView);
		}
		SDL_RenderSetClipRect(Global_Renderer, &Global_Views.compositeView);
		SDL_RenderCopy(Global_Renderer, Global_VideoClip.texture, &Global_VideoClip.srcRect, 
		               &Global_Views.compositeView);

		SDL_RenderSetClipRect(Global_Renderer, NULL);

		if(Global_Show_Transform_Tool)
		{
			drawClipTransformControls(Global_Renderer, &Global_VideoClip);
		}

		SDL_RenderPresent(Global_Renderer);
	}

	TTF_CloseFont(fontAnaheimRegular);

	SDL_Quit();
	TTF_Quit();

	freeVideoClipFull(&Global_VideoClip);
	
	printf("\nGoodbye.\n");

	return 0;
}

