#include <linux/syscalls.h>
#include <linux/kernel.h>

SYSCALL_DEFINE0(lin_hello){

    printk("Hello World, and you too I guess\n");

    return 0;
}