#ifndef VIDEO_H
#define VIDEO_H

// Video only clips
// NOTE:IMPORTANT: Video clips MUST be initalized to zero.
struct VideoClip
{
	AVFormatContext    *formatCtx;
	AVCodecContext     *codecCtx;
	AVCodec            *codec;
	AVStream           *stream;
	AVFrame            *frame;
	AVPacket            packet;
	struct SwsContext  *swsCtx;
	int                 streamIndex;
	SDL_Texture        *texture;
	SDL_Rect            srcRect;
	SDL_Rect            destRect;
	int32               yPlaneSz;
	int32               uvPlaneSz; 
	uint8              *yPlane;
	uint8              *uPlane; 
	uint8              *vPlane;
	int32               uvPitch;
	const char         *filename;
	float               framerate;
	float               avgFramerate;
	float               msperframe;
	int                 iframerate;
	int                 aspectRatioW;
	int                 aspectRatioH;
	float               aspectRatioF;
	int                 currentFrame;
	int                 frameCount;
	int                 width;
	int                 height;
	bool								loop;
};

// This will free the clip for reinitalization, we do not free the texture
// as SDL still needs it for video resizing.
void freeVideoClip(VideoClip *clip)
{
	av_frame_free(&clip->frame);
	avcodec_close(clip->codecCtx);
	avformat_close_input(&clip->formatCtx);
	free(clip->formatCtx);
	free(clip->yPlane);
	free(clip->uPlane);
	free(clip->vPlane);
}

// This will free the clip entirely so it CANNOT be used again.
void freeVideoClipFull(VideoClip *clip)
{
	freeVideoClip(clip);
	SDL_free(clip->texture);
}

void updateVideoClipTexture(VideoClip *clip)
{
	AVPicture pict;
	pict.data[0] = clip->yPlane;
	pict.data[1] = clip->uPlane;
	pict.data[2] = clip->vPlane;
	pict.linesize[0] = clip->codecCtx->width;
	pict.linesize[1] = clip->uvPitch;
	pict.linesize[2] = clip->uvPitch;

	sws_scale(clip->swsCtx, (uint8 const * const *)clip->frame->data, 
	          clip->frame->linesize, 
	          0, clip->codecCtx->height, pict.data, pict.linesize);

	SDL_UpdateYUVTexture(clip->texture, NULL, clip->yPlane, 
	                     clip->codecCtx->width, clip->uPlane,
	                     clip->uvPitch, clip->vPlane, clip->uvPitch);

	clip->currentFrame = clip->frame->coded_picture_number;
}

void setVideoClipToBeginning(VideoClip *clip)
{
	av_seek_frame(clip->formatCtx, clip->streamIndex, 0, AVSEEK_FLAG_FRAME);
	// printf("Looped clip: %s\n", clip->filename); // DEBUG
}

// Return 0 for sucessful frame, 1 for end of file or error
int decodeNextVideoFrame(VideoClip *clip)
{
	int frameFinished;
	bool readFullFrame = false;
	do
	{
		if(av_read_frame(clip->formatCtx, &clip->packet) >= 0)
		{
			if(clip->packet.stream_index == clip->streamIndex)
			{
				do
				{
					avcodec_decode_video2(clip->codecCtx, clip->frame, &frameFinished, &clip->packet);
				} while(!frameFinished);
				readFullFrame = true;
			}
		}
		else
		{
			if(clip->loop) setVideoClipToBeginning(clip);
		}
	} while(!readFullFrame);

	return 0;
}

// Print the video clip animation
void printVideoClipInfo(VideoClip clip)
{
	printf("FILENAME: %s\n", clip.filename);
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
	printf("Video bitrate: ");
	if (clip.formatCtx->bit_rate) printf("%d kb/s\n", (int64_t)clip.formatCtx->bit_rate / 1000);
	else printf("N/A\n");
	printf("Width/Height: %dx%d\n", clip.width, clip.height);
	printf("Real based framerate: %.2ffps\n", clip.framerate);
	printf("Real based framerate int: %d\n", clip.iframerate);
	printf("Milliseconds per frame: %.4f\n", clip.msperframe);
	printf("Average framerate: %.2fps\n", clip.avgFramerate);
	printf("Aspect Ratio: (%f), [%d:%d]\n", clip.aspectRatioF, 
	       clip.aspectRatioW, clip.aspectRatioH);
	printf("Number of frames: ");
	if(clip.frameCount) printf("%d\n", clip.frameCount);
	else printf("unknown\n");
	printf("\n");
}

