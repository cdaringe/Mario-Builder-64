#ifndef ULTRA64_H_STUB
#define ULTRA64_H_STUB

// Minimal stub for compilation only - host project provides actual implementation
#include <stdint.h>
#include <stddef.h>

// Always define basic types early - this ensures they're available before any main headers
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

typedef f32 Vec3f[3];
typedef s16 Vec3s[3];

typedef uint32_t Gfx;
typedef uint32_t Mtx;
typedef uint32_t Vp;
typedef uint32_t Vtx;

// Graphics lighting types
typedef struct { int dummy; } Light;
typedef struct { Light l[1]; } Lights1;
typedef struct { Light l[2]; } Lights2;
typedef struct { Light l[7]; } Lights7;

// OS types - use unique struct names to avoid conflicts
typedef struct OSThreadStub { int dummy; } OSThread;
typedef struct OSMesgQueueStub { int dummy; } OSMesgQueue;
typedef struct OSTaskStub { int dummy; } OSTask;
typedef struct OSContStatusStub { int dummy; } OSContStatus;
typedef struct OSContPadExStub { int dummy; } OSContPadEx;
typedef struct OSMesgStub { int dummy; } OSMesg;

// Controller constants
#define MAXCONTROLLERS 4

#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL ((void*)0)
#endif

#define M_PI 3.14159265359f
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#endif