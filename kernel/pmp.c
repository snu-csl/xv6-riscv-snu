//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2026)
//
//  Project #2: System Calls
//
//  September 15, 2026
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

// PA2: DO NOT MODIFY THIS FILE
#ifdef SNU
#include "types.h"
#include "memlayout.h"
#include "riscv.h"

void
setpmp(void)
{
  // configure Physical Memory Protection to give supervisor mode access
  // to all of physical memory
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);
}
#endif

