#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_mycall_m_ds_count 471
#define SYS_mycall_ds_register_pid 472

static int (*main_orig)(int, char **, char **);

int main_hook(int argc, char **argv, char **envp)
{
        syscall(SYS_mycall_ds_register_pid);
        // syscall(SYS_mycall_m_ds_count);
        // chroot("./");
        // chdir("/");
        int ret = main_orig(argc, argv, envp);
        // syscall(SYS_mycall_m_ds_count);
        return ret;
}

int __libc_start_main(
        int (*main)(int, char **, char **),
        int argc,
        char **argv,
        int (*init)(int, char **, char **),
        void (*fini)(void),
        void (*rtld_fini)(void),
        void *stack_end)
{
        main_orig = main;

        typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

        return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}
