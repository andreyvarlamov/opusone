#ifndef OPUSONE_COMMON_H
#define  OPUSONE_COMMON_H

#include <stdint.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

typedef i32      b32;

#define global_variable static
#define local_persist static
#define internal static

#define Assert(Expression) if (!(Expression)) { *(int *) 0 = 0; }
#define InvalidCodePath Assert(!"Invalid Code Path")
#define Noop { volatile int X = 0; }

#define ArrayCount(Array) (sizeof((Array)) / (sizeof((Array)[0])))

#define Kilobytes(Value) (         (Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define Max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define Min(X, Y) (((X) < (Y)) ? (X) : (Y))

#define SIMPLE_STRING_SIZE 128
struct simple_string
{
    u32 BufferSize = SIMPLE_STRING_SIZE;
    u32 Length = 0;
    char D[SIMPLE_STRING_SIZE];
};

inline simple_string
SimpleString(const char *String)
{
    simple_string Result {};

    for (u32 StringIndex = 0;
         StringIndex < (Result.BufferSize - 1);
         ++StringIndex)
    {
        if (String[StringIndex] == '\0')
        {
            break;
        }
        
        Result.D[StringIndex] = String[StringIndex];
        Result.Length++;
    }

    Result.D[Result.Length] = '\0';

    return Result;
}

struct memory_arena
{
    size_t Size;
    u8 *Base;
    size_t Used;
};

inline memory_arena
MemoryArena(u8 *Base, size_t Size)
{
    memory_arena Arena {};
    
    Arena.Size = Size;
    Arena.Base = Base;

    return Arena;
}

inline void *
PushSize_(memory_arena *Arena, size_t Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#define PushStruct(Arena, type) (type *) PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *) PushSize_(Arena, Count * sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)

#endif
