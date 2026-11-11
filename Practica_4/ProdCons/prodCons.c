#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("prodCons | Practica 4 Parte B");
MODULE_AUTHOR("Daniel Martin del Castillo y Daniel Manjon Caballero");


int init_module(void);
void cleanup_module(void);
static int prodCons_open(struct inode *, struct file *);
static int prodCons_release(struct inode *, struct file *);
static ssize_t prodCons_read(struct file *, char *, size_t, loff_t *);
static ssize_t prodCons_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);

#define DEVICE_NAME "prodCons"  
#define CLASS_NAME "practica"
#define MAX_CHARS_INPUT 50

struct kfifo buf_circ; // buffer circular

DEFINE_SEMAPHORE(mtx)
struct semaphore huecos;
struct semaphore elementos;

static struct file_operations fops = {
    .read = prodCons_read,
    .write = prodCons_write,
    .open = prodCons_open,
    .release = prodCons_release,
};

int param_capacity=4; // numero de enteros
module_param(param_capacity, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(stime_ms, "Capacity of the Kfifo list");

static struct miscdevice misc_prodCons = {
    .minor = MISC_DYNAMIC_MINOR,    /* kernel dynamically assigns a free minor# */
    .name = DEVICE_NAME, /* when misc_register() is invoked, the kernel
                        * will auto-create device file as /dev/chardev ;
                        * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
    .mode = 0666,     /* ... dev node perms set as specified here */
    .fops = &fops,    /* connect to this driver's 'functionality' */
};


int init_module(void)
{
    int major;
    int minor;     
    int ret;
    struct device *device;

    ret = kfifo_alloc(&buf_circ,(param_capacity * sizeof(int)),GFP_KERNEL);

    if(ret){
        printk(KERN_INFO "Error allocating memory for Kfifo\n");
        return ret;
    }

    sema_init(&huecos,param_capacity);
    sema_init(&elementos,param_capacity);

    ret = misc_register(&misc_prodCons);

    if (ret) {
        pr_err("Couldn't register misc device\n");
        return ret;
    }

    device = misc_prodCons.this_device;

    /* Access devt field in device structure to retrieve (major,minor) */
    major = MAJOR(device->devt);
    minor = MINOR(device->devt);

    dev_info(device, "I was assigned major number %d. To talk to\n", major);
    dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
    dev_info(device, "Remove the module when done.\n");

    return 0;
}


void cleanup_module(void)
{
    misc_deregister(&misc_prodCons);
    pr_info("prodCons misc driver deregistered. Bye\n");

    kfifo_free(&buf_circ);
}


static int prodCons_open(struct inode *inode, struct file *file)
{
    // No hace nada
    return 0;
}

static int prodCons_release(struct inode *inode, struct file *file)
{
    // No hace nada
    return 0;
}

static ssize_t prodCons_write(struct file *filp,
                            const char __user *buf,
                            size_t len,
                            loff_t *off)
{
    if((*off) > 0)return 0;

    char kbuf[MAX_CHARS_INPUT+1];
    int val=0;
    
    copy_form_user(&kbuf,buf,len);

    if(sscanf(kbuf,"%i",&val) == 1){

        if(down_interrumpible(&huecos)){
            return -EINTR; // ha sido interrumpido
        }

        if(down_interrumpible(&mtx)){ // comienzo region mtx
            up(&huecos);
            return -EINTR; // ha sido interrumpido y debe liberar el hueco, porque no ha hecho nada
        }

        kfifo_in(&buf_circ,&val,sizeof(int));

        up(&mtx); // fin region mtx
        up(&elementos);
    }
    else {
        printk(KERN_INFO "prodCons: Operation not permitted: %s\n",kbuf);
        return -EPERM;
    }

    return len;
}

static ssize_t prodCons_read(struct file *filp,   
                           char *buffer,    
                           size_t len,   
                           loff_t * off)
{  
    if (*off > 0)return 0;

    int val=0;

    if(down_interrumpible(&elementos)){
        return -EINTR;
    }

    if(down_interrumpible(&mtx)){ // comienzo region mtx
        up(&elementos);
        return -EINTR;
    }

    kfifo(&buf_circ,&val,sizeof(int)); 

    up(&mtx);
    up(&huecos);

    *off+=len;
    return len;
}