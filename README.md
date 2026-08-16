# PT-surgery

PT-surgery is a Linux kernel mechanism for recovering from ECC-uncorrectable memory errors in page table pages. It reconstructs a damaged page table page from software-maintained metadata and replaces the damaged page table page, enabling the affected process to continue execution without terminating.

This repository contains the source code and experimental components of PT-surgery, which is based on Linux-6.1.35.

## How to Build

PT-surgery can be built using the standard Linux kernel build process.

```bash
make oldconfig
make -j$(nproc)
make modules -j$(nproc)
sudo make modules_install
sudo make install
sudo reboot
```

## How to Run with PT-surgery

PT-surgery can be applied to individual user processes.

A process can be registered with PT-surgery by invoking the `pt_surgery_register_pid` system call. The system call automatically registers the calling process as a target of PT-surgery.

### Using LD_PRELOAD

PT-surgery provides a pre-built `main-hook.so` that automatically invokes `pt_surgery_register_pid` before the application's `main()` function is executed.

To run an application with PT-surgery, set `LD_PRELOAD` to the path of
`main-hook.so`:

```bash
LD_PRELOAD=/path/to/PT-surgery/ld-preload/main-hook.so <application>
```

A successful registration produces output similar to:

```text
pt_surgery_register_pid: ret=0, pid=12345
```

## Page Table Fault Injection

PT-surgery provides a system call to emulate a page table fault injection. The system call takes a user-space virtual address as an argument and treats the page table page containing the corresponding PTE as damaged. PT-surgery then reconstructs and replaces the damaged page table page.

The system call is defined as follows:

```c
SYSCALL_DEFINE1(pt_surgery_handle_damaged_pte, unsigned long, uvaddr)
```

The system call number on x86-64 is **481**:

```text
481    common    pt_surgery_handle_damaged_pte    sys_pt_surgery_handle_damaged_pte
```

### Usage

The system call can be invoked from a user-space program using the `syscall()` interface:

```c
#include <unistd.h>
#include <sys/syscall.h>

#define SYSNUM_PT_SURGERY_HANDLE_DAMAGED_PTE 481

unsigned long uvaddr = /* user-space virtual address */;
long ret = syscall(SYSNUM_PT_SURGERY_HANDLE_DAMAGED_PTE, uvaddr);
```

