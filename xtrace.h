#ifndef XTRACE_H
#define XTRACE_H

// NOTE: This is for Visual Studio convenience. It takes any call to printf and sends it to
// xtrace, which in turn calls OutputDebugString to output to the VS Output window.
#ifdef _WIN32
	#include <windows.h>
	#ifdef   XTRACE_REPLACE
		#define printf xtrace
	#elif defined XTRACE_SUPPLEMENT
		#define printf xtrace_and_printf
	#endif // REPLACE_PRINTF_WITH_XTRACE

	void xtrace(LPCTSTR lpszFormat, ...)
	{
	    va_list args;
	    va_start(args, lpszFormat);
	    int nBuf = _vscprintf(lpszFormat, args) + 1;
	    char *szBuffer = (char *)malloc(nBuf * sizeof(char));
	    vsnprintf_s(szBuffer, nBuf, nBuf - 1, lpszFormat, args);
	    OutputDebugString(szBuffer);
	    free(szBuffer);
	    va_end(args);
	}

	void xtrace_and_printf(LPCTSTR lpszFormat, ...)
	{
		#undef printf
		va_list args;
		va_start(args, lpszFormat);
		int nBuf = _vscprintf(lpszFormat, args) + 1;
		char *szBuffer = (char *)malloc(nBuf * sizeof(char));
		vsnprintf_s(szBuffer, nBuf, nBuf - 1, lpszFormat, args);
		OutputDebugString(szBuffer);
		printf("%s", szBuffer);
		free(szBuffer);
		va_end(args);
		#define printf xtrace_and_printf
	}
#endif // _WIN32

#endif // XTRACE_H