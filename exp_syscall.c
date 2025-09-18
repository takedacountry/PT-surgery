#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>

#define SYS_mycall_print_user_pgtable 457
#define SYS_mycall_print_kernel_pgtable 458
#define SYS_mycall_ds_make 459
#define SYS_mycall_ds_search 460
#define SYS_mycall_recover_all_pgtable 461
#define SYS_mycall_ds_m_delete 462
#define SYS_mycall_ds_make_user 463
#define SYS_mycall_ds_make_kernel 464
#define SYS_mycall_print_kernel_pgtable2 465
#define SYS_mycall_print_user_pgtable2 466
#define SYS_mycall_m_search 467
#define SYS_mycall_register_broken_pte 468
#define SYS_mycall_recover_pgtable 469
#define SYS_mycall_m_ds_count 471
#define SYS_mycall_ds_register_pid 472
#define SYS_mycall_make_ds_usr_from_pgtable 473
#define SYS_mycall_ds_search2 474
#define SYS_mycall_m_search2 475

#define two_gb (1UL << 31) //2GB
#define four_gb (1UL << 32) //4GB
#define eight_gb (1UL << 33) //8GB
#define sixteen_gb (1UL << 34) //16GB
#define thirtytwo_gb (1UL << 35) //32GB
#define two_mb (1UL << 21) // 2MB
#define four_mb (1UL << 22) // 4MB
#define eight_mb (1UL << 23) // 8MB
#define four_kb (1UL << 12) // 4KB
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while(0) 

int main(void)
{
    char *p;
    pid_t pid;
    clock_t start_mmap, end_mmap;
    clock_t start_munmap, end_munmap;
    clock_t start_mprotect, end_mprotect;

    // printf("%ld %d\n", syscall(SYS_mycall_ds_register_pid), getpid());
    // printf("%ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));

    syscall(SYS_mycall_ds_register_pid);
    syscall(SYS_mycall_make_ds_usr_from_pgtable);

    // printf("This is 4GB code\n");
    // printf("This is 4MB code\n");
    // printf("This is 4KB code\n");
    
    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    start_mmap = clock();
    // p = mmap(NULL, thirtytwo_gb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, sixteen_gb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, eight_gb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, four_gb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, two_gb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, eight_mb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    p = mmap(NULL, four_mb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, two_mb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    // p = mmap(NULL, four_kb, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, -1, 0);
    end_mmap = clock();
    if (p == MAP_FAILED)
        handle_error("mmap error");

    // printf("va: %p\n",p);
    printf("%f ",(double)(end_mmap - start_mmap)*1000/CLOCKS_PER_SEC);

    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_ds_search));
    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    
    // pid = fork();
    // if (pid == 0) {
    //     printf("child process!\n");
    //     // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    //     exit(0);
    // }else if (pid == -1){
    //     perror("fork");
    //     exit(EXIT_FAILURE);
    // }else {
    //     int status;
    //     printf("Adult process!\n");
    //     wait(&status);
    //     if (WIFEXITED(status)) {
    //         printf("exit: %d\n", WEXITSTATUS(status));
    //     }
    // }

    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    syscall(SYS_mycall_register_broken_pte, p);

    start_mprotect = clock();
    // if (mprotect(p, thirtytwo_gb, PROT_READ) == -1)
    // if (mprotect(p, sixteen_gb, PROT_READ) == -1)
    // if (mprotect(p, eight_gb, PROT_READ) == -1)
    // if (mprotect(p, four_gb, PROT_READ) == -1)
    // if (mprotect(p, two_gb, PROT_READ) == -1)
    // if (mprotect(p, eight_mb, PROT_READ) == -1)
    if (mprotect(p, four_mb, PROT_READ) == -1)
    // if (mprotect(p, two_mb, PROT_READ) == -1)
    // if (mprotect(p, four_kb, PROT_READ) == -1)
       handle_error("mprotect error");
    end_mprotect = clock();
    printf("%f ",(double)(end_mprotect - start_mprotect)*1000/CLOCKS_PER_SEC);

    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    // printf("%ld\n", syscall(SYS_mycall_ds_search2));
    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    
    start_munmap = clock();
    // munmap(p, thirtytwo_gb);
    // munmap(p, sixteen_gb);
    // munmap(p, eight_gb);
    // munmap(p, four_gb);
    // munmap(p, two_gb);
    // munmap(p, eight_mb);
    munmap(p, four_mb);
    // munmap(p, two_mb);
    // munmap(p, four_kb);
    end_munmap = clock();
    printf("%f\n",(double)(end_munmap - start_munmap)*1000/CLOCKS_PER_SEC);


    return 0;
}
