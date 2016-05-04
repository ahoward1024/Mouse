#ifndef VIDEO_H
#define VIDEO_H

// Frame Data
struct Frame
{
	AVPictureType  picType = AV_PICTURE_TYPE_NONE;
	int            pkeyf = 0; // Parent keyframe
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
	int              _framesSize;
	int             *keyframeList; 
	int              streamIndex   = 0;
	int              bitrate       = 0;
	int              arW           = 0;
	int              arH           = 0;
	int              width         = 0;
	int              height        = 0;
	uint32           nkeyframes    = 0;
	uint32           nvideoFrames  = 0;
	uint32					 ntotalFrames  = 0;
	uint64           timeBase      = 0;
	float            framerate     = 0.0f;
	float            avgFramerate  = 0.0f;
	float            msperframe    = 0.0f;
	float            arF           = 0.0f;
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
};

// This will free the clip for reinitalization, we do not free the texture
// as SDL still needs it for video resizing.
void freeVideoFile(VideoFile *vfile)
{
	printf("Freeing video file: %s\n\n", vfile->formatCtx->filename); // DEBUG
	avcodec_close(vfile->codecCtx);
	avformat_close_input(&vfile->formatCtx);
}

void freeVideoClip(VideoClip *clip)
{
	printf("Freeing video clip %d : %s\n", clip->number, clip->vfile->formatCtx->filename);
	av_free(clip->swsCtx);
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

// Return 0 for sucessful frame, 1 for end of file or error
int nextFullFrame(VideoClip *clip)
{
	printf("\n< START: NEXT FULL FRAME:\n");
	AVPacket packet;
	av_init_packet(&packet);
	int gotFrame = 0;
	int ret = 0;
	do
	{
		if(av_read_frame(clip->vfile->formatCtx, &packet) >= 0)
		{
			if(packet.stream_index == clip->vfile->streamIndex)
			{
				ret = avcodec_decode_video2(clip->vfile->codecCtx, clip->frame, &gotFrame, &packet);
				AVPictureType pt = clip->frame->pict_type;
				printf("DECODE: PICTYPE: %s\n", avpictypeChar(pt));
				printf("DECODE: DTS %d\n", packet.dts);
			}
		}
		av_packet_unref(&packet);
	} while(!gotFrame);
	av_packet_unref(&packet);
	printf("> DONE: NEXT FULL FRAME\n\n");
	return ret;
}

int singleFrame(VideoClip *clip)
{
	printf("< START: SINGLE FRAME\n");
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
	printf("DECODE DTS: %d\n", clip->frame->pkt_dts);
	printf("DECODE PTS: %d\n", clip->frame->pkt_pts);
	printf("DECODE PICTYPE: %s\n", avpictypeChar(clip->frame->pict_type));
	printf("> DONE: SINGLE FRAME\n");
	return result;
}

int seekToFrame(VideoClip *clip, int index, int currentIndex)
{
	int64 timestamp = clip->vfile->frames[index].dts;
	int flags = 0;
	if(index < currentIndex) flags = AVSEEK_FLAG_BACKWARD;
	int pkeyf = clip->vfile->frames[index].pkeyf;

	printf("SEEK: %s\n", (flags & AVSEEK_FLAG_BACKWARD) ? "BACKWARDS" : "FORWARDS");
	printf("Wandted index: %d, Current index: %d\n", index, currentIndex);

	printf("PICTYPE at INDEX: %s\n", avpictypeChar(clip->vfile->frames[index].picType));

	avcodec_flush_buffers(clip->vfile->codecCtx);

	if(pkeyf == -1)
	{
		if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, timestamp, flags) >= 0)
		{
			printf("Keyframe seek sucessful.\n");
			singleFrame(clip);
			printf("PICTYPE from DECODE: %s\n\n", avpictypeChar(clip->frame->pict_type));
			updateVideoClipTexture(clip);
		}
		else
		{
			printf("Keyframe seek failed.\n\n");
			return 0;
		}
	}
	else
	{
		timestamp = clip->vfile->frames[pkeyf].dts;
		flags = AVSEEK_FLAG_BACKWARD;
		if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, timestamp, flags) >= 0)
		{
			printf("Parent keyframe (%d) seek sucessful.\n", pkeyf);
			int count = index - pkeyf;
			AVPacket packet;
			av_init_packet(&packet);
			int gotFrame = 0;
			printf("Reading %d frames\n", count);
			for(int i = 0; i <= count; ++i)
			{
				singleFrame(clip);
			}
			av_packet_unref(&packet);
			updateVideoClipTexture(clip);
			printf("\n");
		}
		else
		{
			printf("Parent keyframe seek failed.\n\n");
		}
	}

	return 1;
}

