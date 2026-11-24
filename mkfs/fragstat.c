//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2026)
//
//  Project #5: deFAT File System
//
//  December 20, 2026
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

// fragstat -- report the fragmentation of a deFAT file system image.
//
//   fragstat [-v] img            F, G and a summary (-v: per-file layout)
//   fragstat -c img              consistency check only
//   fragstat -f name img         one file's blocks as consecutive runs, with
//                                that file's own F and A (A is the number of
//                                links that are not consecutive)
//   fragstat -m old.img new.img  M, the number of data blocks relocated

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

#define BSIZE         1024        // block size
#define FSMAGIC_DEFAT 0x44464154  // 'D', 'F', 'A', 'T'
#define DIRSIZ        14
#define ROOTINO       1           // root i-number
#define T_DIR         1           // directory (struct dinode's type field)

struct xv6_dirent {
  ushort inum;
  char name[DIRSIZ];
};

#define FAT_EOC  0      // end of the chain
#define FAT_RSVD 0xFFFF // a reserved block: boot, super, log, FAT or inode

struct superblock {
  uint magic;      // Must be FSMAGIC_DEFAT
  uint size;       // Size of file system image (blocks)
  uint nblocks;    // Number of data blocks
  uint ninodes;    // Number of inodes
  uint nlog;       // Number of log blocks
  uint logstart;   // Block number of first log block
  uint inodestart; // Block number of first inode block
  uint nfat;       // Number of FAT blocks
  uint fatstart;   // Block number of first FAT block
  uint freehead;   // Head of the free block list
  uint freeblks;   // Number of free data blocks
};

struct dinode {
  short type;     // File type
  short major;    // Major device number (T_DEVICE only)
  short minor;    // Minor device number (T_DEVICE only)
  short nlink;    // Number of links to inode in file system
  uint size;      // Size of file (bytes)
  uint startblk;  // First block number
};

#define IPB           (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)
#define FPB           (BSIZE / sizeof(ushort))
#define NIBLOCKS(sb)  ((sb).ninodes / IPB + 1)

static struct superblock sb;
static uint *fat;    // the whole File Allocation Table
static uchar *owner; // 0 = unvisited, 1 = in a file, 2 = on the free list
static int nblk;     // sb.size
static int ds;       // first data block
static int fd;

static int nerr;

static void
err(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "fragstat: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  nerr++;
}

