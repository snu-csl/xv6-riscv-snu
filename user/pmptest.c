//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2026)
//
//  Project #2: System calls
//
//  September 15, 2026
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPMP  4

int
main(int argc, char *argv[])
{
  void *p;
  int perm;
  
  for (int i = 0; i < NPMP; i++)
  {
    p = getpmpaddr(i);
    perm = getpmpcfg(i);

    printf("pmpaddr[%d] = %p (%s%s%s)\n", 
        i, p,
        (perm & PMP_R)? "R":"-",
        (perm & PMP_W)? "W":"-",
        (perm & PMP_X)? "X":"-");
  }
  exit(0);
}

