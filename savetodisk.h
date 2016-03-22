#ifndef SAVETODISK_H
#define SAVETODISK_H

void SaveFrameAsPPM(AVFrame *pFrame, int width, int height, int iFrame, const char *folder)
{
	FILE *pFile;
	char szFilename[32];

	sprintf(szFilename, "%sframe%d.ppm", folder, iFrame);
	printf("Saving frame %d to file %s\n", iFrame, szFilename);
	pFile = fopen(szFilename, "wb");
	if(pFile == NULL) return;

	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	for(int y = 0; y < height; ++y)
	{
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);
	}

	int fd = fileno(pFile);
	struct stat buff;
	fstat(fd, &buff);
	int32 sz = buff.st_size;

	fclose(pFile);

	printf("Finished saving file %s, %d bytes\n", szFilename, sz);
}

// TODO: Clean this up??
void OutputFramesToDisk(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, struct SwsContext *sws_ctx,
                        AVPacket packet, AVFrame *pFrame, AVFrame *pFrameRGB, int32 videoStreamIndex)
{
	int32 i = 0;
	int32 frameFinished;
	while(av_read_frame(pFormatCtx, &packet) >= 0)
	{
		if(packet.stream_index == videoStreamIndex)
		{
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			if(frameFinished)
			{
				sws_scale(sws_ctx, (uint8 const * const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
				          pFrameRGB->data, pFrameRGB->linesize);
				if(++i <= 5)
				{
					SaveFrameAsPPM(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i, "res/frames/");
				}
			}
		}
		av_free_packet(&packet);
	}
}

#endif