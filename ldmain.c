#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
 
typedef int (*orig_main_f_type)();
 
int main(int argc, char **argv) {
    printf("custom main is called\n");
    orig_main_f_type orig_main;
    orig_main = (orig_main_f_type)dlsym(RTLD_NEXT, "main");
    return orig_main();
}