static void
rsect(int f, uint sec, void *buf)
{
  if (lseek(f, (off_t)sec * BSIZE, SEEK_SET) != (off_t)sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (read(f, buf, BSIZE) != BSIZE) {
    perror("read");
    exit(1);
  }
}

static void
loadimg(const char *path, int *fdp, struct superblock *sbp, uint **fatp)
{
  char buf[BSIZE];
  int f, b, i, n;

  if ((f = open(path, O_RDONLY)) < 0) {
    perror(path);
    exit(1);
  }
  rsect(f, 1, buf);
  memmove(sbp, buf, sizeof(*sbp));
  if (sbp->magic != FSMAGIC_DEFAT) {
    fprintf(stderr, "fragstat: %s: bad magic 0x%x (expected 0x%x)\n", path,
            sbp->magic, FSMAGIC_DEFAT);
    exit(1);
  }
  n = sbp->size;
  *fatp = malloc(n * sizeof(uint));
  if (*fatp == 0) {
    fprintf(stderr, "fragstat: out of memory\n");
    exit(1);
  }
  for (b = 0; b < (int)sbp->nfat; b++) {
    rsect(f, sbp->fatstart + b, buf);
    for (i = 0; i < (int)FPB; i++) {
      int e = b * (int)FPB + i;
      if (e < n)
        (*fatp)[e] = ((ushort *)buf)[i];
    }
  }
  *fdp = f;
}

// Walk one chain, marking the blocks it visits.  Returns the length, or -1
// if the chain is damaged.  Fills runs[] with the block numbers if non-null.
static int
walk(uint start, int mark, uint *out, int maxout, const char *what)
{
  uint b = start;
  int n = 0;

  while (b != (uint)FAT_EOC) {
    if (b >= (uint)nblk) {
      err("%s: block %u is outside the file system", what, b);
      return -1;
    }
    if ((int)b < ds) {
      err("%s: block %u is not a data block", what, b);
      return -1;
    }
    if (owner[b]) {
      err("%s: block %u is already claimed by %s", what, b,
          owner[b] == 1 ? "a file" : "the free list");
      return -1;
    }
    owner[b] = mark;
    if (out && n < maxout)
      out[n] = b;
    n++;
    if (n > nblk) {
      err("%s: chain does not terminate", what);
      return -1;
    }
    if (fat[b] == FAT_RSVD) {
      err("%s: block %u points at a reserved entry", what, b);
      return -1;
    }
    b = fat[b];
  }
  return n;
}

// Read inode inum into *out.  Returns 0 on success, -1 if inum is out of
// range.
static int
readinode(uint inum, struct dinode *out)
{
  char buf[BSIZE];

  if (inum == 0 || inum >= sb.ninodes)
    return -1;
  rsect(fd, IBLOCK(inum, sb), buf);
  *out = *((struct dinode *)buf + inum % IPB);
  return 0;
}

// Collect the chain of inode *dip into out[], up to maxout blocks.  Returns
// the length, or -1 if the chain is damaged.
static int
chainof(const struct dinode *dip, uint *out, int maxout)
{
  uint b = dip->startblk;
  int n = 0;

  while (b != (uint)FAT_EOC) {
    if (b >= (uint)nblk || (int)b < ds || fat[b] == FAT_RSVD)
      return -1;
    if (n < maxout)
      out[n] = b;
    if (++n > nblk)
      return -1;
    b = fat[b];
  }
  return n;
}

// Look up one component in the directory inode *dip.  Returns its i-number,
// or 0 if there is no such entry.
static uint
lookupin(const struct dinode *dip, const char *name, int len)
{
  char buf[BSIZE];
  uint b;
  int i, nb = 0;
  struct xv6_dirent *de;

  if (len > DIRSIZ)
    return 0;
  b = dip->startblk;
  while (b != (uint)FAT_EOC) {
    if (b >= (uint)nblk || (int)b < ds || fat[b] == FAT_RSVD)
      return 0;
    rsect(fd, b, buf);
    for (i = 0; i < (int)(BSIZE / sizeof(struct xv6_dirent)); i++) {
      de = (struct xv6_dirent *)buf + i;
      if (de->inum == 0)
        continue;
      if (strncmp(de->name, name, len) == 0 &&
          (len == DIRSIZ || de->name[len] == '\0'))
        return de->inum;
    }
    if (++nb > nblk)
      return 0;
    b = fat[b];
  }
  return 0;
}

// Resolve a '/'-separated path from the root directory.  Returns the
// i-number, or 0 if any component is missing.
static uint
resolve(const char *path, struct dinode *out)
{
  uint inum = ROOTINO;
  const char *p = path;

  if (readinode(inum, out) < 0)
    return 0;
  while (*p) {
    const char *e;
    while (*p == '/')
      p++;
    if (!*p)
      break;
    e = p;
    while (*e && *e != '/')
      e++;
    if (out->type != T_DIR)
      return 0;
    inum = lookupin(out, p, (int)(e - p));
    if (inum == 0 || readinode(inum, out) < 0)
      return 0;
    p = e;
  }
  return inum;
}

// -f: print one file's layout as a list of consecutive runs, with the
// fragmentation of that file alone.  A is the number of places where the
// chain is not consecutive -- the numerator of F for this file.
static int
showfile(const char *name)
{
  struct dinode din;
  uint *chain;
  int n, i, A = 0, first = 1;

  if (resolve(name, &din) == 0) {
    fprintf(stderr, "fragstat: %s: no such file or directory\n", name);
    return 1;
  }

  chain = malloc(nblk * sizeof(uint));
  if (chain == 0) {
    fprintf(stderr, "fragstat: out of memory\n");
    return 1;
  }
  n = chainof(&din, chain, nblk);
  if (n < 0) {
    fprintf(stderr, "fragstat: %s: damaged chain\n", name);
    free(chain);
    return 1;
  }

  printf("%s:", name);
  for (i = 0; i < n;) {
    int j = i;
    while (j + 1 < n && chain[j + 1] == chain[j] + 1)
      j++;
    if (!first)
      printf(",");
    first = 0;
    if (j == i)
      printf(" %u", chain[i]);
    else
      printf(" %u-%u", chain[i], chain[j]);
    if (j + 1 < n)
      A++;
    i = j + 1;
  }
  printf(" (%d block%s)", n, n == 1 ? "" : "s");
  if (n >= 2)
    printf(", F = %.4f", (double)A / (n - 1));
  else
    printf(", F = %.4f", 0.0);
  printf(", A = %d\n", A);

  free(chain);
  return 0;
}

int
main(int argc, char *argv[])
{
  const char *path;
  char buf[BSIZE];
  int checkonly = 0, verbose = 0, cmp = 0;
  const char *fname = 0;
  int i, b, nfiles = 0, breaks = 0, links = 0;
  int nused = 0, nfree = 0, run = 0, bestrun = 0, ascending = 1;
  uint *chain;
  int argi = 1;

  while (argi < argc && argv[argi][0] == '-') {
    if (strcmp(argv[argi], "-c") == 0)
      checkonly = 1;
    else if (strcmp(argv[argi], "-v") == 0)
      verbose = 1;
    else if (strcmp(argv[argi], "-m") == 0)
      cmp = 1;
    else if (strcmp(argv[argi], "-f") == 0) {
      if (argi + 1 >= argc) {
        fprintf(stderr, "fragstat: -f needs a file name\n");
        exit(1);
      }
      fname = argv[++argi];
    } else {
      fprintf(stderr, "fragstat: unknown option %s\n", argv[argi]);
      exit(1);
    }
    argi++;
  }
  if (argi >= argc) {
    fprintf(stderr,
            "usage: fragstat [-c|-v] img\n"
            "       fragstat -f name img\n"
            "       fragstat -m old.img new.img\n");
    exit(1);
  }
  path = argv[argi];

  loadimg(path, &fd, &sb, &fat);
  nblk = sb.size;
  ds = sb.inodestart + NIBLOCKS(sb);
  owner = calloc(nblk, 1);
  chain = malloc(nblk * sizeof(uint));

  // -f: one file's layout, then stop.
  if (fname)
    exit(showfile(fname));

  // -m: count how many data blocks differ between two images.
  if (cmp) {
    int fd2, n = 0, differ = 0;
    char b1[BSIZE], b2[BSIZE];
    struct superblock sb2;
    uint *fat2;

    if (argi + 1 >= argc) {
      fprintf(stderr, "fragstat -m needs two images\n");
      exit(1);
    }
    loadimg(argv[argi + 1], &fd2, &sb2, &fat2);
    for (b = ds; b < nblk; b++) {
      rsect(fd, b, b1);
      rsect(fd2, b, b2);
      n++;
      if (memcmp(b1, b2, BSIZE) != 0)
        differ++;
    }
    printf("M %d   (of %d data blocks)\n", differ, n);
    exit(0);
  }

  // Reserved entries.
  for (b = 0; b < ds; b++)
    if (fat[b] != FAT_RSVD)
      err("metadata block %d has FAT entry 0x%x, expected 0x%x", b, fat[b],
          FAT_RSVD);

  // Every file and directory.
  for (i = 1; i < (int)sb.ninodes; i++) {
    struct dinode *dip;
    char name[32];
    int n, j, fb = 0;

    rsect(fd, IBLOCK(i, sb), buf);
    dip = (struct dinode *)buf + i % IPB;
    if (dip->type == 0)
      continue;
    nfiles++;
    if (dip->startblk == 0) {
      if (dip->size != 0)
        err("inode %d has size %u but no blocks", i, dip->size);
      continue;
    }
    snprintf(name, sizeof(name), "inode %d", i);
    n = walk(dip->startblk, 1, chain, nblk, name);
    if (n < 0)
      continue;
    nused += n;
    if ((uint)n * BSIZE < dip->size)
      err("inode %d: size %u needs more than its %d blocks", i, dip->size, n);
    for (j = 0; j + 1 < n; j++) {
      links++;
      if (chain[j + 1] != chain[j] + 1) {
        breaks++;
        fb++;
      }
    }
    if (verbose)
      printf("  inode %-3d type %d  size %-8u blocks %-5d first %-5u breaks %d\n",
             i, dip->type, dip->size, n, chain[0], fb);
  }

  // The free list.
  nfree = walk(sb.freehead, 2, chain, nblk, "free list");
  if (nfree < 0)
    nfree = 0;
  else {
    if (nfree != (int)sb.freeblks)
      err("superblock says %u free blocks, the free list has %d", sb.freeblks,
          nfree);
    ascending = 1;
    for (i = 1; i < nfree; i++)
      if (chain[i] <= chain[i - 1])
        ascending = 0;
  }

  // Partition check.
  for (b = ds; b < nblk; b++)
    if (owner[b] == 0)
      err("block %d belongs to neither a file nor the free list", b);

  // Largest contiguous run of free blocks.
  run = 0;
  for (b = ds; b < nblk; b++) {
    if (owner[b] == 2) {
      if (++run > bestrun)
        bestrun = run;
    } else
      run = 0;
  }

  if (checkonly) {
    if (nerr == 0)
      printf("%s: consistent (%d files, %d used, %d free)\n", path, nfiles,
             nused, nfree);
    exit(nerr ? 1 : 0);
  }

  printf("%s: %d files, %d data blocks (%d used, %d free)\n", path, nfiles,
         nblk - ds, nused, nfree);
  if (links == 0)
    printf("F 0.0000   (no file has two or more blocks)\n");
  else
    printf("F %.4f   (%d of %d chain links are not consecutive)\n",
           (double)breaks / links, breaks, links);
  if (nfree == 0)
    printf("G 1.0000   (no free blocks)\n");
  else
    printf("G %.4f   (largest free run %d of %d free blocks)\n",
           (double)bestrun / nfree, bestrun, nfree);
  // Compaction: are the used blocks exactly [ds, ds+nused)?
  {
    int packed = 1;
    for (b = ds; b < ds + nused; b++)
      if (owner[b] != 1)
        packed = 0;
    printf("compacted: %s   free list ascending: %s\n", packed ? "yes" : "no",
           ascending ? "yes" : "no");
  }

  if (nerr)
    fprintf(stderr, "fragstat: %d problem(s) found\n", nerr);
  exit(nerr ? 1 : 0);
}
