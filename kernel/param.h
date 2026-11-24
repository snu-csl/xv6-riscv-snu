#define NPROC       64                // maximum number of processes
#define NCPU        8                 // maximum number of CPUs
#define NOFILE      16                // open files per process
#define NFILE       100               // open files per system
#define NINODE      50                // maximum number of active i-nodes
#define NDEV        10                // maximum major device number
#define ROOTDEV     1                 // device number of file system root disk
#define MAXARG      32                // max exec arguments
#ifdef SNU
#define MAXOPBLOCKS 14                // max # of blocks any FS op writes in deFAT
#else
#define MAXOPBLOCKS 10                // max # of blocks any FS op writes
#endif
#define LOGBLOCKS   (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#ifdef SNU
#define NBUF        (MAXOPBLOCKS * 4) // size of disk block cache
#else
#define NBUF        (MAXOPBLOCKS * 3) // size of disk block cache
#endif
#define FSSIZE      2000              // size of file system in blocks
#define MAXPATH     128               // maximum file path name
#define USERSTACK   1                 // user stack pages
#ifdef SNU
#define FSSIZE_MAX  2048              // FSSIZE <= FSSIZE_MAX
#define FATPB       (BSIZE/sizeof(ushort))  // FAT entries per block
#define NFATMAX     ((FSSIZE_MAX + FATPB - 1) / FATPB)  // Max number of FAT blocks
#endif
