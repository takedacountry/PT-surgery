#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>

#define SYS_mycall_m_ds_count 471

int main(void)
{
    printf("%ld\n", syscall(SYS_mycall_m_ds_count));
    return 0;
}
