#ifndef XTRACE_H
#define XTRACE_H

// NOTE: This is for Visual Studio convenience. It takes any call to printf and sends it to
// xtrace, which in turn calls OutputDebugString to output to the VS Output window.
#ifdef __WIN32__
#include <windows.h>
#define printf xtrace
void xtrace(LPCTSTR lpszFormat, ...)
{
    va_list args;
    va_start(args, lpszFormat);
    int nBuf;
    TCHAR szBuffer[512]; // get rid of this hard-coded buffer
    nBuf = _vsnprintf_s(szBuffer, 511, lpszFormat, args);
    ::OutputDebugString(szBuffer);
    va_end(args);
}
#endif

#endif