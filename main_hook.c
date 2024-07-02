#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_mycall_print_user_pgtable 457
#define SYS_mycall_print_kernel_pgtable 458
#define SYS_mycall_ds_make 459
#define SYS_mycall_ds_search 460
#define SYS_mycall_recover_all_pgtable 461
#define SYS_mycall_ds_delete 462
#define SYS_mycall_ds_make_user 463
#define SYS_mycall_ds_make_kernel 464
#define SYS_mycall_print_kernel_pgtable2 465
#define SYS_mycall_print_user_pgtable2 466
#define SYS_mycall_m_search 467
#define SYS_mycall_m_delete 468
#define SYS_mycall_recover_pgtable 469
#define SYS_mycall_ds_init 470
#define SYS_mycall_ds_free 471
#define SYS_mycall_ds_register_pid 472
#define SYS_mycall_make_ds_usr_from_pgtable 473
#define SYS_mycall_ds_search2 474
#define SYS_mycall_m_search2 475

static int (*main_orig)(int, char **, char **);

int main_hook(int argc, char **argv, char **envp)
{
        printf("register pid %ld %d\n", syscall(SYS_mycall_ds_register_pid, getpid()), getpid());
        printf("make ds usr  %ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
        printf("-----Before main-----\n");
        chroot("./");
        chdir("/");
        int ret = main_orig(argc, argv, envp);
        printf("-----after main-----\n");
        printf("ds delete %ld\n", syscall(SYS_mycall_ds_delete));
        printf("m  delete %ld\n", syscall(SYS_mycall_m_delete));
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
