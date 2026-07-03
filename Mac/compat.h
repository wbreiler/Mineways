// macOS compatibility shims for Mineways Win32 source
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

// errno_t: C11 type, not always exposed on macOS headers
#ifndef errno_t
typedef int errno_t;
#endif

// ── Basic Windows types ───────────────────────────────────────────────────────
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;
typedef long           BOOL;
typedef void*          LPVOID;
typedef unsigned int   UINT;
typedef long long      LONGLONG;
typedef unsigned long long ULONGLONG;
typedef int            INT;
typedef long           LONG;
typedef unsigned long  ULONG;

// boolean (used by ObjFileManip.cpp — Java-ish type, not standard C++)
typedef bool boolean;

// Win32 opaque handle types (not used functionally in the Mac build)
typedef void* HKEY;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HMODULE;

// HANDLE is FILE* on macOS (PortaCreate/PortaWrite/PortaClose all use FILE*)
typedef FILE* HANDLE;

#define TRUE  1
#define FALSE 0

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// INVALID_HANDLE_VALUE: PortaOpen returns FILE* (NULL on failure)
#define INVALID_HANDLE_VALUE ((FILE*)NULL)

// ── qsort_s: Windows has (base, num, size, compare, ctx); macOS qsort_r is (base, num, size, ctx, compare) ──
// ponytail: wraps argument reorder — same semantics, different call-site order
static inline void qsort_s(void* base, size_t num, size_t size,
                            int (*compare)(void*, const void*, const void*),
                            void* ctx)
{
    qsort_r(base, num, size, ctx, compare);
}

// ── strcpy_s 2-arg template (Windows allows strcpy_s(dst,src) for arrays) ────
#ifdef __cplusplus
template<size_t N>
inline errno_t strcpy_s(char (&dst)[N], const char* src) {
    strncpy(dst, src, N - 1); dst[N - 1] = '\0'; return 0;
}
template<size_t N>
inline errno_t strcpy_s(wchar_t (&dst)[N], const wchar_t* src) {
    wcsncpy(dst, src, N - 1); dst[N - 1] = L'\0'; return 0;
}
// 3-arg versions for pointer targets (fall-through to macro below if not array)
inline errno_t strcpy_s(char* dst, size_t n, const char* src) {
    strncpy(dst, src, n - 1); dst[n - 1] = '\0'; return 0;
}
#endif

// ── Calling conventions (no-ops on ARM64/macOS) ───────────────────────────────
#define __stdcall
#define __cdecl
#define __forceinline __attribute__((always_inline)) inline
#ifndef __declspec
#define __declspec(x)
#endif
#define WINAPI
#define WINAPIV
#define CALLBACK

// ── TCHAR — keep as wchar_t to match existing L"..." literals in core files ───
#ifndef TCHAR
typedef wchar_t TCHAR;
#endif
#ifndef _T
#define _T(x) L##x
#endif
#define TEXT(x) L##x

// ── String aliases ────────────────────────────────────────────────────────────
#define _strdup strdup
#ifndef strcat_s
static inline errno_t _mwStrcat_s(char* dst, size_t n, const char* src) {
    strncat(dst, src, n - strlen(dst) - 1);
    return 0;
}
#define strcat_s(dst, n, src) _mwStrcat_s(dst, n, src)
#endif

// Wide-string aliases
#define _wcsicmp(a,b)       wcscasecmp(a,b)
#define _wcsnicmp(a,b,n)    wcsncasecmp(a,b,n)
// Narrow-string case-insensitive
#define _stricmp(a,b)       strcasecmp(a,b)
#define _strnicmp(a,b,n)    strncasecmp(a,b,n)

static inline errno_t _strlwr_s(char* s, size_t /*n*/) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
    return 0;
}
static inline errno_t _wcslwr_s(wchar_t* s, size_t /*n*/) {
    for (; *s; ++s) *s = (wchar_t)towlower(*s);
    return 0;
}

// Time shims (__time32_t / _time32 / _localtime32_s / asctime_s)
#include <time.h>
#define __time32_t  time_t
#define _time32(p)  time(p)
static inline errno_t _localtime32_s(struct tm* t, const time_t* clock) {
    return localtime_r(clock, t) ? 0 : 1;
}
static inline errno_t asctime_s(char* buf, size_t sz, const struct tm* t) {
    char* s = asctime(t);
    if (!s) return 1;
    strncpy(buf, s, sz - 1);
    buf[sz - 1] = '\0';
    return 0;
}

static inline errno_t _mwWcscpy_s(wchar_t* dst, size_t n, const wchar_t* src) {
    wcsncpy(dst, src, n - 1);
    dst[n - 1] = L'\0';
    return 0;
}
#define wcscpy_s(dst, n, src) _mwWcscpy_s(dst, n, src)

