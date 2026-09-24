#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist");
MODULE_AUTHOR("Daniel y Daniel");

#define BUFFER_LENGTH PAGE_SIZE

static struct proc_dir_entry *proc_entry;

struct list_head mylist; /* Nodo fantasma (cabecera) de la lista enlazada */
/* Estructura que representa los nodos de la lista */
struct list_item
{
    int data;
    struct list_head links;
};

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    int available_space = BUFFER_LENGTH - 1;

    if ((*off) > 0) /* The application can write in this entry just once !! */
        return 0;

    if (len > available_space)
    {
        printk(KERN_INFO "Modlist: not enough space!!\n");
        return -ENOSPC;
    }

    int number;
    char* string;

    // Copiar el buffer del usuario al nuestro para evitar problemas
    // if(sscanf (buf, "add %i, &num") == 1)

    sscanf(buf,"%s %d",string,&number);

    if(strcmp(string,"add") == 0){
        struct list_item newItem = kmalloc(sizeof(list_item), GFP_KERNEL);
        newItem.data = number;

        list_add_tail(newItem.links,&mylist);
    }
    else if(strcmp(string,"remove") == 0){
        struct list_item* item=NULL;
        struct list_head* cur_node=NULL;

        list_for_each_safe(cur_node,&mylist){
            item = list_entry(cur_node,struct list_item,links);
            if(item.data==)
        }
    }
    else if(strcmp(string,"cleanup") == 0){

    }
    else{
        printk(KERN_INFO "Modlist: Operation not permitted\n");
        return -EPERM;
    }


    /* Transfer data from user to kernel space */
    if (copy_from_user(&clipboard[0], buf, len))
        return -EFAULT;

    clipboard[len] = '\0'; /* Add the `\0' */
    *off += len;           /* Update the file pointer */

    return len;
}

static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{

    int nr_bytes;

    if ((*off) > 0) /* Tell the application that there is nothing left to read */
        return 0;

    nr_bytes = strlen(clipboard);

    if (len < nr_bytes)
        return -ENOSPC;

    /* Transfer data from the kernel to userspace */
    if (copy_to_user(buf, clipboard, nr_bytes))
        return -EINVAL;

    (*off) += len; /* Update the file pointer */

    return nr_bytes;
}

static const struct proc_ops proc_entry_fops = {
    .proc_read = modlist_read,
    .proc_write = modlist_write,
};

int init_modlist_module(void)
{
    int ret = 0;

    // Inicializamos la lista
    INIT_LIST_HEAD(&mylist);

    // Creamos la entrada modlist
    proc_entry = proc_create("modlist", 0666, NULL, &proc_entry_fops);
    if (proc_entry == NULL)
    {
        ret = -ENOMEM;
        printk(KERN_INFO "Modlist: Can't create /proc entry\n");
    }
    else
    {
        printk(KERN_INFO "Modlist: Module loaded\n");
    }

    return ret;
}

void exit_modlist_module(void)
{
    remove_proc_entry("modlist", NULL);
    // !! Liberar memoria

    printk(KERN_INFO "Modlist: Module unloaded.\n");
}

module_init(init_modlist_module);
module_exit(exit_modlist_module);
