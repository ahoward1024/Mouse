#ifndef UTIL_H
#define UTIL_H

#include "datatypes.h"

void printTiming(uint64 time)
{
	if(time < 1000) printf("Finished in: [00m:00s:%dms]\n\n", time);
	else if(time < 60000)
	{
		uint64 ms = time % 1000;
		uint64 s = (time / 1000) % 60;
		char *spad = "0";
		if(s > 9) spad = "";
		printf("Finished in: [00m:%s%ds:%dms]\n\n", spad, s, ms);
	}
	else
	{
		uint64 ms = time % 1000;
		uint64 s = (time / 1000) % 60;
		uint64 m = (time / (60000)) % 60;
		char *spad = "0";
		if(s > 9) spad = "";
		char *mpad = "0";
		if(m > 9) mpad = "";
		printf("Finished in: [%s%dm:%s%ds:%dms]\n\n", mpad, m, spad, s, ms);
	}
}

void printTiming(const char *message, uint64 time)
{
	if(time < 1000) printf("Finished %s in: [00m:00s:%dms]\n\n", message, time);
	else if(time < 60000)
	{
		uint64 ms = time % 1000;
		uint64 s = (time / 1000) % 60;
		char *spad = "0";
		if(s > 9) spad = "";
		printf("Finished %s in: [00m:%s%ds:%dms]\n\n", message, spad, s, ms);
	}
	else
	{
		uint64 ms = time % 1000;
		uint64 s = (time / 1000) % 60;
		uint64 m = (time / (60000)) % 60;
		char *spad = "0";
		if(s > 9) spad = "";
		char *mpad = "0";
		if(m > 9) mpad = "";
		printf("Finished %s in: [%s%dm:%s%ds:%dms]\n\n", message, mpad, m, spad, s, ms);
	}
}

#endif