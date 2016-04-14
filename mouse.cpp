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

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

struct AudioClip
{
	// TODO: Create audio only clips
	AVFormatContext    *formatCtx;
	AVCodecContext     *codecCtx;
	AVCodec            *codec;
	AVStream           *stream;
	AVFrame            *frame;
	AVPacket            packet;
	AVSampleFormat      sampleFmt;
	struct SwrContext  *swrCtx;
	SDL_AudioSpec       spec;
	int                 streamIndex;
	const char         *filename;
	int                 bitrate;
	int                 samplerate;
	int                 channels;
	uint64              layout;
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

global AudioClip Global_AudioClip = {};
global SDL_AudioDeviceID Global_AudioDeviceID = {};
global SDL_AudioSpec Global_AudioSpec = {};

void outputAudio(void *userdata, uint8 *audiostream, int nbytes)
{ 
	
}

void printAudioClipInfo(AudioClip clip)
{
	printf("INIT AUDIO CLIP ================\n");
	printf("Filename: %s\n", clip.filename);
	printf("Duration: ");
	if (clip.formatCtx->duration != AV_NOPTS_VALUE)
	{
		int64 duration = clip.formatCtx->duration + 
		(clip.formatCtx->duration <= INT64_MAX - 5000 ? 5000 : 0);
		int secs = duration / AV_TIME_BASE;
		int us = duration % AV_TIME_BASE;
		int mins = secs / 60;
		secs %= 60;
		int hours = mins / 60;
		mins %= 60;
		printf("%02d:%02d:%02d.%02d\n", hours, mins, secs, (100 * us) / AV_TIME_BASE);
	}
	if (clip.formatCtx->start_time != AV_NOPTS_VALUE)
	{
		int secs, us;
		printf("Start time: ");
		secs = clip.formatCtx->start_time / AV_TIME_BASE;
		us   = llabs(clip.formatCtx->start_time % AV_TIME_BASE);
		printf("%d.%02d\n", secs, (int) av_rescale(us, 1000000, AV_TIME_BASE));
	}
	printf("Bitrate: %d\n", clip.bitrate);
	printf("Samplerate: %d Hz\n", clip.samplerate);
	printf("Channels: %d\n", clip.channels);
	// So far, the largest string in ffmpeg's channel layout table is 14 characters, so I think
	// allocating for a 32 character string is plenty sufficient (though, who knows???)
	char *chlayoutstr = (char *)malloc(sizeof(char) * 32);
	av_get_channel_layout_string(chlayoutstr, 32, clip.channels, clip.layout);
	printf("Channel Layout: %s\n", chlayoutstr);
	free(chlayoutstr);
	printf("Looped: %s\n", clip.loop ? "true" : "false");
	printf("\n");
}

void initAudioDevice()
{
	// TODO: Make this a user defined setting
	SDL_AudioSpec wantedSpec;
	wantedSpec.freq = 44100; 
	wantedSpec.format = AUDIO_S16SYS; 
	wantedSpec.channels = 2; 
	wantedSpec.silence = 0; 
	wantedSpec.samples = 1024; 
	wantedSpec.callback = outputAudio;
	wantedSpec.userdata = NULL;

	int numAudioDevices = SDL_GetNumAudioDevices(0);
	printf("Available audio devices: \n");
	for(int i = 0; i < numAudioDevices; ++i)
	{
		printf("Device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
	}
	printf("\n");

	// TODO: Get an audio device specified by the user. NULL for the first argument gets the default.
	Global_AudioDeviceID = SDL_OpenAudioDevice(NULL, 0, &wantedSpec, &Global_AudioSpec, 
	                                         SDL_AUDIO_ALLOW_ANY_CHANGE);
	assert(Global_AudioDeviceID != 0);
	SDL_PauseAudioDevice(Global_AudioDeviceID, 1); // Pause the audio so we don't try to output sound!
}

void copyAudioSpec(SDL_AudioSpec *dest, SDL_AudioSpec src)
{
	dest->freq = src.freq;
	dest->format = src.format;
	dest->channels = src.channels;
	dest->silence = src.silence;
	dest->samples = src.samples;
	dest->size = src.size;
	dest->callback = src.callback;
	dest->userdata = NULL;
}

void initAudioClip(AudioClip *clip, const char *filename, bool loop)
{
	clip->formatCtx = NULL;
	if(avformat_open_input(&clip->formatCtx, filename, NULL, NULL) != 0)
	{
		printf("Could not find file: %s\nExiting.\n", filename);
		exit(-1);
	}

	clip->filename = av_strdup(clip->formatCtx->filename);

	avformat_find_stream_info(clip->formatCtx, NULL);

	av_dump_format(clip->formatCtx, 0, filename, 0); // DEBUG

	clip->streamIndex = -1;

	for(int i = 0; i < clip->formatCtx->nb_streams; ++i)
	{
		if(clip->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			clip->streamIndex = i;
			clip->stream = clip->formatCtx->streams[clip->streamIndex];
			break;
		}
	}
	assert(clip->streamIndex != -1);

	AVCodecContext *codecCtxOrig = clip->formatCtx->streams[clip->streamIndex]->codec;
	clip->codec = avcodec_find_decoder(codecCtxOrig->codec_id);

	clip->codecCtx = avcodec_alloc_context3(clip->codec);
	avcodec_copy_context(clip->codecCtx, codecCtxOrig);

	avcodec_open2(clip->codecCtx, clip->codec, NULL);

	clip->frame = NULL; // Yes, this is neccessary... av_frame_alloc() will fail without it.
	clip->frame = av_frame_alloc();

	clip->stream = clip->formatCtx->streams[clip->streamIndex];

	clip->loop = loop;

	copyAudioSpec(&clip->spec, Global_AudioSpec);

	clip->bitrate = clip->stream->codec->bit_rate;
	clip->samplerate = clip->stream->codec->sample_rate;
	clip->channels = clip->stream->codec->channels;
	clip->layout = clip->stream->codec->channel_layout;
	clip->sampleFmt = clip->stream->codec->sample_fmt;

	clip->swrCtx = swr_alloc();
	clip->swrCtx = swr_alloc_set_opts(clip->swrCtx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 
	                                  Global_AudioSpec.freq, clip->channels, clip->sampleFmt, 
	                                  clip->samplerate, 0, NULL);
	swr_init(clip->swrCtx);

	avcodec_close(codecCtxOrig);
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
					//decodeVideoFrameNext(clip);
					//updateVideoClipTexture(clip);
				} break;
				case SDLK_LEFT:
				{
					if(!Global_Paused) Global_Paused = true;
					//decodeVideoFramePrev(clip);
					//updateVideoClipTexture(clip);
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

	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
	TTF_Init();
	initAudioDevice();

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
	else fname = nggyu; // DEBUG FILENAME

	// TODO: List of clips to pass around
	initVideoClip(&Global_VideoClip, Global_Renderer, fname, true);
	printVideoClipInfo(Global_VideoClip);
	//decodeAllFramesToBuffer(&Global_VideoClip);
	storeAllFramesInBuffer(&Global_VideoClip);
	updateVideoClipTexture(&Global_VideoClip, Global_VideoClip.frameBuffer[0]);

	resizeAllWindowElements(Global_Window, &Global_Views, Global_VideoClip, Global_Layout);

	initAudioClip(&Global_AudioClip, fname, true);
	printAudioClipInfo(Global_AudioClip);
	SDL_PauseAudioDevice(Global_AudioDeviceID, false);

	Global_Paused = false; // DEBUG
	SDL_SetRenderDrawBlendMode(Global_Renderer, SDL_BLENDMODE_BLEND);

#if 0
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
#endif

	int startTicks = SDL_GetTicks();
	float ticksElapsed = 0.0f;

	int index = 0;

	while(Global_Running)
	{
		HandleEvents(event, &Global_VideoClip);

		SDL_GetWindowSize(Global_Window, &windowWidth, &windowHeight);

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

		// SDL_RenderCopy(Global_Renderer, textTexture, NULL, &textRect);

		if(!Global_Paused)
		{
			int endTicks = SDL_GetTicks();
			ticksElapsed += (float)endTicks - (float)startTicks;
			if(ticksElapsed >= (Global_VideoClip.msperframe - (Global_VideoClip.msperframe * 0.05f)))
			{
				// playVideoClip(Global_VideoClip);
				if(index < Global_VideoClip.frameBufferIndex)
				{
					playFrameAtIndex(&Global_VideoClip, index);
					index++;
				}
				ticksElapsed = 0;
			}
		}
		else
		{
			SDL_PauseAudioDevice(Global_AudioDeviceID, true);
		}
		startTicks = SDL_GetTicks();
		if(Global_Layout == LAYOUT_DUAL)
		{
			SDL_RenderSetClipRect(Global_Renderer, &Global_Views.currentView);
			SDL_RenderCopy(Global_Renderer, Global_VideoClip.texture, &Global_VideoClip.srcRect, 
			               &Global_Views.currentView);
		}
		// Always copy the video render textures to the composite view because it is the 
		// main video view.
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

	//TTF_CloseFont(fontAnaheimRegular);
	TTF_Quit();
	assert(Global_AudioDeviceID != 0);
	SDL_CloseAudioDevice(Global_AudioDeviceID);
	SDL_CloseAudio();
	SDL_Quit();

	freeVideoClipFull(&Global_VideoClip);
	
	printf("\nGoodbye.\n");

	return 0;
}

