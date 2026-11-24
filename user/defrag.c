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

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int n = defrag();

  if (n < 0) 
  {
    fprintf(2, "defrag: failed\n");
    exit(1);
  }
  printf("defrag: %d blocks relocated\n", n);
  exit(0);
}

