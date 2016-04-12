#ifndef UI_H
#define UI_H

#include "video.h"

struct ViewRects
{
	SDL_Rect currentViewBack; 
	SDL_Rect compositeViewBack;
	SDL_Rect currentView;
	SDL_Rect compositeView;
	SDL_Rect timelineView;
	SDL_Rect browserView;
	SDL_Rect effectsView;
};

struct Border
{
	SDL_Rect top;
	SDL_Rect bot;
	SDL_Rect left;
	SDL_Rect right;
};

global Border currentBorder, compositeBorder,
timelineBorder, browserBorder, effectsBorder;

enum BorderSide
{
	BORDER_SIDE_INSIDE,
	BORDER_SIDE_OUTSIDE,
};

enum WindowLayout
{
	LAYOUT_SINGLE,
	LAYOUT_DUAL,
};

internal bool isInsideRect(SDL_Rect rect, int mx, int my)
{
	int x1, x2, y1, y2;
	int A, B, C, D;
	x1 = rect.x;
	y1 = rect.y;
	x2 = x1;
	y2 = y1 + rect.h;

	A = -(y2 - y1);
	B = x2 - x1;
	C = -(A * x1 + B * y1);

	D = A * mx + B * my + C;

	if(D == 0) return true;

	return false;
}

internal void copyRect(SDL_Rect *dest, SDL_Rect src)
{
	dest->x = src.x;
	dest->y = src.y;
	dest->w = src.w;
	dest->h = src.h;
}

void setRenderColor(SDL_Renderer *renderer, tColor color, uint8 alpha)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
}

void setRenderColor(SDL_Renderer *renderer, tColor color)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

// TODO: Clean drawClipTransformControls() up
void drawClipTransformControls(SDL_Renderer *renderer, VideoClip *clip)
{

	SDL_Rect r = clip->destRect;

	SDL_Rect i;
	SDL_Rect b;

	i.x = r.x - 2;
	i.y = r.y - 2;
	i.w = 4;
	i.h = 4;

	b.x = i.x - 1;
	b.y = i.y - 1;
	b.w = 7;
	b.h = 7;

	// TOP ROW
	{
		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + (r.h / 2);
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + r.h - i.h;
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);	
	}
	i.x = r.x + (r.w / 2) - i.w;
	b.x = i.x - 1;

	// MIDDLE ROW
	{
		i.y = r.y;
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + (r.h / 2) - i.h;
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + r.h - i.h;
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);
	}

	i.x = r.x + r.w - i.w;
	b.x = i.x - 1;

	// BOTTOM ROW
	{
		i.y = r.y - 2;
		b.y = i.y - 1;
		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + (r.h / 2);
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);

		i.y = r.y + r.h - i.h;
		b.y = i.y - 1;

		setRenderColor(renderer, tcBlack);
		SDL_RenderFillRect(renderer, &b);
		setRenderColor(renderer, tcWhite);
		SDL_RenderFillRect(renderer, &i);
	}
}

internal void drawBorder(SDL_Renderer *renderer, Border b)
{
	SDL_RenderFillRect(renderer, &b.top);
	SDL_RenderFillRect(renderer, &b.bot);
	SDL_RenderFillRect(renderer, &b.left);
	SDL_RenderFillRect(renderer, &b.right);
}

