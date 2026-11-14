#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>

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

DEFINE_SEMAPHORE(mtx);
struct semaphore huecos;
struct semaphore elementos;

static struct file_operations fops = {
    .read = prodCons_read,
    .write = prodCons_write,
    .open = prodCons_open,
    .release = prodCons_release,
};

int capacity=4; // numero de enteros
module_param(capacity, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(capacity, "Capacity of the Kfifo list");

static struct miscdevice misc_prodCons = {
    .minor = MISC_DYNAMIC_MINOR,    /* kernel dynamically assigns a free minor# */
    .name = DEVICE_NAME, 
    .mode = 0666,   
    .fops = &fops,    
};


int init_module(void)
{
    int major;
    int minor;     
    int ret;
    struct device *device;

    if(!(capacity == 4 || capacity == 8)){
        printk(KERN_INFO "prodCons: param 'capacity' is invalid, returning to size 4...\n");
        capacity = 4;
    }

    ret = kfifo_alloc(&buf_circ,(capacity * sizeof(int)),GFP_KERNEL);

    if(ret){
        printk(KERN_INFO "Error allocating memory for Kfifo\n");
        return ret;
    }

    sema_init(&huecos,capacity);
    sema_init(&elementos,0);

    ret = misc_register(&misc_prodCons);

    if (ret) {
        pr_err("Couldn't register misc device\n");
        kfifo_free(&buf_circ);
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
    char kbuf[MAX_CHARS_INPUT+1];
    int val=0;

    if((*off) > 0)return 0;

    if(len > MAX_CHARS_INPUT){
        printk(KERN_INFO "prodCons: Too much input\n");
        return -ENOMEM;
    }
    
    if(copy_from_user(&kbuf,buf,len)){
        return -EFAULT;
    }

    kbuf[len] = '\0';

    if(sscanf(kbuf,"%i",&val) == 1){
        
        try_module_get(THIS_MODULE);

        if(down_interruptible(&huecos)){
            return -EINTR; // ha sido interrumpido
        }

        if(down_interruptible(&mtx)){ // comienzo region mtx
            up(&huecos);
            return -EINTR; // ha sido interrumpido y debe liberar el hueco, porque no ha hecho nada
        }

        if(kfifo_in(&buf_circ,&val,sizeof(int)) != sizeof(int)){
            printk(KERN_INFO "prodCons: Kfifo_in failed, tried to insert %i \n",val);
            up(&mtx);
            up(&huecos);
            return -EFAULT;
        }

        up(&mtx); // fin region mtx
        up(&elementos);

        module_put(THIS_MODULE);
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

    int val=0;

    char val_buffer[MAX_CHARS_INPUT+1];

    if((*off)>0)return 0;

    try_module_get(THIS_MODULE);

    if(down_interruptible(&elementos)){
        return -EINTR;
    }

    if(down_interruptible(&mtx)){ // comienzo region mtx
        up(&elementos);
        return -EINTR;
    }

    if(kfifo_out(&buf_circ,&val,sizeof(int)) != sizeof(int)){
        printk(KERN_INFO "prodCons: Kfifo_out failed\n");
        up(&mtx);
        up(&elementos);
        return -EFAULT;
    }

    up(&mtx);
    up(&huecos);

    snprintf(val_buffer,sizeof(val_buffer),"%i\n",val);

    if(copy_to_user(buffer,val_buffer,sizeof(val_buffer))){
        return -EFAULT;
    }

    module_put(THIS_MODULE);

    *off+=sizeof(int);
    return sizeof(int);
}