static inline errno_t _mwWcscat_s(wchar_t* dst, size_t n, const wchar_t* src) {
    size_t dstLen = wcslen(dst);
    wcsncat(dst, src, n - dstLen - 1);
    return 0;
}
#define wcscat_s(dst, n, src) _mwWcscat_s(dst, n, src)
#ifndef wcsncat_s
#define wcsncat_s(dst, n, src, cnt) (wcsncat(dst, src, cnt), 0)
#endif

static inline errno_t wcsncpy_s(wchar_t* dst, size_t dstSz, const wchar_t* src, size_t cnt) {
    size_t n = (cnt < dstSz - 1) ? cnt : dstSz - 1;
    wcsncpy(dst, src, n);
    dst[n] = L'\0';
    return 0;
}

// ── Missing CRT helpers ───────────────────────────────────────────────────────
// _fileno: macOS has fileno() (POSIX); Windows spells it _fileno
#ifndef _fileno
#define _fileno fileno
#endif

// swprintf_s: macOS has swprintf with identical args
#ifndef swprintf_s
#define swprintf_s(buf, count, ...) swprintf(buf, count, __VA_ARGS__)
#endif

// wcscpy_s / wcscat_s (safe variants not in POSIX)
#ifndef wcscpy_s
#define wcscpy_s(dst, n, src) wcsncpy(dst, src, n)
#endif
#ifndef wcscat_s
#define wcscat_s(dst, n, src) wcsncat(dst, n - wcslen(dst) - 1, src)  // ponytail: approximate
#endif
#ifndef wcsncat_s
#define wcsncat_s(dst, n, src, cnt) wcsncat(dst, src, cnt)
#endif

// _wfopen_s: Windows wide-path fopen; macOS needs wcstombs conversion
static inline int _wfopen_s(FILE** pFile, const wchar_t* path, const wchar_t* mode)
{
    char p[4096], m[32];
    wcstombs(p, path, sizeof(p));
    wcstombs(m, mode, sizeof(m));
    *pFile = fopen(p, m);
    return (*pFile == NULL) ? 1 : 0;
}

// ── Win32 text encoding stubs (MultiByteToWideChar / WideCharToMultiByte) ────
// ponytail: only handles UTF-8 ↔ wchar_t; sufficient for Mineways path/name conversions
#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008
static inline int MultiByteToWideChar(unsigned /*cp*/, unsigned /*flags*/,
                                       const char* mbstr, int mblen,
                                       wchar_t* wcstr, int wclen)
{
    if (mblen == -1) mblen = (int)strlen(mbstr) + 1;
    return (int)mbstowcs(wcstr, mbstr, (size_t)wclen);
}
static inline int WideCharToMultiByte(unsigned /*cp*/, unsigned /*flags*/,
                                       const wchar_t* wcstr, int wclen,
                                       char* mbstr, int mblen,
                                       const char* /*dflt*/, int* /*used*/)
{
    if (wclen == -1) wclen = (int)wcslen(wcstr) + 1;
    return (int)wcstombs(mbstr, wcstr, (size_t)mblen);
}

// ── Win32 filesystem stubs ────────────────────────────────────────────────────
#define ERROR_ALREADY_EXISTS EEXIST
static inline DWORD GetLastError() { return (DWORD)errno; }

static inline BOOL CreateDirectoryW(const wchar_t* wpath, void* /*sa*/)
{
    char path[4096];
    wcstombs(path, wpath, sizeof(path));
    return (mkdir(path, 0755) == 0 || errno == EEXIST) ? TRUE : FALSE;
}

// ── Win32 file I/O stubs (maps to fwrite/fread via HANDLE=FILE*) ─────────────
static inline BOOL WriteFile(HANDLE h, const void* buf, DWORD len,
                              DWORD* written, void* /*ov*/)
{
    size_t n = fwrite(buf, 1, len, h);
    if (written) *written = (DWORD)n;
    return (n == (size_t)len) ? TRUE : FALSE;
}

static inline BOOL ReadFile(HANDLE h, void* buf, DWORD len,
                             DWORD* nread, void* /*ov*/)
{
    size_t n = fread(buf, 1, len, h);
    if (nread) *nread = (DWORD)n;
    return TRUE;
}

static inline DWORD GetFileSize(HANDLE h, DWORD* /*high*/)
{
    long cur = ftell(h);
    fseek(h, 0, SEEK_END);
    long sz = ftell(h);
    fseek(h, cur, SEEK_SET);
    return (DWORD)sz;
}

