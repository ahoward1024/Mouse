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
	SDL_Rect scrubber = { 0, 0, 2, 14 };
};

struct Point
{
	int x;
	int y;
};

inline float fmax(float a, float b) { return a > b ? a : b ; }
inline float fmin(float a, float b) { return a < b ? a : b ; }

void DrawText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *text, 
              SDL_Color color)
{
	SDL_Surface *surface;
	if(surface = (TTF_RenderText_Blended(font, text, color)))
	{
		SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
		if(texture)
		{
			SDL_Rect rect;
			rect.x = x;
			rect.y = y;
			rect.w = surface->w;
			rect.h = surface->h;

			SDL_RenderCopy(renderer, texture, NULL, &rect);
			SDL_DestroyTexture(texture);
			SDL_FreeSurface(surface);
		}
	}
}

void setScrubberXPosition(VideoClip clip, ViewRects *views, int currentTime)
{
	if(currentTime > 0)
	{
		float percent = ((float)clip.endFrame / (float)currentTime);
		views->scrubber.x = clip.tlRect.x + ((float)clip.tlRect.w / percent);
	}
	else
	{
		views->scrubber.x = clip.tlRect.x;
	}
}

void layoutWindowElements(SDL_Window *window, ViewRects *views, VideoClip *clip, int currentTime)
{
	// printf("RESIZING ELEMENTS\n"); // DEBUG
	const int padding = 200;

	int windowWidth, windowHeight, videoW, videoH;

	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	// printf("Window: %d %d\n", windowWidth, windowHeight); // DEBUG

	// BACKGROUND
	if(views->width >= windowWidth || views->height >= windowHeight)
	{
		float scale = fmin((float)(windowWidth - padding) / (float)views->width,
		                   (float)(windowHeight - padding) / (float)views->height);
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
		if(clip->width > views->background.w || clip->height > views->background.h)
		{
			float scale = fmin((float)views->background.w / (float)clip->width,
			                   (float)views->background.h / (float)clip->height);
			clip->videoRect.w = clip->width * scale;
			clip->videoRect.h = clip->height * scale;
		}
		else
		{
			clip->videoRect.w = clip->width;
			clip->videoRect.h = clip->height;
		}

		clip->videoRect.x = views->background.x + ((views->background.w - clip->videoRect.w) / 2);
		clip->videoRect.y = views->background.y + ((views->background.h - clip->videoRect.h) / 2);
	}

	// printRectangle(clip->videoRect, "Clip"); // DEBUG

	// CLIP TL RECT
	{
		clip->tlRect.x = views->background.x;
		clip->tlRect.y = views->background.y + views->background.h + (clip->tlRect.h * 2);
		clip->tlRect.w = views->background.w;
		clip->tlRect.h = 20; // TODO(alex): This is hardcoded for now, change this dynamically.
	}

	// SCRUBBER
	{
		setScrubberXPosition(*clip, views, currentTime);
		views->scrubber.h = clip->tlRect.h + 4;
		views->scrubber.y = clip->tlRect.y - 2;
	}

	printRectangle(clip->tlRect, "Clip TL rect");
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