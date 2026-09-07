#include "../include/common.h"
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace cultus {

const SizeClass SIZE_CLASSES[NUM_CLASSES] = {
    {8,    32}, {16,  32}, {24,  32}, {32,  32},
    {40,  32}, {48,  32}, {56,  32}, {64,  32},
    {80,  16}, {96,  16}, {112, 16}, {128, 16},
    {160, 16}, {192, 16}, {224, 16}, {256, 16},
    {320,  8}, {384,  8}, {448,  8}, {512,  8},
    {640,  8}, {768,  8}, {896,  8}, {1024, 8},
    {1280, 4}, {1536, 4}, {1792, 4}, {2048, 4},
    {2560, 4}, {3072, 4}, {3584, 4}, {4096, 4},
    {5120, 2}, {6144, 2}, {7168, 2}, {8192, 2},
};

uint8_t SIZE_CLASS_LOOKUP[MAX_SMALL_SIZE + 1];

static struct LookupInit {
    LookupInit() {
        int cls = 0;
        for (size_t sz = 0; sz <= MAX_SMALL_SIZE; sz++) {
            while (cls < (int)NUM_CLASSES - 1 && SIZE_CLASSES[cls].block_size < sz)
                cls++;
            SIZE_CLASS_LOOKUP[sz] = (uint8_t)cls;
        }
    }
} g_lookup_init;

int sizeToClass(size_t sz) {
    if (sz <= MAX_SMALL_SIZE) return SIZE_CLASS_LOOKUP[sz];
    return -1;
}

void* osAllocPages(size_t bytes) {
#if defined(_WIN32)
    void* p = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p;
#else
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? nullptr : p;
#endif
}

void osFreePages(void* ptr, size_t bytes) {
#if defined(_WIN32)
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, bytes);
#endif
}

}
