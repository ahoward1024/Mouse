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
#include "util.h"

#include "datatypes.h"
#include "colors.h"

#include "ui.h"
#include "video.h"
#include "audio.h"


struct AVClip
{
	// TODO: Create audio and video clips
};

global int Global_screenWidth = 1920;
global int Global_screenHeight = 1080;
global SDL_Rect Global_screenRect = {};

global ViewRects Global_views = {};

global VideoFile Global_videoFile = {};
global VideoClip Global_videoClip = {};

// TEST VIDEO: 1 frame of RED, 2 frames of BLUE, 2 frames of GREEN, 1 frame of YELLOW
// 6 frames total, at 60 fps
// NOTE: testavi is UNCOMPRESSED!! testmp4 is compressed with the x264 codec
global const char *testraw  = "../res/video/test/testraw.avi";
global const char *testmp4  = "../res/video/test/testx264.mp4";
global const char *testavi  = "../res/video/test/testx264.avi";

// Exactly 240 black frames at 60 fps
const char *black240raw  = "../res/video/test/black240raw.avi";
const char *black240avi  = "../res/video/test/black240x264.avi";
const char *black240mp4  = "../res/video/test/black240x264.mp4";

// TEST LONG FILE (WARNING: ONLY ON QUASAR)
const char *pm4 =  "H:\\Fraps\\Movies\\MISC\\pm4 - 2.avi";

// TEST VIDEO FILES
global const char *anime404       = "../res/video/Anime404.mp4";
global const char *a404test       = "../res/video/a404test.mp4";
global const char *dance          = "../res/video/dance.mp4";
global const char *nggyu          = "../res/video/nggyu.mp4";
global const char *vapor          = "../res/video/vapor.mp4";
global const char *watamote       = "../res/video/watamote.mp4";
// TEST AUDIO FILES
global const char *nevercomedown  = "../res/audio/nevercomedown.mp3";
global const char *ruskie         = "../res/audio/ruskie.webm";
global const char *someonenew     = "../res/audio/someonenew.mp3";
global const char *switchfriends  = "../res/audio/switchfriends.wav";

global bool Global_running = true;
global bool Global_paused = true;
global bool Global_flush = false;
global int  Global_playIndex = 0;
global int  Global_clipNumbers = 0;

global SDL_Window   *Global_window;
global SDL_Renderer *Global_renderer;

global int  Global_mousex = 0, Global_mousey = 0;
global bool Global_mousedown = false;
global bool Global_drawClipBoundRect = true;

global SDL_AudioDeviceID Global_AudioDeviceID = {};
global SDL_AudioSpec Global_AudioSpec = {};

