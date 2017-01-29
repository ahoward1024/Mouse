#ifndef VIDEO_H
#define VIDEO_H

// Frame Data
struct Frame
{
	int            parentKeyframe = -1;
	int64          pts = INT_MIN;
	int64          dts = INT_MIN;
};

struct VideoFile
{
	AVFormatContext *formatCtx;
	AVCodecContext  *codecCtx;
	AVCodec         *codec;
	AVStream        *stream;
	Frame           *frames;
	int             *keyframeList;
	int             *ptsList;
	int             *ptsListSorted;
	int              streamIndex    = 0;
	int              bitrate        = 0;
	int              arW            = 0;
	int              arH            = 0;
	int              width          = 0;
	int              height         = 0;
	uint32          _framesListSize = 0;
	uint32          _ptsListSize    = 0;
	uint32           nkeyframes     = 0;
	uint32					 nframes        = 0;
	uint64           timeBase       = 0;
	float            framerate      = 0.0f;
	float            avgFramerate   = 0.0f;
	float            msperframe     = 0.0f;
	float            arF            = 0.0f;
};

struct VideoClip
{
	VideoFile    *vfile;
	SDL_Rect      tlRect;
	SDL_Rect      videoRect;
	SDL_Texture  *texture;
	AVFrame      *frame;
	SwsContext   *swsCtx;
	uint8        *yPlane;  
	uint8        *uPlane;  
	uint8        *vPlane;  
	int32         uvPitch;
	int           width;
	int           height;
	int           arW;
	int           arH;
	int           beginFrame;
	int           endFrame;
	int           number;
	char         *filename;
};

int ptsCompare(const void * a, const void * b)
{
	return (*(int *)a - *(int *)b);
}

// TODO Fix memory leak error when dragging and dropping clips
// This will free the clip for reinitalization, we do not free the texture
// as SDL still needs it for video resizing.
void freeVideoFile(VideoFile *vfile)
{
	printf("Freeing video file: %s\n\n", vfile->formatCtx->filename); // DEBUG
	avcodec_close(vfile->codecCtx);
	avformat_close_input(&vfile->formatCtx);
	av_free(vfile->codec);
}

void freeVideoClip(VideoClip *clip)
{
	printf("Freeing video clip %d : %s\n", clip->number, clip->vfile->formatCtx->filename); // DEBUG
	av_free(clip->swsCtx);
	av_frame_free(&clip->frame);
	SDL_free(clip->texture);
	free(clip->yPlane);
	free(clip->uPlane);
	free(clip->vPlane);
}

void updateVideoClipTexture(VideoClip *clip)
{
	AVPicture pict;
	pict.data[0] = clip->yPlane;
	pict.data[1] = clip->uPlane;
	pict.data[2] = clip->vPlane;
	pict.linesize[0] = clip->vfile->width;
	pict.linesize[1] = clip->uvPitch;
	pict.linesize[2] = clip->uvPitch;

	sws_scale(clip->swsCtx, (uint8 const * const *)clip->frame->data, clip->frame->linesize, 
	          0, clip->vfile->height, pict.data, pict.linesize);

	SDL_UpdateYUVTexture(clip->texture, NULL, clip->yPlane, 
	                     clip->vfile->width, clip->uPlane,
	                     clip->uvPitch, clip->vPlane, clip->uvPitch);
}

inline void flushPlayEnd(VideoClip *clip, int *currentTime)
{
	// printf("Flushing with index.\n"); // DEBUG
	if(clip->vfile->codec->capabilities & AV_CODEC_CAP_DELAY)
	{
		AVPacket packet;
		av_init_packet(&packet);
		packet.data = NULL;
		packet.size = 0;
		int gotFrame = 1;
		int result = 0;
		while((result = avcodec_decode_video2(clip->vfile->codecCtx, clip->frame,
		                                      &gotFrame, &packet)) >= 0 && gotFrame);
		{
			if(gotFrame)
			{
				updateVideoClipTexture(clip);
				++currentTime;
			}
		}
		av_packet_unref(&packet);
	}
}

inline void flushClipEnd(VideoClip *clip)
{
	// printf("Flushing clip only.\n"); // DEBUG
	if(clip->vfile->codec->capabilities & AV_CODEC_CAP_DELAY)
	{
		AVPacket packet;
		av_init_packet(&packet);
		packet.data = NULL;
		packet.size = 0;
		int gotFrame = 0;
		int result = 0;
		while((result = avcodec_decode_video2(clip->vfile->codecCtx, 
		                                      clip->frame, &gotFrame, &packet)) >= 0 && gotFrame)
		{
			if(gotFrame)
			{
				updateVideoClipTexture(clip);
			}
		}
		av_packet_unref(&packet);
	}
}

