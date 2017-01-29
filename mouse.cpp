// TODO Fix memory leak and crashing errors when dragging and dropping clips

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "resource.h"

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
#include "util.h"

#include "datatypes.h"
#include "colors.h"

#include "ui.h"
#include "video.h"
#include "audio.h"

#include "testvideos.h"

global int Global_screenWidth = 1920;
global int Global_screenHeight = 1080;
global SDL_Rect Global_screenRect = {};

global ViewRects Global_views = {};

global VideoFile Global_videoFile = {};
global VideoClip Global_videoClip = {};

global bool Global_running = true;
global bool Global_paused = true;
global bool Global_flush = false;
global int  Global_playIndex = 0;
global int  Global_clipNumbers = 0;

global SDL_Window   *Global_window;
global SDL_Renderer *Global_renderer;

global bool Global_drawClipBoundRect = true;

global SDL_AudioDeviceID Global_AudioDeviceID = {};
global SDL_AudioSpec Global_AudioSpec = {};

struct Mouse
{
	int32 x;
	int32 y;
	SDL_Point click;
	bool  down;
};

inline Mouse createMouse()
{
	Mouse mouse = {0};
	mouse.click = {-1, -1};
	return mouse;
}

internal void newClip(const char *name)
{
	Global_playIndex = 0;
	freeVideoClip(&Global_videoClip);
	freeVideoFile(&Global_videoFile);
	loadVideoFile(&Global_videoFile, Global_renderer, name);
	printVideoFileInfo(Global_videoFile);
	createVideoClip(&Global_videoClip, &Global_videoFile, Global_renderer, Global_clipNumbers++);
	printVideoClipInfo(Global_videoClip);
	layoutWindowElements(Global_window, &Global_views, &Global_videoClip, Global_playIndex);
}

