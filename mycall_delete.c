#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define SYS_mycall_ds_m_delete 462

int main(void)
{
    printf("%ld\n", syscall(SYS_mycall_ds_m_delete));
    return 0;
}