inline int decodeSingleFrame(VideoClip *clip)
{
	AVPacket packet;
	av_init_packet(&packet);
	int gotFrame = 0;
	bool done = false;
	int result = 0;
	do
	{
		if(av_read_frame(clip->vfile->formatCtx, &packet) >= 0)
		{
			if(packet.stream_index == clip->vfile->streamIndex)
			{
				result = avcodec_decode_video2(clip->vfile->codecCtx, clip->frame, &gotFrame, &packet);
			}
		}
		else
		{
			gotFrame = -1;
		}
	} while(!gotFrame);
	av_packet_unref(&packet);
	return result;
}

inline int decodeSingleFrameNoCapDelay(VideoClip *clip)
{
	AVPacket packet;
	av_init_packet(&packet);
	int gotFrame = 0;
	bool done = false;
	int result = 0;
	do
	{
		if(av_read_frame(clip->vfile->formatCtx, &packet) >= 0)
		{
			if(packet.stream_index == clip->vfile->streamIndex)
			{
				result = avcodec_decode_video2(clip->vfile->codecCtx, clip->frame, &gotFrame, &packet);
				done = true;
			}
		}
	} while(!done);
	av_packet_unref(&packet);
	return result;
}

inline int decodeSingleFrameCapDelay(VideoClip *clip)
{
	AVPacket packet;
	av_init_packet(&packet);
	int gotFrame = 0;
	bool done = false;
	int result = 0;
	do
	{
		if(av_read_frame(clip->vfile->formatCtx, &packet) >= 0)
		{
			if(packet.stream_index == clip->vfile->streamIndex)
			{
				result = avcodec_decode_video2(clip->vfile->codecCtx, clip->frame, &gotFrame, &packet);
				done = true;
			}
		}
	} while(!done);
	if(clip->vfile->codec->capabilities & AV_CODEC_CAP_DELAY)
	{
		packet.data = NULL;
		packet.size = 0;
		avcodec_decode_video2(clip->vfile->codecCtx, clip->frame, &gotFrame, &packet);
	}
	av_packet_unref(&packet);
	return result;
}

// WARNING: When you call this function MAKE ABSOLUTELY SURE THE WANTED FRAME IS SANITIZED
// This function will make no attempt to make sure the value is able to be seeked to in the
// interest of speed. This is an _incredibly_ slow function in it's own right.
bool seekToAnyFrame(VideoClip *clip, int wantedFrame, int currentFrame)
{
	int flags = 0;
	if(wantedFrame < currentFrame) flags = AVSEEK_FLAG_BACKWARD;
	int pkeyf = clip->vfile->frames[wantedFrame].parentKeyframe;

	avcodec_flush_buffers(clip->vfile->codecCtx);

	// If the wanted frame is the first frame in the video, then it is a keyframe and we just need
	// to seek to it
	if(wantedFrame == 0)
	{
		int64 pkeyfdts = clip->vfile->frames[0].dts;
		if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, pkeyfdts, flags) >= 0)
		{
			// printf("Seek to frame 0 successfull.\n");
			decodeSingleFrameCapDelay(clip);
			updateVideoClipTexture(clip);
			return true;
		}
		else
		{
			printf("Seeking to frame 0 failed!\n");
			return false;
		}
	}

	// If pkeyf == -1 then this frame is itself a keyframe, so it's parent keyframe does not
	// need to be calculated, it can be seeked to and decoded right away.
	if(pkeyf == -1)
	{
		int64 dts = clip->vfile->frames[wantedFrame].dts; 
		if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, dts, flags) >= 0)
		{
			decodeSingleFrameCapDelay(clip);
			updateVideoClipTexture(clip);
			return true;
		}
		else
		{
			printf("Keyframe seek failed.\n\n");
			return false;
		}
	}
	else
	{
		// Otherwise, the frame is NOT a keyframe and we need to seek to it's parent keyframe
		// and decode from to the parent keyframe and up to the frame we want
		int64 pkeyfdts = clip->vfile->frames[pkeyf].dts;
		flags = AVSEEK_FLAG_BACKWARD; // We MUST set this to backwards. The pkeyf is always behind!
		// Try to seek to the parent keyframe
		if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, pkeyfdts, flags) >= 0)
		{
			int wantedPts = clip->vfile->ptsListSorted[wantedFrame];
			int currentPts = clip->vfile->frames[pkeyf].pts;
			int count = (wantedFrame - pkeyf) + 10;
			int i = 0;
			while((wantedPts != currentPts) && (i <= count))
			{
				// Decode all the frames a quickly as possible (without the cap delay flush)
				decodeSingleFrameNoCapDelay(clip);
				++i;
				currentPts = clip->vfile->frames[pkeyf + i].pts;
			}
			// 
			if(i >= count)
			{
				printf("Something went horribly wrong: iterator is >= count.\n\n");
			}
			// Quickly flush the buffer
			flushClipEnd(clip);
			// Finally decode the last frame (with the cap delay), which is the frame we actually want!!
			decodeSingleFrameCapDelay(clip);
			updateVideoClipTexture(clip);
			return true;
		}
		else
		{
			printf("Parent keyframe seek failed.\n\n");
			return false;
		}
	}

	return 1;
}

