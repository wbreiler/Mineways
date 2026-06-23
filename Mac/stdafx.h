// macOS replacement for Win/stdafx.h
// Included before all source files in the Mac build.
#pragma once

// Must come first — defines HANDLE, Windows types, wide-path helpers
#include "compat.h"

// Standard C/C++ headers
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// Project version
#define MINEWAYS_MAJOR_VERSION 13
#define MINEWAYS_MINOR_VERSION 0

// Utility macros (match Win/stdafx.h)
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef clamp
#define clamp(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
#endif
#ifndef swapint
#define swapint(a,b) {int _t=(a);(a)=(b);(b)=_t;}
#endif

#define MAX_PATH_AND_FILE (2*MAX_PATH)

// Platform-abstracted file I/O — wide-path version for macOS
#define PORTAFILE FILE*
#define PortaOpen(fn)       _mwPortaOpenW(fn)
#define PortaAppend(fn)     _mwPortaAppendW(fn)
#define PortaCreate(fn)     _mwPortaCreateW(fn)
#define PortaSeek(h,ofs)    fseek(h, ofs, SEEK_SET)
#define PortaRead(h,buf,len) (fread(buf, len, 1, h) != 1)
#define PortaWrite(h,buf,len) (fwrite(buf, len, 1, h) != 1)
#define PortaClose(h)       fclose(h)

// Compat shims already defined in compat.h
#define strncpy_s(f,n,w,m) strncpy(f,w,m)
#define strncat_s(f,n,w,m) strncat(f,w,m)
#define sprintf_s           snprintf
#define _snprintf_s(buf,sz,cnt,...) snprintf(buf,sz,__VA_ARGS__)
#define sscanf_s            sscanf
#define _wcsnicmp(a,b,n)    wcsncasecmp(a,b,n)
#define _wcsicmp(a,b)       wcscasecmp(a,b)

// _countof equivalent
#ifndef _countof
#define _countof(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

// Windows BOOL result from file ops (non-zero = success)
#define BR_UNUSED 0
