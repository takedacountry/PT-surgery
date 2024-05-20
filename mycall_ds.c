#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#define INDEX 1024*1024*1024 //1GB
#define PGD 512
#define PUD 512
#define PMD 512
#define PTE 512

int main(void)
{
    // printf("%ld\n", syscall(SYS_mycall_ds_delete));
    // printf("%ld\n", syscall(SYS_mycall_m_delete));
    printf("%ld %d\n", syscall(SYS_mycall_ds_register_pid, getpid()), getpid());
    printf("%ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));
    

    char *ma = (char*)malloc(INDEX);
    char *mb = (char*)malloc(INDEX);
    // char *mc = (char*)malloc(INDEX);
    // char *md = (char*)malloc(INDEX);
    // char *me = (char*)malloc(INDEX);
    // char *mf = (char*)malloc(INDEX);
    // char *mg = (char*)malloc(INDEX);
    // char *mh = (char*)malloc(INDEX);
    // char *mi = (char*)malloc(INDEX);
    // char *mj = (char*)malloc(INDEX);
    // char *mk = (char*)malloc(INDEX);
    // char *ml = (char*)malloc(INDEX);
    // char *mm = (char*)malloc(INDEX);
    // char *mn = (char*)malloc(INDEX);
    // char *mo = (char*)malloc(INDEX);
    // char *mp = (char*)malloc(INDEX);
    
    memset(ma, 0, INDEX);
    memset(mb, 0, INDEX);
    // memset(mc, 0, INDEX);
    // memset(md, 0, INDEX);
    // memset(me, 0, INDEX);
    // memset(mf, 0, INDEX);
    // memset(mg, 0, INDEX);
    // memset(mh, 0, INDEX);
    // memset(mi, 0, INDEX);
    // memset(mj, 0, INDEX);
    // memset(mk, 0, INDEX);
    // memset(ml, 0, INDEX);
    // memset(mm, 0, INDEX);
    // memset(mn, 0, INDEX);
    // memset(mo, 0, INDEX);
    // memset(mp, 0, INDEX);
    

    printf("va: %p\n", ma); // print user va 
    printf("va: %p\n", mb); // print user va 
    // printf("va: %p\n", mc); // print user va 
    // printf("va: %p\n", md); // print user va 
    // printf("va: %p\n", me); // print user va
    // printf("va: %p\n", mf); // print user va
    // printf("va: %p\n", mg); // print user va
    // printf("va: %p\n", mh); // print user va
    // printf("va: %p\n", mi); // print user va
    // printf("va: %p\n", mj); // print user va
    // printf("va: %p\n", mk); // print user va
    // printf("va: %p\n", ml); // print user va
    // printf("va: %p\n", mm); // print user va
    // printf("va: %p\n", mn); // print user va
    // printf("va: %p\n", mo); // print user va
    // printf("va: %p\n", mp); // print user va
    
    
    // printf("%ld\n", syscall(SYS_mycall_ds_init));
    printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_print_kernel_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    // printf("%ld\n", syscall(SYS_mycall_print_kernel_pgtable2));
    // printf("%ld\n", syscall(SYS_mycall_ds_make_user));
    // printf("%ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_ds_make_kernel));
    printf("%ld\n", syscall(SYS_mycall_ds_search, getpid()));
    printf("%ld\n", syscall(SYS_mycall_m_search, getpid()));
    
    printf("%ld\n", syscall(SYS_mycall_recover_all_pgtable));
    // printf("%ld\n", syscall(SYS_mycall_recover_pgtable, 0x0));
    
    printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    // printf("%ld\n", syscall(SYS_mycall_print_kernel_pgtable2));

    // success
    // for(char *t = ma; t < ma + INDEX; t++){
    //     printf("%d ", *t);
    // }
    
    // memset(ma, 1, INDEX);
    // memset(mb, 1, INDEX);
    // memset(mc, 1, INDEX);
    // memset(md, 1, INDEX);
    // memset(me, 1, INDEX);
    // memset(mf, 1, INDEX);
    // memset(mg, 1, INDEX);
    // memset(mh, 1, INDEX);
    // memset(mi, 1, INDEX);
    // memset(mj, 1, INDEX);
    // memset(mk, 1, INDEX);
    // memset(ml, 1, INDEX);
    // memset(mm, 1, INDEX);
    // memset(mn, 1, INDEX);
    // memset(mo, 1, INDEX);
    // memset(mp, 1, INDEX);

    // for(char *t = ma; t < ma + 100; t++){
    //     printf("%d ", *t);
    // }
    
    free(ma);

    printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    printf("%ld\n", syscall(SYS_mycall_ds_search2, getpid()));
    printf("%ld\n", syscall(SYS_mycall_m_search2, getpid()));
    
    free(mb);
    // free(mc);
    // free(md);
    // free(me);
    // free(mf);
    // free(mg);
    // free(mh);
    // free(mi);
    // free(mj);
    // free(mk);
    // free(ml);
    // free(mm);
    // free(mn);
    // free(mo);
    // free(mp);
    

    printf("%ld\n", syscall(SYS_mycall_ds_delete));
    printf("%ld\n", syscall(SYS_mycall_m_delete));
    // printf("%ld\n", syscall(SYS_mycall_ds_free));
    
    return 0;
}
