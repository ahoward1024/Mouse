#ifndef UI_H
#define UI_H

#include "video.h"

struct ViewRects
{	
	// TODO: Make these a user defined setting
	const int width = 1920;
	const int height = 1080;
	const int arw = 16;
	const int arh = 9;
	// END
	SDL_Rect background;
	SDL_Rect timeline;
};

inline float fmax(float a, float b) { return a > b ? a : b ; }
inline float fmin(float a, float b) { return a < b ? a : b ; }

void layoutWindowElements(SDL_Window *window, ViewRects *views, VideoClip *clip)
{
	// printf("RESIZING ELEMENTS\n"); // DEBUG
	const int bufferSpace = 10;

	int windowWidth, windowHeight, videoW, videoH;

	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	// printf("Window: %d %d\n", windowWidth, windowHeight); // DEBUG

	// BACKGROUND
	if(views->width >= windowWidth || views->height >= windowHeight)
	{
		float scale = fmin((float)(windowWidth - 20) / (float)views->width,
		                   (float)(windowHeight - 20) / (float)views->height);
		views->background.w = views->width * scale;
		views->background.h = views->height * scale;

		views->background.x = (windowWidth / 2) - (views->background.w / 2);
		views->background.y = (windowHeight / 2) - (views->background.h / 2);
	}
	else
	{
		views->background.w = views->width;
		views->background.h = views->height;

		views->background.x = (windowWidth / 2) - (views->background.w / 2);
		views->background.y = (windowHeight / 2) - (views->background.h / 2);
	}

	// printRectangle(views->background, "Background"); // DEBUG

	// CLIP
	{
		float scale = fmin((float)views->background.w / (float)clip->width,
		                   (float)views->background.h / (float)clip->height);
		clip->videoRect.w = clip->width * scale;
		clip->videoRect.h = clip->height * scale;

		clip->videoRect.x = views->background.x + ((views->background.w - clip->videoRect.w) / 2);
		clip->videoRect.y = views->background.y + ((views->background.h - clip->videoRect.h) / 2);
	}

	// printRectangle(clip->videoRect, "Clip"); // DEBUG

	// TIMELINE
	// Put the timeline clip view in the bottom part of the screen under the video
	//views->timeline.x = 10;
	//views->timeline.y = views->background.h + 20;
	//views->timeline.w = (windowWidth) - (10 * 2);
	//views->timeline.h = (windowHeight - views->timeline.y) - 10;
}

void setVideoClipPosition(SDL_Window *window, VideoClip *clip, int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if(x < 0 || y < 0 || x > width || y > height) return;
	clip->videoRect.x = x;
	clip->videoRect.y = y;
}

void setRenderColorAlpha(SDL_Renderer *r, tColor c, uint8 alpha)
{
	SDL_SetRenderDrawColor(r, c.r, c.b, c.g, alpha);
}

void setRenderColor(SDL_Renderer *r, tColor c)
{
	SDL_SetRenderDrawColor(r, c.r, c.b, c.g, c.a);
}

void drawClipBoundRect(SDL_Renderer *renderer, VideoClip clip, tColor c, int thickness)
{
	SDL_Rect r = clip.videoRect;

	setRenderColor(renderer, c);

	for(int i = 0; i < thickness; i++)
	{
		r.x -= 1;
		r.y -= 1;
		r.w += 2;
		r.h += 2;

		SDL_RenderDrawRect(renderer, &r);
	}
}

#endif