void initVideoClip(VideoClip *clip, SDL_Renderer *renderer, const char *file, bool loop)
{
	clip->formatCtx = NULL;
	if(avformat_open_input(&clip->formatCtx, file, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", file);
		exit(-1);
	}

	clip->filename = av_strdup(clip->formatCtx->filename);

	avformat_find_stream_info(clip->formatCtx, NULL);

	// av_dump_format(clip->formatCtx, 0, file, 0); // DEBUG

	clip->streamIndex = -1;

	for(int i = 0; i < clip->formatCtx->nb_streams; ++i)
	{
		if(clip->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			clip->streamIndex = i;
			clip->stream = clip->formatCtx->streams[clip->streamIndex];
			break;
		}
	}

	assert(clip->streamIndex != -1);
	AVCodecContext *codecCtxOrig = 
	clip->formatCtx->streams[clip->streamIndex]->codec;
	clip->codec = avcodec_find_decoder(codecCtxOrig->codec_id);
	
	clip->codecCtx	= avcodec_alloc_context3(clip->codec);
	avcodec_copy_context(clip->codecCtx, codecCtxOrig);

	clip->width = clip->codecCtx->width;
	clip->height = clip->codecCtx->height;

	avcodec_close(codecCtxOrig);

	avcodec_open2(clip->codecCtx, clip->codec, NULL);

	clip->frame = NULL;
	clip->frame = av_frame_alloc();

	clip->swsCtx = sws_getContext(clip->codecCtx->width,
	                              clip->codecCtx->height,
	                              clip->codecCtx->pix_fmt,
	                              clip->codecCtx->width,
	                              clip->codecCtx->height,
	                              AV_PIX_FMT_YUV420P,
	                              SWS_BILINEAR,
	                              NULL,
	                              NULL,
	                              NULL);

	clip->yPlaneSz = clip->codecCtx->width * clip->codecCtx->height;
	clip->uvPlaneSz = clip->codecCtx->width * clip->codecCtx->height / 4;
	clip->yPlane = (uint8 *)malloc(clip->yPlaneSz);
	clip->uPlane = (uint8 *)malloc(clip->uvPlaneSz);
	clip->vPlane = (uint8 *)malloc(clip->uvPlaneSz);

	clip->uvPitch = clip->codecCtx->width / 2;

	clip->srcRect;
	clip->srcRect.x = 0;
	clip->srcRect.y = 0;
	clip->srcRect.w = clip->codecCtx->width;
	clip->srcRect.h = clip->codecCtx->height;

#if 0
	int windowWidth, windowHeight;
	SDL_GetWindowSize(Global_Window, &windowWidth, &windowHeight);
	SDL_Rect videoDestRect;
	clip->destRect.x = 0;
	clip->destRect.y = 0;
	clip->destRect.w = clip->srcRect.w;
	clip->destRect.h = clip->srcRect.h;
#endif

	if(!clip->texture) 
	{
		clip->texture = SDL_CreateTexture(renderer,
		                                  SDL_PIXELFORMAT_YV12,
		                                  SDL_TEXTUREACCESS_STREAMING,
		                                  clip->codecCtx->width,
		                                  clip->codecCtx->height);
	}

	// Use the video stream to get the real base framerate and average framerates
	clip->stream = clip->formatCtx->streams[clip->streamIndex];
	AVRational fr = clip->stream->r_frame_rate;
	clip->framerate = (float)fr.num / (float)fr.den;
	AVRational afr = clip->stream->avg_frame_rate;
	clip->avgFramerate = (float)afr.num / (float)afr.den;
	clip->msperframe = (1/clip->framerate) * 1000;
	clip->iframerate = clip->framerate * 1000;
	
	// Use av_reduce() to reduce a fraction to get the width and height of the clip's aspect ratio
	av_reduce(&clip->aspectRatioW, &clip->aspectRatioH,
	          clip->codecCtx->width,
	          clip->codecCtx->height,
	          128);
	clip->aspectRatioF = (float)clip->aspectRatioW / (float)clip->aspectRatioH;

	clip->loop = loop;

	clip->currentFrame = -1;

	clip->frameCount = clip->stream->nb_frames;

	avcodec_close(codecCtxOrig);
	// Decode the first video frame
	decodeNextVideoFrame(clip);
	updateVideoClipTexture(clip);

	av_free_packet(&clip->packet);
}

void setVideoClipPosition(SDL_Window *window, VideoClip *clip, int x, int y)
{
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	if(x < 0 || y < 0 || x > width || y > height) return;
	clip->destRect.x = x;
	clip->destRect.y = y;
}

#endif