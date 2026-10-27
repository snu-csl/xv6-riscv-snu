typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;


#ifdef SNU
#define PROT_READ       0x0001
#define PROT_WRITE      0x0002

#define MAP_PRIVATE     0x0010
#define MAP_SHARED      0x0020
#define MAP_HUGEPAGE    0x0100

#define KC_FREEMEM      0
#define KC_USED4K       1
#define KC_USED2M       2
#define KC_PF           3

#define KT_KALLOC       0
#define KT_KFREE        1
#define KT_KALLOC_HUGE  2
#define KT_KFREE_HUGE   3
#endif
