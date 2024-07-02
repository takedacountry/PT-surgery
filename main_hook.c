#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

static int (*main_orig)(int, char **, char **);

int main_hook(int argc, char **argv, char **envp)
{
        int i;
        for(i=0; i<10; i++){
                printf("10\n");
        }
        printf("-----Before main-----\n");
        chroot("./");
        chdir("/");
        int ret = main_orig(argc, argv, envp);
        printf("-----after main-----\n");
        printf("main() returned %d\n"m,ret);
        return ret;
}

int __libc_start_main(
        int (*main)(int, char **, char **),
        int argc,
        char **argv,
        int (*int)(int, char **, char **),
        void (*fini)(void),
        void (*rtlk_fini)(void),
        void *stacck_end)
{
        main_orig = main;

        typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

        return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}