// NOTE: As of this point, refreshing all window elements is actually fast enough when updating the
// clips. This is not really a problem right now as the code to do this is really only for testing.
// Remember, however, any time a new clip is loaded the rectangles for the composite view and
// current views must be updated so it can resize the clip's aspect ratio correctly.
internal void HandleEvents(Mouse *mouse, SDL_Event event, VideoClip *clip, char **fname)
{
	SDL_PumpEvents();
	SDL_GetMouseState(&mouse->x, &mouse->y);
	if(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT) Global_running = false;
		if(event.type == SDL_MOUSEMOTION)
		{
			SDL_GetMouseState(&mouse->x, &mouse->y);

			if(mouse->down)
			{
				if(SDL_PointInRect(&mouse->click, &Global_videoClip.tlRect))
				{
					float percent = (float)(mouse->x - clip->tlRect.x) / clip->tlRect.w;
					int wantedFrame = (float)clip->endFrame * percent;
					if(wantedFrame < 0) wantedFrame = 0;
					if(wantedFrame > clip->endFrame) wantedFrame = clip->endFrame;
					if(seekToAnyFrame(&Global_videoClip, wantedFrame, Global_playIndex))
					{
						Global_playIndex = wantedFrame;
						setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
					}
				}
			}
		}
		if(event.type == SDL_MOUSEBUTTONDOWN) 
		{
			mouse->down = true;
			mouse->click.x = mouse->x;
			mouse->click.y = mouse->y;
		}
		if(event.type == SDL_MOUSEBUTTONUP)
		{
			mouse->down = false;
			if(SDL_PointInRect(&mouse->click, &Global_videoClip.tlRect))
			{
				float percent = (float)(mouse->x - clip->tlRect.x) / clip->tlRect.w;
				int wantedFrame = (float)clip->endFrame * percent;
				if(wantedFrame < 0) wantedFrame = 0;
				if(wantedFrame > clip->endFrame) wantedFrame = clip->endFrame;
				if(seekToAnyFrame(&Global_videoClip, wantedFrame, Global_playIndex))
				{
					Global_playIndex = wantedFrame;
					setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
				}
			}
			mouse->click.x = -1;
			mouse->click.y = -1;
		}
		if(event.type == SDL_KEYDOWN)
		{
			SDL_Keycode key = event.key.keysym.sym;
			switch(key)
			{
				case SDLK_ESCAPE:
				{
					Global_running = false;
				} break;
				case SDLK_SPACE:
				{
					if(!Global_paused) Global_paused = true;
					else Global_paused = false;
				} break;
				case SDLK_RIGHT:
				case SDLK_f:
				{
					if(!Global_paused) Global_paused = true;
					int index = Global_playIndex + 1;
					if(index <= Global_videoClip.endFrame)
					{
						bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
						if(result)
						{
							++Global_playIndex;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						} 
					}
				} break;
				case SDLK_LEFT:
				case SDLK_d:
				{
					if(!Global_paused) Global_paused = true;
					int index = Global_playIndex - 1;
					if(index >= 0)
					{
						bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
						if(result)
						{
							--Global_playIndex;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						}
					}
				} break;
				case SDLK_r:
				case SDLK_UP:
				{
					if(!Global_paused) Global_paused = true;
					int index = Global_playIndex + 10;
					if(index < Global_videoClip.endFrame - 10)
					{
						bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
						if(result)
						{
							Global_playIndex += 10;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						}
					}
					else
					{
						bool result = seekToAnyFrame(&Global_videoClip, Global_videoClip.endFrame, 
						                             Global_playIndex);
						if(result)
						{
							Global_playIndex = Global_videoClip.endFrame;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						}
					}
				} break;
				case SDLK_e:
				case SDLK_DOWN:
				{
					if(!Global_paused) Global_paused = true;
					int index = Global_playIndex - 10;
					if(Global_playIndex > 10)
					{
						bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
						if(result)
						{
							Global_playIndex -= 10;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						}
					}
					else
					{
						bool result = seekToAnyFrame(&Global_videoClip, 0, Global_playIndex);
						if(result)
						{
							Global_playIndex = 0;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						}
					}
				} break;
				case SDLK_HOME:
				{
					if(!Global_paused) Global_paused = true;
					bool result = seekToAnyFrame(&Global_videoClip, 0, Global_playIndex);
					if(result)
					{
						Global_playIndex = 0;
						setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						printf("Play index: %d\n", Global_playIndex);
					}
				} break;
				case SDLK_END:
				{
					if(!Global_paused) Global_paused = true;
					bool result = seekToAnyFrame(&Global_videoClip, 
					                             Global_videoClip.endFrame, Global_playIndex);
					if(result)
					{
						Global_playIndex = Global_videoClip.endFrame;
						setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
						printf("Play index: %d\n", Global_playIndex);
					}
				} break;
				case SDLK_b:
				{
					if(!Global_drawClipBoundRect) Global_drawClipBoundRect =  true;
					else Global_drawClipBoundRect = false;
				} break;
			}
		}
		if(event.type == SDL_WINDOWEVENT)
		{
			switch(event.window.event)
			{
				case SDL_WINDOWEVENT_RESIZED:
				{
					layoutWindowElements(Global_window, &Global_views, &Global_videoClip, Global_playIndex);
				} break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
				{
					Global_paused = true;
				} break;
			}
		}
		if(event.type == SDL_DROPFILE)
		{
			// TODO Fix drag and drop up so there are no more crashes.
			*fname = event.drop.file;
			Global_playIndex = 0;

			freeVideoClip(&Global_videoClip);
			freeVideoFile(&Global_videoFile);

			loadVideoFile(&Global_videoFile, Global_renderer, *fname);
			printVideoFileInfo(Global_videoFile);
			createVideoClip(&Global_videoClip, &Global_videoFile, Global_renderer, Global_clipNumbers++);
			printVideoClipInfo(Global_videoClip);
			// MUST Layout Window Elements so the video and scrubber are in the correct place
			layoutWindowElements(Global_window, &Global_views, &Global_videoClip, Global_playIndex);
		}
	}
}

void WaitForDroppedFileEvent(SDL_Event event, char **fname, bool *gotFile)
{
	SDL_PumpEvents();
	if(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_DROPFILE:
			{
				*fname = event.drop.file;
				*gotFile = true;
			} break;
			case SDL_KEYDOWN:
			{
				SDL_Keycode key = event.key.keysym.sym;
				if(key == SDLK_ESCAPE) Global_running = false;
			} break;
			case SDL_QUIT:
			{
				Global_running = false;
			} break;
		}
	}
}

