#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>

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

#define INDEX 1024*1024*1024 //1GB
#define PAGESIZE 2*1024*1024 // 4MB
#define PGD 512
#define PUD 512
#define PMD 512
#define PTE 512

#define OFFSET_SHIFT 		(12)
#define OFFSET_SIZE			(1 << OFFSET_SHIFT)
#define OFFSET_MASK			(OFFSET_SIZE - 1)
#define OFFSET_MASK_NOT		(~OFFSET_MASK)

pthread_mutex_t mutex;

void* thread_func(void* arg)
{
    // pthread_mutex_lock(&mutex);
    // printf("%ld %d\n", syscall(SYS_mycall_ds_register_pid), getpid());
    // printf("%ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));

    // char *ma = (char*)malloc(PAGESIZE);
    // memset(ma, 0, PAGESIZE);
    // printf("va: %p\n", ma); // print user va 
    // free(ma);

    pid_t pid;
    if((pid = fork()) == -1){
        printf("fork() failed");
        return NULL;
    }else if(pid == 0){
        // child
        // memset(ma, 1, PAGESIZE);
        // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    }else{
        //parent
        int status;
        wait(&status);
        if (WIFEXITED(status)) {
            printf("exit: %d\n", WEXITSTATUS(status));
        }
        // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));
        // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    }
    
    // pthread_mutex_unlock(&mutex);
    return NULL;
}

int main(void)
{
    pid_t pid;
    // pthread_t thread1;
    // pthread_t thread2;

    printf("%ld %d\n", syscall(SYS_mycall_ds_register_pid), getpid());
    // printf("%ld\n", syscall(SYS_mycall_make_ds_usr_from_pgtable));
    
    char *ma = (char*)malloc(PAGESIZE);
    // char *mb = (char*)malloc(INDEX);
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
    // char *ma1 = (char*)malloc(INDEX);
    // char *mb1 = (char*)malloc(INDEX);
    // char *mc1 = (char*)malloc(INDEX);
    // char *md1 = (char*)malloc(INDEX);
    // char *me1 = (char*)malloc(INDEX);
    // char *mf1 = (char*)malloc(INDEX);
    // char *mg1 = (char*)malloc(INDEX);
    // char *mh1 = (char*)malloc(INDEX);
    // char *mi1 = (char*)malloc(INDEX);
    // char *mj1 = (char*)malloc(INDEX);
    // char *mk1 = (char*)malloc(INDEX);
    // char *ml1 = (char*)malloc(INDEX);
    // char *mm1 = (char*)malloc(INDEX);
    // char *mn1 = (char*)malloc(INDEX);
    // char *mo1 = (char*)malloc(INDEX);
    // char *mp1 = (char*)malloc(INDEX);
    
    memset(ma, 0, PAGESIZE);
    // memset(mb, 0, INDEX);
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
    // memset(ma1, 0, INDEX);
    // memset(mb1, 0, INDEX);
    // memset(mc1, 0, INDEX);
    // memset(md1, 0, INDEX);
    // memset(me1, 0, INDEX);
    // memset(mf1, 0, INDEX);
    // memset(mg1, 0, INDEX);
    // memset(mh1, 0, INDEX);
    // memset(mi1, 0, INDEX);
    // memset(mj1, 0, INDEX);
    // memset(mk1, 0, INDEX);
    // memset(ml1, 0, INDEX);
    // memset(mm1, 0, INDEX);
    // memset(mn1, 0, INDEX);
    // memset(mo1, 0, INDEX);
    // memset(mp1, 0, INDEX);
    
    printf("va: %p\n", ma); // print user va 
    // printf("va: %p\n", mb); // print user va 
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
    // printf("va: %p\n", ma1); // print user va 
    // printf("va: %p\n", mb1); // print user va 
    // printf("va: %p\n", mc1); // print user va 
    // printf("va: %p\n", md1); // print user va 
    // printf("va: %p\n", me1); // print user va
    // printf("va: %p\n", mf1); // print user va
    // printf("va: %p\n", mg1); // print user va
    // printf("va: %p\n", mh1); // print user va
    // printf("va: %p\n", mi1); // print user va
    // printf("va: %p\n", mj1); // print user va
    // printf("va: %p\n", mk1); // print user va
    // printf("va: %p\n", ml1); // print user va
    // printf("va: %p\n", mm1); // print user va
    // printf("va: %p\n", mn1); // print user va
    // printf("va: %p\n", mo1); // print user va
    // printf("va: %p\n", mp1); // print user va

    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    // printf("%ld\n", syscall(SYS_mycall_ds_search));
    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable));

    // printf("%ld\n", syscall(SYS_mycall_register_broken_pte, (unsigned long)ma));
    // printf("%ld\n", syscall(SYS_mycall_register_broken_pte, (unsigned long)mb));


    // pthread_mutex_init(&mutex, NULL);
    // if(pthread_create(&thread1, NULL, thread_func, NULL) != 0) {
    // 	perror("failure thread1 create");
    //     return 0;
    // }
    // if(pthread_create(&thread2, NULL, thread_func, NULL) != 0) {
    //     perror("failure thread2 create");
    //     return 0;
    // }

    // pthread_join(thread1, NULL);
    // pthread_join(thread2, NULL);

    // pthread_mutex_destroy(&mutex);

    // if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
    //     perror("signal");
    //     exit(EXIT_FAILURE);
    // }
    // pid = fork();
    // switch (pid) {
    // case -1:
    //     perror("fork");
    //     exit(EXIT_FAILURE);
    // case 0:
    //     puts("Child exiting.");
    //     exit(EXIT_SUCCESS);
    // default:
    //     printf("Child is PID %jd\n", (intmax_t) pid);
    //     puts("Parent exiting.");
        
    //     memset(ma, 1, PAGESIZE);
    //     printf("%ld\n", syscall(SYS_mycall_m_ds_count));
        // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    //     free(ma);
        
    //     exit(EXIT_SUCCESS);
    // }

    // pid = fork();
    // printf("pid: %d\n",pid);
    // if (pid == 0) {
    //     printf("child process!\n");
    //     // printf("%ld\n", syscall(SYS_mycall_ds_search2));
    //     // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    //     exit(0);
    // }else if (pid < 0){
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

    // memset(ma, 1, PAGESIZE);
    
    // printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));
    
    // for (int i=0; i < 50000; i++) {
    //     int r = rand() % 10000;
    //     if (r == 0) {
    //         printf("%ld %p\n", syscall(SYS_mycall_register_broken_pte, (unsigned long)thread_func), thread_func);
    //     }
    // }

    free(ma);
    // free(mb);
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
    // free(ma1);
    // free(mb1);
    // free(mc1);
    // free(md1);
    // free(me1);
    // free(mf1);
    // free(mg1);
    // free(mh1);
    // free(mi1);
    // free(mj1);
    // free(mk1);
    // free(ml1);
    // free(mm1);
    // free(mn1);
    // free(mo1);
    // free(mp1);

    // printf("%ld\n", syscall(SYS_mycall_ds_m_delete));
    // printf("%ld\n", syscall(SYS_mycall_print_user_pgtable2));

    return 0;
}
