//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2026)
//
//  Project #4: mmap() with Huge Pages
//
//  October 27, 2026
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
  uint64 p = (uint64) 0x100000000ULL;

  mmap((void *)p, 100, PROT_WRITE, MAP_PRIVATE);
  *(int *)p = 0xdeadbeef;
  if (fork() == 0)
  {
    if (fork() == 0)
    {
      *(int *)p = 0x0badbeef;
      printf("pid %d: %x\n", getpid(), *(int *)p);
      exit(0);
    }
    wait(0);
    printf("pid %d: %x\n", getpid(), *(int *)p);
    exit(0);
  }
  wait(0);
  printf("pid %d: %x\n", getpid(), *(int *)p);
  munmap((void *)p);
  exit(0);
  return;
}
