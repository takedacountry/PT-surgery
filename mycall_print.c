#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>

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


int main(void)
{
    printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));
    printf("%ld\n", syscall(SYS_mycall_ds_search, getpid()));
    printf("%ld\n", syscall(SYS_mycall_m_search, getpid()));

    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    // printf("%ld\n", syscall(SYS_mycall_ds_search2, getpid()));
    // printf("%ld\n", syscall(SYS_mycall_m_search2, getpid()));
    
    return 0;
}