void probeForNumberOfFrames(VideoFile *vfile)
{
	float estimatedFrames = 
		ceil(((float)vfile->formatCtx->duration / AV_TIME_BASE) * vfile->framerate) + 10;
	printf("Estimated Frames: %f\n", estimatedFrames);
	vfile->_framesListSize = estimatedFrames;
	vfile->frames = (Frame *)malloc(estimatedFrames * sizeof(Frame));

	vfile->_ptsListSize = estimatedFrames;
	vfile->ptsList = (int *)malloc(estimatedFrames * sizeof(int));

	vfile->keyframeList = (int *)malloc(estimatedFrames * sizeof(int));

	uint32 nframes = 0;

	AVFormatContext *formatCtx = NULL;
	if(avformat_open_input(&formatCtx, vfile->formatCtx->filename, NULL, NULL) != 0)
	{
		printf("Could not open file: \"%s\".\n", vfile->formatCtx->filename);
		exit(-1);
	}

	AVCodecContext *codecCtxOrig = vfile->stream->codec;
	AVCodec *codec = avcodec_find_decoder(codecCtxOrig->codec_id);

	AVCodecContext *codecCtx	= avcodec_alloc_context3(codec);
	avcodec_copy_context(codecCtx, codecCtxOrig);

	avcodec_open2(codecCtx, codec, NULL);

	AVFrame *frame = av_frame_alloc();

	AVPacket packet;
	av_init_packet(&packet);

	int gotFrame = 0;
	int ret = 0;
	int parentKeyframe = -2;

	uint64 start = (uint64)SDL_GetTicks();
	while(av_read_frame(formatCtx, &packet) >= 0)
	{
		if(packet.stream_index == vfile->streamIndex)
		{
			vfile->frames[nframes].parentKeyframe = parentKeyframe;
			if(packet.flags & AV_PKT_FLAG_KEY)
			{
				vfile->frames[nframes].parentKeyframe = -1;
				parentKeyframe = nframes;
				vfile->keyframeList[vfile->nkeyframes] = nframes;
				vfile->nkeyframes++;
			}
			vfile->frames[nframes].pts = packet.pts;
			vfile->frames[nframes].dts = packet.dts;
			vfile->ptsList[nframes] = packet.pts;
			nframes++;
			assert(nframes <= estimatedFrames);		
		}
		av_packet_unref(&packet);
	}
	uint64 end = (uint64)SDL_GetTicks();
	uint64 elapsed = end - start;
	printTiming("probing frames", elapsed);

	av_frame_free(&frame);
	avcodec_flush_buffers(codecCtx);
	avformat_close_input(&formatCtx);
	avcodec_close(codecCtx);
	avcodec_close(codecCtxOrig);

	vfile->nframes = nframes;

	// TODO: pts list qsort

	vfile->ptsListSorted = (int *)malloc(vfile->_ptsListSize * sizeof(int));

	for(int i = 0; i < nframes; ++i)
	{
		vfile->ptsListSorted[i] = vfile->ptsList[i];
	}

	qsort(vfile->ptsListSorted, nframes, sizeof(int), ptsCompare);

#if 0
	for(int i = 0; i < nframes; ++i)
	{
		
		int64 pts = vfile->frames[i].pts;
		int64 dts = vfile->frames[i].dts;
		int pkey = vfile->frames[i].parentKeyframe;

		printf("FRAME %d: \t PTS: %d,\t DTS: %d, \t PKEY: %d\n", 
		       i + 1, pts, dts, pkey); 
	}
	printf("\n");
#endif
}

