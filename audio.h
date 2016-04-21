#ifndef AUIDO_H
#define AUDIO_H
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

SDL_AudioDeviceID initAudioDevice(SDL_AudioSpec spec)
{
	SDL_AudioDeviceID result;

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
	printf("Available audio output devices: \n");
	for(int i = 0; i < numAudioDevices; ++i)
	{
		printf("Device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
	}
	printf("\n");

	// TODO: Get an audio device specified by the user. NULL for the first argument gets the default.
	result = SDL_OpenAudioDevice(NULL, 0, &wantedSpec, &spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
	assert(result != 0);
	SDL_PauseAudioDevice(result, 1); // Pause the audio so we don't try to output sound!

	return result;
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

void initAudioClip(AudioClip *clip, SDL_AudioSpec spec, const char *filename, bool loop)
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

	copyAudioSpec(&clip->spec, spec);

	clip->bitrate = clip->stream->codec->bit_rate;
	clip->samplerate = clip->stream->codec->sample_rate;
	clip->channels = clip->stream->codec->channels;
	clip->layout = clip->stream->codec->channel_layout;
	clip->sampleFmt = clip->stream->codec->sample_fmt;

	clip->swrCtx = swr_alloc();
	clip->swrCtx = swr_alloc_set_opts(clip->swrCtx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 
	                                  spec.freq, clip->channels, clip->sampleFmt, 
	                                  clip->samplerate, 0, NULL);
	swr_init(clip->swrCtx);

	avcodec_close(codecCtxOrig);
}

#endif