int main(int argc, char **argv)
{
	printf("Hello world.\n\n");

	av_register_all();

	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);

	Global_window = SDL_CreateWindow("Mouse", 
	                                 SDL_WINDOWPOS_CENTERED, 
	                                 SDL_WINDOWPOS_CENTERED, 
	                                 Global_screenWidth, Global_screenHeight, 
	                                 SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE
	                                 |SDL_WINDOW_MAXIMIZED);
	int windowWidth, windowHeight;

	Global_renderer = SDL_CreateRenderer(Global_window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Event event = {};

	Global_screenRect.x = 0;
	Global_screenRect.y = 0;
	Global_screenRect.w = Global_screenWidth;
	Global_screenRect.h = Global_screenHeight;

	char *fname = "";
	// If we have an argument then we try to use it as a filename, otherwise we go to the loop.
	if(argv[1]) fname = argv[1];
	else
	{
		bool gotFile = false;
		// We didn't get a file from argv[1] so open the window and render it and wait for a file
		// to be dropped on the window.
		while(!gotFile && Global_running)
		{
			WaitForDroppedFileEvent(event, &fname, &gotFile);
			SDL_SetRenderDrawColor(Global_renderer, 
			                       tcBackground.r, 
			                       tcBackground.g, 
			                       tcBackground.b, 
			                       tcBackground.a);
			SDL_RenderClear(Global_renderer);
			SDL_RenderPresent(Global_renderer);
		}

		// Never got a filename (user gave an exit code without dropping a file).
		// Just skip everything and exit.
		if(!gotFile) return -1;
	}

	// Global_AudioDeviceID = initAudioDevice(Global_AudioSpec); WARNING XXX FIXME Breaks SDL_Quit
	loadVideoFile(&Global_videoFile, Global_renderer, fname); 
	printVideoFileInfo(Global_videoFile);
	createVideoClip(&Global_videoClip, &Global_videoFile, Global_renderer, Global_clipNumbers++);
	printVideoClipInfo(Global_videoClip);
	// MUST Layout Window Elements so the video and scrubber are in the correct place
	layoutWindowElements(Global_window, &Global_views, &Global_videoClip, Global_playIndex);

	TTF_Init();

	if(!TTF_WasInit())
	{
		printf("Could not initalize TTF library!\nExiting.\n");
		return -1;
	}

	TTF_Font *fontDroidSansMono24 = TTF_OpenFont("../res/fonts/DroidSansMono.ttf", 24);
	TTF_Font *fontDroidSansMono32 = TTF_OpenFont("../res/fonts/DroidSansMono.ttf", 32);

	SDL_Surface *playIndexSurface;
	SDL_Surface *filenameSurface;
	SDL_Surface *ticksElapsedSurface;
	SDL_Surface *mousePosSurface;

	Mouse mouse = createMouse();

	float ticksElapsed = 0.0f;
	int startTicks = SDL_GetTicks();
	while(Global_running)
	{
		HandleEvents(&mouse, event, &Global_videoClip, &fname);

		SDL_GetWindowSize(Global_window, &windowWidth, &windowHeight);

		SDL_SetRenderDrawColor(Global_renderer, 
		                       tcBackground.r, 
		                       tcBackground.g, 
		                       tcBackground.b, 
		                       tcBackground.a);
		SDL_RenderClear(Global_renderer);

		setRenderColor(Global_renderer, tcBlack);
		SDL_RenderFillRect(Global_renderer, &Global_views.background);

		//setRenderColor(Global_renderer, tcView);
		//SDL_RenderFillRect(Global_renderer, &Global_views.timeline);
		#if 1
		if(!Global_paused)
		{
			int endTicks = SDL_GetTicks();
			ticksElapsed += (float)endTicks - (float)startTicks;
			if(ticksElapsed >= 
			   (Global_videoClip.vfile->msperframe - (Global_videoClip.vfile->msperframe * 0.05f)))
			{
				int res = decodeSingleFrame(&Global_videoClip);
				if(res > 0)
				{
					updateVideoClipTexture(&Global_videoClip);
					Global_playIndex++;
					setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
				}
				else
				{
					Global_flush = true;
				}
				ticksElapsed = 0;
			}
		}
		startTicks = SDL_GetTicks();
		#endif

		int filenameTextHeight;
		TTF_SizeText(fontDroidSansMono24, Global_videoClip.filename, NULL, &filenameTextHeight);
		int fnameTextX = Global_views.background.x;
		int fnameTextY = Global_views.background.y - filenameTextHeight - 8;
		DrawText(Global_renderer, fontDroidSansMono24, fnameTextX, fnameTextY, 
		         Global_videoClip.filename, SDLC_white);

		int playHeadW;
		char playheadTextBuffer[32];
		sprintf(playheadTextBuffer, "%d", Global_playIndex);
		TTF_SizeText(fontDroidSansMono24, playheadTextBuffer, &playHeadW, NULL);
		int playHeadX = Global_views.scrubber.x - (playHeadW / 2);
		int playHeadY = Global_views.scrubber.y + Global_views.scrubber.h;
		DrawText(Global_renderer, fontDroidSansMono24, playHeadX, playHeadY,
		         playheadTextBuffer, SDLC_white);

		// < DEBUG
		#if 0
		char ticksElapsedBuffer[32];
		if(ticksElapsed < 10.0f) sprintf(ticksElapsedBuffer, "Ticks:  %.4f", ticksElapsed);
		else sprintf(ticksElapsedBuffer, "Ticks: %.4f", ticksElapsed);
		if(ticksElapsedSurface = TTF_RenderText_Blended(fontDroidSansMono24,
		                                                ticksElapsedBuffer, SDLC_white))
		{
			int w;
			TTF_SizeText(fontDroidSansMono24, "Ticks: 00.0000", &w, NULL);
			SDL_Rect ticksElapsedRect;
			ticksElapsedRect.w = ticksElapsedSurface->w;
			ticksElapsedRect.h = ticksElapsedSurface->h;
			ticksElapsedRect.x = Global_views.background.x + 
				Global_views.background.w - w;
			ticksElapsedRect.y = Global_views.background.y - ticksElapsedRect.h;

			SDL_Texture *ticksElapsedTexture = SDL_CreateTextureFromSurface(Global_renderer, 
			                                                                ticksElapsedSurface);
			SDL_RenderCopy(Global_renderer, ticksElapsedTexture, NULL, &ticksElapsedRect);
			SDL_DestroyTexture(ticksElapsedTexture);
			SDL_FreeSurface(ticksElapsedSurface);
		}
		#endif
		// > DEBUG

		// < DEBUG
		#if 0
		char mousePosBuffer[32];
		sprintf(mousePosBuffer, "Mouse: (%d,%d)", Global_mousex, Global_mousey);
		if(mousePosSurface = TTF_RenderText_Blended(fontDroidSansMono24, mousePosBuffer, SDLC_white))
		{
			SDL_Rect mousePosRect;
			mousePosRect.w = mousePosSurface->w;
			mousePosRect.h = mousePosSurface->h;
			mousePosRect.x = 8;
			mousePosRect.y = 2;

			SDL_Texture *mousePosTexture = SDL_CreateTextureFromSurface(Global_renderer, mousePosSurface);

			SDL_RenderCopy(Global_renderer, mousePosTexture, NULL, &mousePosRect);
			SDL_DestroyTexture(mousePosTexture);
			SDL_FreeSurface(mousePosSurface);
		}
		#endif
		// > DEBUG


		SDL_RenderCopy(Global_renderer, Global_videoClip.texture, NULL, 
		               (SDL_Rect *)&Global_videoClip.videoRect);

		setRenderColor(Global_renderer, tcView);
		SDL_RenderFillRect(Global_renderer, &Global_videoClip.tlRect);

		setRenderColor(Global_renderer, tcRed);
		SDL_RenderFillRect(Global_renderer, &Global_views.scrubber);

		if(Global_drawClipBoundRect)
		{
			// drawClipBoundRect(Global_renderer, Global_videoClip, tcRed, 5); // DEBUG
		}

		SDL_RenderPresent(Global_renderer);
	}

	freeVideoClip(&Global_videoClip);
	freeVideoFile(&Global_videoFile);

	TTF_CloseFont(fontDroidSansMono24);
	TTF_CloseFont(fontDroidSansMono32);

	TTF_Quit();

	//assert(Global_AudioDeviceID != 0);
	//SDL_CloseAudioDevice(Global_AudioDeviceID);
	SDL_CloseAudio();

	SDL_Quit();
	
	printf("\nGoodbye.\n");

	return 0;
}

