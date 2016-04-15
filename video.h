#ifndef VIDEO_H
#define VIDEO_H

#define MAX_FRAME_BUFFER_SIZE 65536

// Video only clips
// NOTE:IMPORTANT: Video clips MUST be initalized to zero.
struct VideoClip
{
	AVFormatContext    *formatCtx;
	AVCodecContext     *codecCtx;
	AVCodec            *codec;
	AVStream           *stream;
	AVPacket            packet;
	struct SwsContext  *swsCtx;
	int                 streamIndex;
	SDL_Texture        *texture;    // MV VClip
	SDL_Rect            srcRect;    // NIX
	SDL_Rect            destRect;   // NIX
	int32               yPlaneSz;   // NIX
	int32               uvPlaneSz;  // NIX
	uint8              *yPlane;     // MV VClip
	uint8              *uPlane;     // MV VClip
	uint8              *vPlane;     // MV VClip
	int32               uvPitch;    // MV VClip
	const char         *filename;
	float               framerate;
	float               avgFramerate;
	float               msperframe;
	int                 bitrate;
	int                 aspectRatioW;
	int                 aspectRatioH;
	float               aspectRatioF;
	int                 currentFrame; // NIX ?? MV VClip
	int                 frameCount;
	int                 width;
	int                 height;
	bool								loop; // NIX
	AVFrame            *frameBuffer[MAX_FRAME_BUFFER_SIZE];
	int                 frameBufferIndex;
};

struct VClip
{
	SDL_Rect      tlRect;
	SDL_Rect      viewRect;
	SDL_Texture  *texture;
	VideoClip    *video;
	uint8        *yPlane;  
	uint8        *uPlane;  
	uint8        *vPlane;  
	int32         uvPitch; 
	int           beginFrame;
	int           endFrame;
};

// This will free the clip for reinitalization, we do not free the texture
// as SDL still needs it for video resizing.
void freeVideoClip(VideoClip *clip)
{
	printf("Freeing clip: %s\n\n", clip->filename); // DEBUG
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
	printf("Freeing clip fully: %s\n\n", clip->filename);
	freeVideoClip(clip);
	SDL_free(clip->texture);
}

// Print the video clip animation
void printVideoClipInfo(VideoClip clip)
{
	printf("INIT VIDEO CLIP ================\n");
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
	printf("Video bitrate: ");
	if (clip.formatCtx->bit_rate) printf("%d kb/s\n", (int64_t)clip.formatCtx->bit_rate / 1000);
	else printf("N/A\n");
	printf("Width/Height: %dx%d\n", clip.width, clip.height);
	printf("Real based framerate: %.2ffps\n", clip.framerate);
	printf("Milliseconds per frame: %.4f\n", clip.msperframe);
	printf("Bitrate: %d\n", clip.bitrate);
	printf("Average framerate: %.2fps\n", clip.avgFramerate);
	printf("Aspect Ratio: (%f), [%d:%d]\n", clip.aspectRatioF, 
	       clip.aspectRatioW, clip.aspectRatioH);
	printf("Number of frames: ");
	if(clip.frameCount) printf("%d\n", clip.frameCount);
	else printf("unknown\n");
	printf("Looped: %s\n", clip.loop ? "true" : "false");
	printf("\n");
}

void storeAllFramesInBuffer(VideoClip *clip)
{
	AVPacket packet;
	av_init_packet(&packet);

	int i = 0;

	clip->frameBufferIndex = 0;

	AVFrame *frame = av_frame_alloc();

	int start = SDL_GetTicks();

	while(av_read_frame(clip->formatCtx, &packet) >= 0)
	{
		int frameFinished = 0;
		if(packet.stream_index == clip->streamIndex)
		{
			avcodec_decode_video2(clip->codecCtx, frame, &frameFinished, &packet);
			if(frameFinished)
			{
				clip->frameBuffer[clip->frameBufferIndex] = av_frame_clone(frame);
				clip->frameBufferIndex++;
			}
		}
	}

	int end = SDL_GetTicks();

	printf("Finished decoding and storing all frames in: %dms\n\n", end - start);
}

void updateVideoClipTexture(VideoClip *clip, AVFrame *frame)
{
	AVPicture pict;
	pict.data[0] = clip->yPlane;
	pict.data[1] = clip->uPlane;
	pict.data[2] = clip->vPlane;
	pict.linesize[0] = clip->codecCtx->width;
	pict.linesize[1] = clip->uvPitch;
	pict.linesize[2] = clip->uvPitch;

	sws_scale(clip->swsCtx, (uint8 const * const *)frame->data, 
	          frame->linesize, 
	          0, clip->codecCtx->height, pict.data, pict.linesize);

	SDL_UpdateYUVTexture(clip->texture, NULL, clip->yPlane, 
	                     clip->codecCtx->width, clip->uPlane,
	                     clip->uvPitch, clip->vPlane, clip->uvPitch);
}

void playFrameAtIndex(VideoClip *clip, int index)
{
	updateVideoClipTexture(clip, clip->frameBuffer[index]);
}

void initVideoClip(VideoClip *clip, SDL_Renderer *renderer, const char *filename, bool loop)
{
	clip->formatCtx = NULL;
	if(avformat_open_input(&clip->formatCtx, filename, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", filename);
		exit(-1);
	}

	clip->filename = av_strdup(clip->formatCtx->filename);

	avformat_find_stream_info(clip->formatCtx, NULL);

	// av_dump_format(clip->formatCtx, 0, filename, 0); // DEBUG

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

	AVCodecContext *codecCtxOrig = clip->formatCtx->streams[clip->streamIndex]->codec;
	clip->codec = avcodec_find_decoder(codecCtxOrig->codec_id);
	
	clip->codecCtx	= avcodec_alloc_context3(clip->codec);
	avcodec_copy_context(clip->codecCtx, codecCtxOrig);

	clip->width = clip->codecCtx->width;
	clip->height = clip->codecCtx->height;

	avcodec_close(codecCtxOrig);

	avcodec_open2(clip->codecCtx, clip->codec, NULL);

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
	clip->bitrate = clip->stream->codec->bit_rate;
	
	// Use av_reduce() to reduce a fraction to get the width and height of the clip's aspect ratio
	av_reduce(&clip->aspectRatioW, &clip->aspectRatioH,
	          clip->codecCtx->width,
	          clip->codecCtx->height,
	          128);
	clip->aspectRatioF = (float)clip->aspectRatioW / (float)clip->aspectRatioH;

	clip->loop = loop;

	clip->currentFrame = -1;

	clip->frameCount = clip->stream->nb_frames;

	clip->frameBufferIndex = 0;

	for(int i = 0; i < MAX_FRAME_BUFFER_SIZE; ++i)
	{
		clip->frameBuffer[i] = av_frame_alloc();
	}

	storeAllFramesInBuffer(clip);
	updateVideoClipTexture(clip, clip->frameBuffer[0]);

	avcodec_close(codecCtxOrig);
}

#endif