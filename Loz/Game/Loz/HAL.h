// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez

   Licensed under the Apache License, Version 2.0.
   See the LICENSE text file for details.
*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <memory>
#include <array>

typedef unsigned int uint;

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef _CRT_WARN
#define _CRT_WARN 0
#endif
#ifndef _RPT0
#define _RPT0(rptno, msg) ((void)0)
#endif
#ifndef _RPT1
#define _RPT1(rptno, msg, a1) ((void)0)
#endif
#ifndef _RPT2
#define _RPT2(rptno, msg, a1, a2) ((void)0)
#endif
#ifndef _RPT3
#define _RPT3(rptno, msg, a1, a2, a3) ((void)0)
#endif
#ifndef _RPT4
#define _RPT4(rptno, msg, a1, a2, a3, a4) ((void)0)
#endif

#if !defined(_WIN32)
#include <strings.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef sprintf_s
#define sprintf_s(buf, fmt, ...) snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__)
#endif
#endif

// Flip flags for tile and sprite rendering
enum FlipFlags
{
    Flip_None       = 0,
    Flip_Horizontal = 1,
    Flip_Vertical   = 2
};

// Endianness conversion helpers
inline uint16_t Swap16( uint16_t val )
{
    return (uint16_t)( (val << 8) | (val >> 8) );
}

inline uint32_t Swap32( uint32_t val )
{
    return ( (val << 24) & 0xFF000000 ) |
           ( (val <<  8) & 0x00FF0000 ) |
           ( (val >>  8) & 0x0000FF00 ) |
           ( (val >> 24) & 0x000000FF );
}

#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || defined(GEKKO) || defined(__BIG_ENDIAN__)
inline uint16_t FromLE16( uint16_t val ) { return Swap16( val ); }
inline int16_t  FromLE16( int16_t val )  { return (int16_t)Swap16( (uint16_t)val ); }
inline uint32_t FromLE32( uint32_t val ) { return Swap32( val ); }
inline int32_t  FromLE32( int32_t val )  { return (int32_t)Swap32( (uint32_t)val ); }

inline uint16_t ToLE16( uint16_t val )   { return Swap16( val ); }
inline int16_t  ToLE16( int16_t val )    { return (int16_t)Swap16( (uint16_t)val ); }
inline uint32_t ToLE32( uint32_t val )   { return Swap32( val ); }
inline int32_t  ToLE32( int32_t val )    { return (int32_t)Swap32( (uint32_t)val ); }
#else
inline uint16_t FromLE16( uint16_t val ) { return val; }
inline int16_t  FromLE16( int16_t val )  { return val; }
inline uint32_t FromLE32( uint32_t val ) { return val; }
inline int32_t  FromLE32( int32_t val )  { return val; }

inline uint16_t ToLE16( uint16_t val )   { return val; }
inline int16_t  ToLE16( int16_t val )    { return val; }
inline uint32_t ToLE32( uint32_t val )   { return val; }
inline int32_t  ToLE32( int32_t val )    { return val; }
#endif

// Portable file opening helper
inline int PortableFOpen( FILE** pFile, const char* filename, const char* mode )
{
    *pFile = fopen( filename, mode );
#if defined(GEKKO)
    if ( *pFile == nullptr && filename != nullptr && filename[0] != '/' && strncmp( filename, "sd:/", 4 ) != 0 )
    {
        char altPath[256];
        snprintf( altPath, sizeof( altPath ), "sd:/apps/zelda1hd/%s", filename );
        *pFile = fopen( altPath, mode );
        if ( *pFile == nullptr )
        {
            snprintf( altPath, sizeof( altPath ), "sd:/zelda1hd/%s", filename );
            *pFile = fopen( altPath, mode );
        }
    }
#endif
    return (*pFile != nullptr) ? 0 : -1;
}
