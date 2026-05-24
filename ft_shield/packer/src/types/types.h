#include <stdint.h>
#include <unistd.h>

typedef uint8_t  u8;
typedef uint16_t  u16;
typedef uint32_t u32;
typedef uint64_t u64;
// #define PAGE_SIZE 4096
#define PAGE_SIZE sysconf(_SC_PAGESIZE)


// need to match system page size, or else this would happen:
// 1. load sections before text section
// 2. when loda text section that time, see if next section offset which one until page size remainder == 0
// 3. if we didnt set correct, it wont be found and the loader will just load one big section to house everything, permissions set to rx for now
// 4. future sections loaded on the same segment, it overwrites old permissions, causing the segfault