// SEEKS INDIVIDUAL FRAMES!!! DOES NOT DECODE CORRECTLY!! NEED PREVIOUS I-FRAME FIRST!!
#if 0
int seekToFrame(VideoClip *clip, int index, int currentIndex)
{
	int64 dts = clip->vfile->frames[index].dts;
	int flags = AVSEEK_FLAG_ANY;
	if(index < currentIndex) flags |= AVSEEK_FLAG_BACKWARD;

	avcodec_flush_buffers(clip->vfile->codecCtx);

	if(av_seek_frame(clip->vfile->formatCtx, clip->vfile->streamIndex, dts, flags) >= 0)
	{
		printf("Seek sucessful.\n");
		singleFrame(clip);
		printf("PICTYPE: %s\n", avpictypeChar(clip->frame->pict_type));
		updateVideoClipTexture(clip);
	}
	else
	{
		printf("Seek failed.\n");
		return 0;
	}
	return 1;
}
#endif

void probeForNumberOfFrames(VideoFile *vfile)
{
	double estimatedFrames = ceil(((double)vfile->formatCtx->duration / AV_TIME_BASE) * 
	                              vfile->framerate);
	vfile->_framesSize = estimatedFrames;
	vfile->frames = (Frame *)malloc(estimatedFrames * sizeof(Frame));

	vfile->keyframeList = (int *)malloc(estimatedFrames * sizeof(int));

	uint32 totalFrames = 0;
	uint32 videoFrames = 0;

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
	int pkeyframe = 0;

	uint64 start = (uint64)SDL_GetTicks();
	while(av_read_frame(formatCtx, &packet) >= 0)
	{
		if(packet.stream_index == vfile->streamIndex)
		{
			ret = avcodec_decode_video2(codecCtx, frame, &gotFrame, &packet);
	
			if(ret > 0)
			{
				if(codec->capabilities & AV_CODEC_CAP_DELAY)
				{
					packet.data = NULL;
					packet.size = 0;
					avcodec_decode_video2(codecCtx, frame, &gotFrame, &packet);
				}
				vfile->frames[totalFrames].picType = frame->pict_type;
				vfile->frames[totalFrames].pkeyf = pkeyframe;
				if(frame->key_frame == 1)
				{
					vfile->frames[totalFrames].pkeyf = -1;
					pkeyframe = totalFrames;
					vfile->keyframeList[vfile->nkeyframes] = totalFrames;
					vfile->nkeyframes++;
				}
				vfile->frames[totalFrames].pts = packet.pts;
				vfile->frames[totalFrames].dts = packet.dts;
				totalFrames++;
				assert(totalFrames < estimatedFrames);
			}
	
			if(gotFrame)
			{
				videoFrames++;
			}			
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

	vfile->ntotalFrames = totalFrames;
	vfile->nvideoFrames = videoFrames;

#if 1
	for(int i = 0; i < vfile->ntotalFrames; ++i)
	{
		
		AVPictureType picType = vfile->frames[i].picType;
		int64 pts = vfile->frames[i].pts;
		int64 dts = vfile->frames[i].dts;
		int pkey = vfile->frames[i].pkeyf;

		printf("FRAME %d, \t PICTYPE: %s, \t PTS: %d,\t DTS: %d, \t PKEY: %d\n", 
		       i, avpictypeChar(picType), pts, dts, pkey); 
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
	// This is pretty hacky. We do this because if the file is just raw, uncompressed frames
	// then they will not be picked up by the decoder as a full frame (for some reason).
	// But they will cound as a decoded frame so we just use the total frames.
	if(clip->vfile->nvideoFrames == 0) clip->endFrame = clip->vfile->ntotalFrames; // HACK
	else clip->endFrame = clip->vfile->nvideoFrames; // HACK

	// seekToClipAtIndex(clip, 0);

	clip->number = number;
}

void loadVideoFile(VideoFile *vfile, SDL_Renderer *renderer, const char *filename)
{
	vfile->formatCtx = NULL;
	if(avformat_open_input(&vfile->formatCtx, filename, NULL, NULL) != 0)
	{
		printf("Could not open file: %s.\n", filename);
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

	AVRational tb = vfile->codecCtx->time_base;
	vfile->timeBase = ((int64)tb.num * AV_TIME_BASE) / (int64)tb.den;

	// Use the video stream to get the real base framerate and average framerates
	vfile->stream = vfile->formatCtx->streams[vfile->streamIndex];
	AVRational fr = vfile->stream->r_frame_rate;
	vfile->framerate = (float)fr.num / (float)fr.den;
	AVRational afr = vfile->stream->avg_frame_rate;
	vfile->avgFramerate = (float)afr.num / (float)afr.den;
	vfile->msperframe = (1/vfile->framerate) * 1000;
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
	printf("Video frames: %d\n", vfile.nvideoFrames);
	printf("Keyframes: %d\n", vfile.nkeyframes);
	#if 1
	printf("\t[ ");
	for(int i = 0, j = 0; i < vfile.nkeyframes - 1; ++i, j++)
	{
		if(j > 10)
		{
			printf("\n\t");
			j = 0;
		}
		printf("%d, ", vfile.keyframeList[i]);
	}
	printf("%d ]\n", vfile.keyframeList[vfile.nkeyframes-1]);
	#endif
	printf("Total frames: %d\n", vfile.ntotalFrames);
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