void createVideoClip(VideoClip *clip, VideoFile *vfile, SDL_Renderer *renderer, int number)
{
	clip->vfile = vfile;

	clip->width = vfile->width;
	clip->height = vfile->height;

	clip->arW = vfile->arW;
	clip->arH = vfile->arH;

	clip->tlRect.x = 0;
	clip->tlRect.y = 0;
	clip->tlRect.w = clip->vfile->width;
	clip->tlRect.h = 10;

	clip->videoRect.x = 0;
	clip->videoRect.y = 0;
	clip->videoRect.w = clip->vfile->width;
	clip->videoRect.h = clip->vfile->height;

	int yPlaneSz = clip->vfile->codecCtx->width * clip->vfile->codecCtx->height;
	int uvPlaneSz = clip->vfile->codecCtx->width * clip->vfile->codecCtx->height / 4;

	clip->yPlane = (uint8 *)malloc(yPlaneSz);
	clip->uPlane = (uint8 *)malloc(uvPlaneSz);
	clip->vPlane = (uint8 *)malloc(uvPlaneSz);

	clip->uvPitch = clip->vfile->codecCtx->width / 2;

	clip->frame = av_frame_alloc();

	clip->swsCtx = sws_getContext(clip->width,
	                              clip->height,
	                              clip->vfile->codecCtx->pix_fmt,
	                              clip->width,
	                              clip->height,
	                              AV_PIX_FMT_YUV420P,
	                              SWS_BILINEAR,
	                              NULL,
	                              NULL,
	                              NULL);

	clip->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
	                                  clip->vfile->width, clip->vfile->height);

	clip->beginFrame = 0;
	clip->endFrame = clip->vfile->nframes - 1;

	decodeSingleFrameCapDelay(clip);
	updateVideoClipTexture(clip);

	clip->number = number;

	clip->filename = av_strdup(clip->vfile->formatCtx->filename);
}

void loadVideoFile(VideoFile *vfile, SDL_Renderer *renderer, const char *filename)
{
	vfile->formatCtx = NULL;
	if(avformat_open_input(&vfile->formatCtx, filename, NULL, NULL) != 0)
	{
		printf("Could not open file: %s\n", filename);
		exit(-1);
	}

	avformat_find_stream_info(vfile->formatCtx, NULL);

	// av_dump_format(vfile->formatCtx, 0, filename, 0); // DEBUG

	vfile->streamIndex = -1;

	for(int i = 0; i < vfile->formatCtx->nb_streams; ++i)
	{
		if(vfile->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			vfile->streamIndex = i;
			vfile->stream = vfile->formatCtx->streams[vfile->streamIndex];
			break;
		}
	}
	assert(vfile->streamIndex != -1);

	AVCodecContext *codecCtxOrig = vfile->formatCtx->streams[vfile->streamIndex]->codec;
	vfile->codec = avcodec_find_decoder(codecCtxOrig->codec_id);
	
	vfile->codecCtx	= avcodec_alloc_context3(vfile->codec);
	avcodec_copy_context(vfile->codecCtx, codecCtxOrig);

	vfile->width = vfile->codecCtx->width;
	vfile->height = vfile->codecCtx->height;

	avcodec_close(codecCtxOrig);

	avcodec_open2(vfile->codecCtx, vfile->codec, NULL);

	AVRational tb = vfile->stream->time_base;
	vfile->timeBase = ((int64)tb.num * AV_TIME_BASE) / (int64)tb.den;

	// Use the video stream to get the real base framerate and average framerates
	vfile->stream = vfile->formatCtx->streams[vfile->streamIndex];
	AVRational fr = vfile->stream->avg_frame_rate;
	vfile->framerate = (float)fr.num / (float)fr.den;
	AVRational afr = vfile->stream->avg_frame_rate;
	vfile->avgFramerate = (float)afr.num / (float)afr.den;
	vfile->msperframe = (1/vfile->framerate) * 1000.0f;
	vfile->bitrate = vfile->stream->codec->bit_rate;
	
	// Use av_reduce() to reduce a fraction to get the width and height of the vfile's aspect ratio
	av_reduce(&vfile->arW, &vfile->arH, vfile->codecCtx->width, vfile->codecCtx->height, 1024);
	vfile->arF = (float)vfile->arW / (float)vfile->arH;

	probeForNumberOfFrames(vfile);

	avcodec_close(codecCtxOrig);
}