static void newClip(const char *name)
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
internal void HandleEvents(SDL_Event event, VideoClip *clip)
{
	SDL_PumpEvents();
	SDL_GetMouseState(&Global_mousex, &Global_mousey);
	if(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT) Global_running = false;
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
							printf("Play index: %d\n", Global_playIndex);
						} 
					}
					else
					{
						printf("\nEnd of video\n\n");
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
							printf("Play index: %d\n", Global_playIndex);
						}
					}
					else
					{
						printf("\nBeginning of video\n\n");
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
							printf("Play index: %d\n", Global_playIndex);
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
							printf("Play index: %d\n", Global_playIndex);
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
							printf("Play index: %d\n", Global_playIndex);
						}
					}
					else
					{
						bool result = seekToAnyFrame(&Global_videoClip, 0, Global_playIndex);
						if(result)
						{
							Global_playIndex = 0;
							setScrubberXPosition(Global_videoClip, &Global_views, Global_playIndex);
							printf("Play index: %d\n", Global_playIndex);
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
				case SDLK_0:
				{
					newClip(switchfriends);
				} break;
				case SDLK_1:
				{
					newClip(anime404);
				} break;
				case SDLK_2:
				{
					newClip(a404test);
				} break;
				case SDLK_3:
				{
					newClip(nggyu);
				} break;
				case SDLK_4:
				{
					newClip(dance);
				} break;
				case SDLK_5:
				{
					newClip(vapor);
				} break;
				case SDLK_6:
				{
					newClip(watamote);
				} break;
				case SDLK_7: // TODO(alex): Audio
				{
					// newClip(nevercomedown);
				} break;
				case SDLK_8:
				{
					// newClip(ruskie);
				} break;
				case SDLK_9:
				{
					// newClip(someonenew);
				} break;
				case SDLK_F1:
				{
					newClip(testraw);
				} break;
				case SDLK_F2:
				{
					newClip(testmp4);
				} break;
				case SDLK_F3:
				{
					newClip(testavi);
				} break;
				case SDLK_F4:
				{
					newClip(black240raw);
				} break;
				case SDLK_F5:
				{
					newClip(black240avi);
				} break;
				case SDLK_F6:
				{
					newClip(black240mp4);
				} break;
				case SDLK_F7:
				{
					newClip(pm4); // WARNING!!! LARGE FILE ONLY ON QUASAR
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

	const char *fname;
	if(argv[1]) fname = argv[1];
	else fname = a404test; // // TEST XXX DEBUG

	// Global_AudioDeviceID = initAudioDevice(Global_AudioSpec); WARNING XXX FIXME Breaks SDL_Quit

	loadVideoFile(&Global_videoFile, Global_renderer, fname); 
	printVideoFileInfo(Global_videoFile);
	createVideoClip(&Global_videoClip, &Global_videoFile, Global_renderer, Global_clipNumbers++);
	printVideoClipInfo(Global_videoClip);

	// Refresh the window layout if SDL is set to maximize. We do this check so we don't have to do
	// the expense of recalculating all of the window stuff if SDL is not set to maximize.
	// Also, SDL will open a window at the default specified resolution and _THEN_ maximize the window
	// So we have to do this if the maximize flag is enabled.
	if(!(SDL_GetWindowFlags(Global_window) & SDL_WINDOW_MAXIMIZED))
		layoutWindowElements(Global_window, &Global_views, &Global_videoClip, Global_playIndex);

	// SDL_SetRenderDrawBlendMode(Global_renderer, SDL_BLENDMODE_BLEND);

	TTF_Init();

	if(!TTF_WasInit())
	{
		printf("Could not initalize TTF library!\nExiting.\n");
		return -1;
	}

	TTF_Font *fontAnaheimRegular24 = TTF_OpenFont("../res/fonts/Anaheim-Regular.ttf", 24);
	TTF_Font *fontAnaheimRegular32 = TTF_OpenFont("../res/fonts/Anaheim-Regular.ttf", 32);

	SDL_Surface *playIndexSurface;
	SDL_Surface *filenameSurface;
	SDL_Surface *ticksElapsedSurface;

	float ticksElapsed = 0.0f;
	int startTicks = SDL_GetTicks();
	while(Global_running)
	{
		HandleEvents(event, &Global_videoClip);

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

		char ticksElapsedBuffer[32];
		sprintf(ticksElapsedBuffer, "Ticks: %f", ticksElapsed);
		if(ticksElapsedSurface = TTF_RenderText_Blended(fontAnaheimRegular24,
		                                                ticksElapsedBuffer, SDLC_white))
		{
			SDL_Rect ticksElapsedRect;
			ticksElapsedRect.w = ticksElapsedSurface->w;
			ticksElapsedRect.h = ticksElapsedSurface->h;
			ticksElapsedRect.x = Global_views.background.x + 
				Global_views.background.w - ticksElapsedRect.w;
			ticksElapsedRect.y = Global_views.background.y - ticksElapsedRect.h - 1;

			SDL_Texture *ticksElapsedTexture = SDL_CreateTextureFromSurface(Global_renderer, 
			                                                                ticksElapsedSurface);
			SDL_RenderCopy(Global_renderer, ticksElapsedTexture, NULL, &ticksElapsedRect);
			SDL_DestroyTexture(ticksElapsedTexture);
			SDL_FreeSurface(ticksElapsedSurface);
		}


		if(filenameSurface = TTF_RenderText_Blended(fontAnaheimRegular32,
		                                            Global_videoClip.vfile->formatCtx->filename, 
		                                            SDLC_white));
		{
			SDL_Rect filenameRect;
			filenameRect.w = filenameSurface->w;
			filenameRect.h = filenameSurface->h;
			filenameRect.x = Global_views.background.x;
			filenameRect.y = Global_views.background.y - filenameRect.h - 2;

			SDL_Texture *filenameTexture = SDL_CreateTextureFromSurface(Global_renderer, filenameSurface);
			SDL_RenderCopy(Global_renderer, filenameTexture, NULL, &filenameRect);
			SDL_DestroyTexture(filenameTexture);
			SDL_FreeSurface(filenameSurface);
		}

		char playheadTextBuffer[32];
		sprintf(playheadTextBuffer, "%d", Global_playIndex);
		if(playIndexSurface = TTF_RenderText_Blended(fontAnaheimRegular24, 
		                                             playheadTextBuffer, SDLC_white))
		{
			SDL_Rect playIndexRect;
			playIndexRect.w = playIndexSurface->w;
			playIndexRect.h = playIndexSurface->h;
			playIndexRect.x = Global_views.scrubber.x - (playIndexRect.w / 2);
			playIndexRect.y = Global_views.scrubber.y + Global_views.scrubber.h;

			SDL_Texture *playIndexTexture = SDL_CreateTextureFromSurface(Global_renderer, 
			                                                             playIndexSurface);
			SDL_RenderCopy(Global_renderer, playIndexTexture, NULL, &playIndexRect);
			SDL_DestroyTexture(playIndexTexture);
			SDL_FreeSurface(playIndexSurface);
		}


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

	TTF_Quit();

	//assert(Global_AudioDeviceID != 0);
	//SDL_CloseAudioDevice(Global_AudioDeviceID);
	SDL_CloseAudio();

	SDL_Quit();
	
	printf("\nGoodbye.\n");

	return 0;
}

