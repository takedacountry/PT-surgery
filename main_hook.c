#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_mycall_print_user_pgtable 457
#define SYS_mycall_ds_search 460
#define SYS_mycall_recover_all_pgtable 461
#define SYS_mycall_print_user_pgtable2 466
#define SYS_mycall_m_search 467
#define SYS_mycall_register_broken_pte 468
#define SYS_mycall_recover_pgtable 469
#define SYS_mycall_m_ds_count 471
#define SYS_mycall_ds_register_pid 472
#define SYS_mycall_make_ds_usr_from_pgtable 473
#define SYS_mycall_ds_search2 474
#define SYS_mycall_m_search2 475

static int (*main_orig)(int, char **, char **);

int main_hook(int argc, char **argv, char **envp)
{
        printf("register pid %ld %d\n", syscall(SYS_mycall_ds_register_pid), getpid());
        // printf("make ds usr  %ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
        // printf("count ds m %ld\n", syscall(SYS_mycall_m_ds_count));
        printf("-----Before main-----\n");
        // chroot("./");
        // chdir("/");
        int ret = main_orig(argc, argv, envp);
        printf("-----after main-----\n");
        // printf("print pgtable %ld\n", syscall(SYS_mycall_print_user_pgtable));
        // printf("count ds m %ld\n", syscall(SYS_mycall_m_ds_count));
        printf("main() returned %d\n", ret);
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
