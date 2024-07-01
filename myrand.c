/* myrandom.c */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
 
typedef int (*orig_rand_f_type)();
 
int rand(){
  printf("custom rand is called\n");
  orig_rand_f_type orig_rand;
  orig_rand = (orig_rand_f_type)dlsym(RTLD_NEXT, "rand");
  return orig_rand();
}
