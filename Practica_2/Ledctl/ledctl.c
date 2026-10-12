#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Módulo análogo a la implementación de la llamada al sistema de ledctl");
MODULE_AUTHOR("Daniel Martin del Castillo y Daniel Manjon Caballero");

#define TAM 50

static struct proc_dir_entry *proc_entry;

struct tty_driver* kbd_driver;

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

// Ojo, hay que llamar a la escritura con 'sudo'
static ssize_t ledctl_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){ 
    int number;
    char kbuf[TAM];

    if (len >= TAM){
        printk(KERN_INFO "Ledctl: this number is not allowed, its too long \n");
        return -ENOSPC;
    }

    // Copiar el buffer del usuario al nuestro para evitar problemas
    if(copy_from_user(kbuf,buf,len)){
        return -EFAULT;
    }

    kbuf[len] = '\0';

    if(sscanf(kbuf,"%i\n", &number) == 1){
        if(0<= number && number <= 7){
            printk(KERN_DEBUG "El valor del num introducido es %i \n",number);

            // Código usable para el parche
            kbd_driver = get_kbd_driver_handler();

            if(kbd_driver == NULL)printk(KERN_DEBUG "El handler es null\n");

            set_leds(kbd_driver,number);
        }
        else{
            printk(KERN_INFO "Ledctl: this number is not permited\n");
            return -EPERM;
        }
    }
    else{
        printk(KERN_INFO "Ledctl: argument must be a number\n");
        return -EPERM;
    }

    (*off)+=len;

    return len;
}

static ssize_t ledctl_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    printk(KERN_INFO "Ledctl: you can't read this\n");
    return -EPERM;
}


static const struct proc_ops proc_entry_fops = {
    .proc_read = ledctl_read,
    .proc_write = ledctl_write,
};

static int __init init_ledctl_module(void)
{
    int ret = 0;

    proc_entry = proc_create("ledctl", 0666, NULL, &proc_entry_fops);
    if (proc_entry == NULL)
    {
        ret = -ENOMEM;
        printk(KERN_INFO "Ledctl: Can't create /proc entry\n");
    }
    else printk(KERN_INFO "Ledctl: Module loaded\n");

    return ret;
}

static void __exit exit_ledctl_module(void)
{
    remove_proc_entry("ledctl", NULL);

    printk(KERN_INFO "Ledctl: Module unloaded.\n");
}

module_init(init_ledctl_module);
module_exit(exit_ledctl_module);
