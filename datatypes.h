#ifndef DATATYPES_H
#define DATATYPES_H

#include <SDL2/SDL.h>
#include <stdint.h>

#define PI32 3.14159265359f

#define global   static
#define local    static
#define internal static

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef uint8_t  uint8;
typedef uint8_t  uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float    float32;
typedef double   float64;

typedef uint32_t bool32;

#define SDL_Bool bool;

#define true  SDL_TRUE
#define false SDL_FALSE

#endif