// This creates a border who's top and bottom are designated on the specified border side
internal void buildBorder(Border *b, SDL_Rect *r, BorderSide bs)
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
internal void dualLayout(SDL_Window *window, ViewRects *views, VideoClip clip)
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
	views->currentViewBack.w = backViewW;
	views->currentViewBack.h = backViewH;	
	views->currentViewBack.x = bufferSpace + 
	(((hwidth - (bufferSpace * 2)) - views->currentViewBack.w) / 2);
	views->currentViewBack.y = bufferSpace + 
	(((hheight - (bufferSpace * 2)) - views->currentViewBack.h) / 2);

	// Fit the current view background into an output aspect ratio rectangle on the right side
	views->compositeViewBack.w = backViewW;
	views->compositeViewBack.h = backViewH;
	views->compositeViewBack.x = (windowWidth / 2) + bufferSpace + 
	(((hwidth - (bufferSpace * 2)) - views->compositeViewBack.w) / 2);
	views->compositeViewBack.y = bufferSpace + (((hheight - (bufferSpace * 2)) - 
	                                             views->compositeViewBack.h) / 2);

  // (??? STC ???) <
	// If the clip is bigger than the back views then we just resize it to be the back view
	// and "fit it to frame". Otherwise we keep the Global_VideoClip at it's original size.
	// NOTE: We may want to test this, perhaps clip that are bigger than the size of the wanted
	// composite should be clipped outside of the composite... 
	if(clip.width > backViewW || clip.height > backViewH)
	{
		// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
		for(int w = 0, h = 0; 
		    w <= views->currentViewBack.w && h <= views->currentViewBack.h; 
		    w += clip.aspectRatioW, h += clip.aspectRatioH)
		{
			currentViewW = w;
			currentViewH = h;
		}

		// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
		for(int w = 0, h = 0; 
		    w <= views->compositeViewBack.w && h <= views->compositeViewBack.h; 
		    w += clip.aspectRatioW, h += clip.aspectRatioH)
		{
			compositeViewW = w;
			compositeViewH = h;
		}
	}
	else
	{
		currentViewW = clip.codecCtx->width;
		currentViewH = clip.codecCtx->height;

		compositeViewW = clip.codecCtx->width;
		compositeViewH = clip.codecCtx->height;
	}
	// (??? STC ???)) >

	// Put the current clip view on the right hand side and center it in the current view background
	views->currentView.w = currentViewW;
	views->currentView.h = currentViewH;
	views->currentView.x = views->currentViewBack.x + 
		((views->currentViewBack.w - views->currentView.w) / 2);
	views->currentView.y = views->currentViewBack.y + 
		((views->currentViewBack.h - views->currentView.h) / 2);
	copyRect(&clip.destRect, views->currentView);
	//buildBorder(&currentBorder, &currentViewBack, BORDER_SIDE_OUTSIDE);

	// Put the composite clip view on the left hand side and center it in the composite 
	// view background
	views->compositeView.w = compositeViewW;
	views->compositeView.h = compositeViewH;
	views->compositeView.x = views->compositeViewBack.x + 
		((views->compositeViewBack.w - views->compositeView.w) / 2);
	views->compositeView.y = views->compositeViewBack.y + 
		((views->compositeViewBack.h - views->compositeView.h) / 2);
	//buildBorder(&compositeBorder, &compositeViewBack, BORDER_SIDE_OUTSIDE);

	// Put the file browser view in the bottom right portion of the screen
	views->browserView.x = bufferSpace;
	views->browserView.y = (windowHeight / 2) + bufferSpace;
	views->browserView.w = (windowWidth / 6) - (bufferSpace * 2);
	views->browserView.h = (windowHeight / 2) - (bufferSpace * 2);
	//buildBorder(&browserBorder, &browserView, BORDER_SIDE_INSIDE);

	// Put the timeline clip view in the center bottom portion of the screen between the file 
	// browser view and the track effects view
	views->timelineView.x = (windowWidth / 6) + bufferSpace;
	views->timelineView.y = (windowHeight / 2) + bufferSpace;
	views->timelineView.w = ((windowWidth / 6) * 4) - (bufferSpace * 2);
	views->timelineView.h = (windowHeight / 2) - (bufferSpace * 2);
	//buildBorder(&timelineBorder, &timelineView, BORDER_SIDE_INSIDE);

	// Put the track effects view in the bottom portion of the screen to the left of the 
	// timeline view
	views->effectsView.x = ((windowWidth / 6) * 5) + bufferSpace;
	views->effectsView.y = (windowHeight / 2) + bufferSpace;
	views->effectsView.w = (windowWidth / 6) - (bufferSpace * 2);
	views->effectsView.h = (windowHeight / 2) - (bufferSpace * 2);
	//buildBorder(&effectsBorder, &effectsView, BORDER_SIDE_INSIDE);
}