void printVideoFileInfo(VideoFile vfile)
{
	printf("< VIDEO FILE\n");
	printf("Filename: %s\n", vfile.formatCtx->filename);
	printf("Codec: %s\n", vfile.codec->long_name);
	printf("Duration: ");
	if (vfile.formatCtx->duration != AV_NOPTS_VALUE)
	{
		int64 duration = vfile.formatCtx->duration + 
		(vfile.formatCtx->duration <= INT64_MAX - 5000 ? 5000 : 0);
		int secs = duration / AV_TIME_BASE;
		int us = duration % AV_TIME_BASE;
		int mins = secs / 60;
		secs %= 60;
		int hours = mins / 60;
		mins %= 60;
		printf("%02d:%02d:%02d.%02d\n", hours, mins, secs, (100 * us) / AV_TIME_BASE);
	}
	if (vfile.formatCtx->start_time != AV_NOPTS_VALUE)
	{
		int secs, us;
		printf("Start time: ");
		secs = vfile.formatCtx->start_time / AV_TIME_BASE;
		us   = llabs(vfile.formatCtx->start_time % AV_TIME_BASE);
		printf("%d.%02d\n", secs, (int) av_rescale(us, 1000000, AV_TIME_BASE));
	}
	printf("Video bitrate: ");
	if (vfile.formatCtx->bit_rate) printf("%d kb/s\n", (int64)vfile.formatCtx->bit_rate / 1000);
	else printf("N/A\n");
	printf("Timebase: %d\n", vfile.timeBase);
	printf("Real based framerate: %.2f fps\n", vfile.framerate);
	printf("Milliseconds per frame: %.4f ms\n", vfile.msperframe);
	printf("Average framerate: %.2f fps\n", vfile.avgFramerate);
	printf("Width/Height: %dx%d\n", vfile.width, vfile.height);
	printf("Aspect Ratio: (%.2f), [%d:%d]\n", vfile.arF, vfile.arW, vfile.arH);
	printf("Keyframes: %d\n", vfile.nkeyframes);
	#if 0
	printf("\t[ ");
	for(int i = 0, j = 0; i < vfile.nkeyframes - 1; ++i, ++j)
	{
		if(j > 10)
		{
			printf("\n\t");
			j = 0;
		}
		printf("%d, ", vfile.keyframeList[i]);
	}
	printf("%d ]\n", vfile.keyframeList[vfile.nkeyframes - 1]);
	printf("PTS List:\n");
	printf("\t[");
	for(int i = 0, j = 0; i < vfile.ntotalFrames - 1; ++i, ++j)
	{
		if(j > 5)
		{
			printf("\n\t");
			j = 0;
		}
		printf("%d: %d, ", i, vfile.ptsList[i]);
	}
	printf("%d: %d ]\n", vfile.ntotalFrames - 1, vfile.ptsList[vfile.ntotalFrames - 1]);
	printf("\nPTS List Sorted:\n");
	printf("\t[");
	for(int i = 0, j = 0; i < vfile.ntotalFrames - 1; ++i, ++j)
	{
		if(j > 5)
		{
			printf("\n\t");
			j = 0;
		}
		printf("%d: %d, ", i, vfile.ptsListSorted[i]);
	}
	printf("%d: %d ]\n", vfile.ntotalFrames - 1, vfile.ptsListSorted[vfile.ntotalFrames - 1]);
	#endif
	printf("Number of frames: %d\n", vfile.nframes);
	printf("> VIDEO FILE\n");
	printf("\n");
}

void printVideoClipInfo(VideoClip clip)
{
	printf("< VIDEO CLIP\n");
	printf("Video file filename: %s\n", clip.vfile->formatCtx->filename);
	printf("Timeline rectangle:\n");
	printf("\t(x,y) : (%d,%d)\n", clip.tlRect.x, clip.tlRect.y);
	printf("\t(w,h) : (%d,%d)\n", clip.tlRect.w, clip.tlRect.h);
	printf("Video rectangle: \n");
	printf("\t(x,y) : (%d,%d)\n", clip.videoRect.x, clip.videoRect.y);
	printf("\t(w,h) : (%d,%d)\n", clip.videoRect.w, clip.videoRect.h);
	printf("Aspect ratio: [%d:%d]\n", clip.arW, clip.arH);
	printf("Beginning frame: %d\n", clip.beginFrame);
	printf("End frame: %d\n", clip.endFrame);
	printf("> VIDEO CLIP\n");
	printf("\n");
}

#endif