// ── Wide-path PortaOpen/Create/Append (converts wchar_t* → UTF-8) ─────────────
// These are referenced by macros in stdafx.h below
static inline FILE* _mwPortaOpenW(const wchar_t* wpath)
{
    char path[4096];
    wcstombs(path, wpath, sizeof(path));
    return fopen(path, "rb");
}
static inline FILE* _mwPortaAppendW(const wchar_t* wpath)
{
    char path[4096];
    wcstombs(path, wpath, sizeof(path));
    return fopen(path, "a");
}
static inline FILE* _mwPortaCreateW(const wchar_t* wpath)
{
    char path[4096];
    wcstombs(path, wpath, sizeof(path));
    // Expand leading ~/ in case caller didn't (fopen won't do it)
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char* home = getenv("HOME");
        if (home) {
            char expanded[4096];
            snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
            return fopen(expanded, "wb");
        }
    }
    return fopen(path, "wb");
}

// ── Win32 directory enumeration (POSIX opendir/readdir/fnmatch backend) ───────
#include <dirent.h>
#include <fnmatch.h>

#define INVALID_FILE_ATTRIBUTES ((DWORD)0xFFFFFFFF)
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL    0x00000080
#define _TRUNCATE                ((size_t)-1)

static inline DWORD GetFileAttributesW(const wchar_t* wpath)
{
    char path[4096]; wcstombs(path, wpath, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

typedef struct _WIN32_FIND_DATAW {
    DWORD   dwFileAttributes;
    wchar_t cFileName[MAX_PATH];
} WIN32_FIND_DATA;

struct _mwFindHandle {
    DIR*  dir;
    char  pattern[256];
    char  dirPath[4096];
};

static inline bool _mwFindMatch(struct dirent* de, struct _mwFindHandle* fh,
                                WIN32_FIND_DATA* ffd)
{
    if (fnmatch(fh->pattern, de->d_name, FNM_CASEFOLD) != 0) return false;
    mbstowcs(ffd->cFileName, de->d_name, MAX_PATH);
    char full[4096 + 256];
    snprintf(full, sizeof(full), "%s/%s", fh->dirPath, de->d_name);
    struct stat st;
    ffd->dwFileAttributes = (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                            ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    return true;
}

static inline HANDLE FindFirstFileW(const wchar_t* wpattern, WIN32_FIND_DATA* ffd)
{
    char pat[4096]; wcstombs(pat, wpattern, sizeof(pat));
    char* slash = strrchr(pat, '/');
    if (!slash) slash = strrchr(pat, '\\');
    char dirPath[4096], filePat[256];
    if (slash) {
        size_t n = (size_t)(slash - pat);
        strncpy(dirPath, pat, n); dirPath[n] = '\0';
        strncpy(filePat, slash + 1, sizeof(filePat) - 1); filePat[sizeof(filePat)-1] = '\0';
    } else {
        strcpy(dirPath, "."); strncpy(filePat, pat, sizeof(filePat)-1); filePat[sizeof(filePat)-1]='\0';
    }
    DIR* d = opendir(dirPath);
    if (!d) return INVALID_HANDLE_VALUE;
    struct _mwFindHandle* fh = (struct _mwFindHandle*)malloc(sizeof(struct _mwFindHandle));
    fh->dir = d;
    strncpy(fh->pattern, filePat, sizeof(fh->pattern)-1); fh->pattern[sizeof(fh->pattern)-1]='\0';
    strncpy(fh->dirPath, dirPath, sizeof(fh->dirPath)-1); fh->dirPath[sizeof(fh->dirPath)-1]='\0';
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (_mwFindMatch(de, fh, ffd)) return (HANDLE)fh;
    }
    closedir(d); free(fh);
    return INVALID_HANDLE_VALUE;
}

static inline BOOL FindNextFileW(HANDLE hFind, WIN32_FIND_DATA* ffd)
{
    struct _mwFindHandle* fh = (struct _mwFindHandle*)hFind;
    struct dirent* de;
    while ((de = readdir(fh->dir)) != NULL) {
        if (_mwFindMatch(de, fh, ffd)) return TRUE;
    }
    return FALSE;
}

static inline BOOL FindClose(HANDLE hFind)
{
    struct _mwFindHandle* fh = (struct _mwFindHandle*)hFind;
    closedir(fh->dir); free(fh);
    return TRUE;
}

#define FindFirstFile  FindFirstFileW
#define FindNextFile   FindNextFileW

// ── SKETCHFAB HTTP upload stubs (curl/openssl not needed for core export) ────
// If SKETCHFAB is defined the source pulls in PublishSkfb.h which needs curl.
// We undef it here; Mineways.cpp (the GUI) isn't compiled anyway.
#ifdef SKETCHFAB
#undef SKETCHFAB
#endif
