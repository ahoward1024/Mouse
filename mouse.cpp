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

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

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
global const char *testwebm = "../res/video/test/testxvp9.webm";

// Exactly 240 black frames at 60 fps
const char *black240raw  = "../res/video/test/black240raw.avi";
const char *black240avi  = "../res/video/test/black240x264.avi";
const char *black240mp4  = "../res/video/test/black240x264.mp4";
const char *black240webm = "../res/video/test/black240vp9.webm";

// TEST LONG FILE (WARNING: ONLY ON QUASAR)
const char *pm4 =  "H:\\Fraps\\Movies\\MISC\\pm4 - 2.avi";

// TEST VIDEO FILES
global const char *anime404mp4    = "../res/video/Anime404.mp4";
global const char *anime404webm   = "../res/video/Anime404.webm";
global const char *anime404AME   = "../res/video/Anime404_AME.mp4";
global const char *a404test       = "../res/video/a404test.mp4";
global const char *dance          = "../res/video/dance.mp4";
global const char *froggy         = "../res/video/froggy.gif";
global const char *groggy         = "../res/video/groggy.gif";
global const char *h2bmkv         = "../res/video/h2b.mkv"; // NOTE: This file is broken
global const char *h2bmp4         = "../res/video/h2b.mp4";
global const char *kiloshelos     = "../res/video/kiloshelos.mp4";
global const char *nevercomedown  = "../res/video/nevercomedown.mkv";
global const char *nggyu          = "../res/video/nggyu.mp4";
global const char *trump          = "../res/video/trump.webm";
global const char *trumpncd       = "../res/video/trump-nevercomedown.webm";
global const char *vapor          = "../res/video/vapor.webm";
global const char *watamote       = "../res/video/watamote.webm";

// TEST AUDIO FILES
global const char *intoxicated    = "../res/audio/Intoxicated.mp3";
global const char *ncdmp3         = "../res/audio/nevercomedown.mp3";
global const char *ringmp3        = "../res/audio/Ring.mp3";
global const char *ruskie         = "../res/audio/ruskie.webm";
global const char *someonenew     = "../res/audio/Someone New.mp3";
global const char *switchfriends  = "../res/audio/Switch Friends.wav";

global bool Global_running = true;
global bool Global_paused = false;
global int  Global_playIndex = 0;
global int  Global_clipNumbers = 0;

global SDL_Window   *Global_window;
global SDL_Renderer *Global_renderer;

global int  Global_mousex = 0, Global_mousey = 0;
global bool Global_mousedown = false;
global bool Global_drawClipBoundRect = true;

global SDL_AudioDeviceID Global_AudioDeviceID = {};
global SDL_AudioSpec Global_AudioSpec = {};

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
				{
					if(!Global_paused) Global_paused = true;
					if(Global_playIndex < Global_videoClip.endFrame)
					{
							int index = Global_playIndex + 1;
							bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
						  if(result)
						  {
						  	++Global_playIndex;
						  	printf("Play index: %d\n", Global_playIndex);
						  } 
					}
					else
					{
						printf("\nEnd of video\n\n");
					}
				} break;
				case SDLK_LEFT:
				{
					if(!Global_paused) Global_paused = true;
					#if 1
					if(Global_playIndex > 0)
					{
						int index = Global_playIndex - 1;
						bool result = seekToAnyFrame(&Global_videoClip, index, Global_playIndex);
					  if(result)
					  {
							printf("Play index: %d\n", Global_playIndex);
							--Global_playIndex;
					  }
					}
					else
					{
						printf("\nBeginning of video\n\n");
					}
					#endif
				} break;
				case SDLK_r:
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
					layoutWindowElements(Global_window, &Global_views, &Global_videoClip);
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
	else fname = nggyu; // DEBUG FILENAME

	// Global_AudioDeviceID = initAudioDevice(Global_AudioSpec); WARNING XXX FIXME Breaks SDL_Quit

	loadVideoFile(&Global_videoFile, Global_renderer, anime404mp4); // TEST XXX
	printVideoFileInfo(Global_videoFile);
	createVideoClip(&Global_videoClip, &Global_videoFile, Global_renderer, Global_clipNumbers++);
	printVideoClipInfo(Global_videoClip);

	if(!(SDL_GetWindowFlags(Global_window) & SDL_WINDOW_MAXIMIZED))
		layoutWindowElements(Global_window, &Global_views, &Global_videoClip);

	SDL_SetRenderDrawBlendMode(Global_renderer, SDL_BLENDMODE_BLEND);

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
				if(Global_playIndex < Global_videoClip.endFrame)
				{
					printf("Play index: %d\n", Global_playIndex);
					decodeSingleFrame(&Global_videoClip);
					updateVideoClipTexture(&Global_videoClip);
					Global_playIndex++;
				}
				else
				{
					Global_paused = true;
				}
				ticksElapsed = 0;
			}
		}
		startTicks = SDL_GetTicks();
		#endif

		SDL_RenderCopy(Global_renderer, Global_videoClip.texture, NULL, 
		               (SDL_Rect *)&Global_videoClip.videoRect);

		if(Global_drawClipBoundRect)
		{
			drawClipBoundRect(Global_renderer, Global_videoClip, tcRed, 5);
		}

		SDL_RenderPresent(Global_renderer);
	}

	freeVideoClip(&Global_videoClip);
	freeVideoFile(&Global_videoFile);

	//assert(Global_AudioDeviceID != 0);
	//SDL_CloseAudioDevice(Global_AudioDeviceID);
	SDL_CloseAudio();

	SDL_Quit();
	
	printf("\nGoodbye.\n");

	return 0;
}