internal void singleLayout(SDL_Window *window, ViewRects *views, VideoClip clip)
{
	const int bufferSpace = 10;

	int windowWidth, windowHeight, hwidth, hheight, backViewW, backViewH, 
	compositeViewW, compositeViewH;

	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	hwidth = windowWidth / 2;
	hheight = windowHeight / 2;

	// Put the timeline clip view in the bottom half of the screen
	views->timelineView.x = bufferSpace;
	views->timelineView.y = (windowHeight / 2) + bufferSpace;
	views->timelineView.w = (windowWidth) - (bufferSpace * 2);
	views->timelineView.h = (windowHeight / 2) - (bufferSpace * 2);
	//buildBorder(&timelineBorder, &timelineView, BORDER_SIDE_INSIDE);

	// Find the nearest 16x9 (STC) resolution to fit the composite views into
	// STC: Need to make the aspect ratio the aspect ratio of the output composition.
	for(int w = 0, h = 0; 
	    w < (windowWidth / 2) && h < (hheight - (bufferSpace * 2));
	    w += 16, h += 9)
	{
		backViewW = w;
		backViewH = h;
	}

	// Fit the current view background into an output aspect ratio in the top middle
	views->compositeViewBack.w = backViewW;
	views->compositeViewBack.h = backViewH;
	views->compositeViewBack.x = (windowWidth / 2) - (views->compositeViewBack.w / 2);
	views->compositeViewBack.y = (windowHeight / 4) - (views->compositeViewBack.h / 2);

	// (??? STC ???) <
  // If the clip is bigger than the back views then we just resize it to be the back view
  // and "fit it to frame". Otherwise we keep the Global_VideoClip at it's original size.
  // NOTE: We may want to test this, perhaps clips that are bigger than the size of the wanted
  // composite should be clipped outside of the composite... 
	if(clip.width > backViewW || clip.height > backViewH)
	{
				// Find the nearest rectangle to fit the clip's aspect ratio into the view backgrounds
		for(int w = 0, h = 0; 
		    w <= views->compositeViewBack.w && h <= views->compositeViewBack.h; 
		    w += clip.aspectRatioW, h += clip.aspectRatioH)
		{
			compositeViewW = w;
			compositeViewH = h;
		}
	}
	else
	{
		compositeViewW = clip.codecCtx->width;
		compositeViewH = clip.codecCtx->height;
	}
	// (??? STC ???)) >

	// Put the composite clip view on the left hand side and center it in the composite 
	// view background
	views->compositeView.w = compositeViewW;
	views->compositeView.h = compositeViewH;
	views->compositeView.x = views->compositeViewBack.x + 
		((views->compositeViewBack.w - views->compositeView.w) / 2);
	views->compositeView.y = views->compositeViewBack.y + 
		((views->compositeViewBack.h - views->compositeView.h) / 2);
	copyRect(&clip.destRect, views->compositeView);
  //buildBorder(&compositeBorder, &compositeViewBack, BORDER_SIDE_OUTSIDE);

	// Put the file browser view in the top right portion (1/6th) of the screen
	views->browserView.x = bufferSpace;
	views->browserView.y = bufferSpace;
	views->browserView.w = views->compositeViewBack.x - (bufferSpace * 2);
	views->browserView.h = (windowHeight / 2) - (bufferSpace * 2);
  //buildBorder(&browserBorder, &browserView, BORDER_SIDE_INSIDE);

	// Put the track effects view in the top left portion (1/6th) of the screen
	views->effectsView.x = views->compositeViewBack.x + views->compositeViewBack.w + bufferSpace;
	views->effectsView.y = bufferSpace;
	views->effectsView.w = windowWidth - views->effectsView.x - bufferSpace;
	views->effectsView.h = (windowHeight / 2) - (bufferSpace * 2);
	//buildBorder(&effectsBorder, &effectsView, BORDER_SIDE_INSIDE);
}

void resizeAllWindowElements(SDL_Window *window, ViewRects *views, VideoClip clip, 
                             WindowLayout layout)
{
	if(layout == LAYOUT_SINGLE) singleLayout(window, views, clip);
	else if(layout == LAYOUT_DUAL) dualLayout(window, views, clip);
}

#endif