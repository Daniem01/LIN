#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist");
MODULE_AUTHOR("Daniel Martin del Castillo y Daniel Manjon Caballero");

// Tamano del buffer del Kernel 
#define TAM 50 

static struct proc_dir_entry *proc_entry;

struct list_head mylist; /* Nodo fantasma (cabecera) de la lista enlazada */
/* Estructura que representa los nodos de la lista */
struct list_item
{
    int data;
    struct list_head links;
};

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
    int number;
    char kbuf[TAM];

    if (len >= TAM){
        printk(KERN_INFO "Modlist: not enough space for this entry!\n");
        return -ENOSPC;
    }

    // Copiar el buffer del usuario al nuestro para evitar problemas
    if(copy_from_user(kbuf,buf,len)){
        return -EFAULT;
    }

    //Anadir \0
    kbuf[len] = '\0';

    if(sscanf (kbuf, "add %i", &number) == 1){
        struct list_item* newItem = kmalloc(sizeof(struct list_item), GFP_KERNEL); 

        if(newItem == NULL)return -ENOMEM;

        newItem->data = number;

        list_add_tail(&newItem->links,&mylist);
    }
    else if(sscanf (kbuf, "remove %i", &number) == 1){
        struct list_item* item=NULL;
        struct list_head* cur_node=NULL;
        struct list_head* next_node = NULL;

        list_for_each_safe(cur_node,next_node,&mylist){
            item = list_entry(cur_node,struct list_item,links);
            if(item->data==number){
                list_del(cur_node);
                kfree(item);
            }
        }
    }
    else if(strcmp(kbuf,"cleanup\n") == 0){ // strcmp devuelve 0 si son iguales
        struct list_item* item=NULL;
        struct list_head* cur_node=NULL;
        struct list_head* next_node = NULL;

        list_for_each_safe(cur_node,next_node,&mylist){
            item = list_entry(cur_node,struct list_item,links);
                list_del(cur_node);
                kfree(item);
        }
    }
    else{
        printk(KERN_INFO "Modlist: Operation not permitted: %s\n",kbuf); // mejor para debugear
        return -EPERM;
    }

    // No hace falta mover nada, porque estamos 
    // tratando con la lista, no con "char clipboard[TAM]""

    return len;
}

/*read con la implementación de la parte obligatoria de la práctica
static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    int n_bytes = 0; // bytes "escritos"

    char kbuf[128];
    char aux_kbuf[TAM];

    struct list_item* item=NULL;
    struct list_head* cur_node=NULL;

    list_for_each(cur_node,&mylist){
        int n = 0;
        item = list_entry(cur_node,struct list_item,links);
        
        n = sprintf(aux_kbuf,"%d\n",item->data);
        if(n_bytes + n < sizeof(kbuf)){
            // escribir en kbuf:
            for(int i = 0; i < n; i++){
                kbuf[n_bytes + i] = aux_kbuf[i];
            }
            n_bytes = n_bytes + n;
        }
    }

    if(*off >= n_bytes)return 0;

    // copiar solo lo que nos solicitan
    if(len > n_bytes - *off)len = n_bytes - *off;

    if(copy_to_user(buf,kbuf + *off,len)){
        return -EFAULT;
    }

    *off += len;

    return len;
}
*/

static void * seq_start(struct seq_file *m, loff_t *pos){ // escoge el primer elemento de la lista determinado por off

    struct list_head* cur_node = NULL;

    loff_t cur_pos = 0;

    list_for_each(cur_node,&mylist){
        if(cur_pos == (*pos)){
            struct list_item *item = list_entry(cur_node,struct list_item,links);
            return item;
        }
        else cur_pos = cur_pos + 1;
    }

    return NULL; // en caso de no encontrar la posición
}

static void * seq_next(struct seq_file*f, void* v, loff_t*pos){ // siguiente elemento de la lista
    struct list_item* item = v; // cast
    
    (*pos) = (*pos) + 1; // avanzar off en 1 

    if(item->links.next == &mylist){ 
        return NULL; // esto es si el next es el nodo fantasma, aka el fin de la lista
    }

    return list_entry(item->links.next,struct list_item,links);
}

static void seq_stop(struct seq_file *m,void *v){ // en nuestro caso, no lo vamos a usar porque solo mostramos la lista entera
    /* Nothing to do */
}

static int seq_show(struct seq_file* m,void * v){ // printear el elemento
    struct list_item* item = v;

    seq_printf(m,"%d\n",item->data);

    return 0;
}

static const struct seq_operations seq_operations_modlist = { // conjunto de ops que usa la función seq_read
    .start = seq_start,
    .next = seq_next,
    .show = seq_show,
    .stop = seq_stop,
};

static int modlist_open(struct inode* inode, struct file *file){ // Asignar ops a seq_read
    return seq_open(file, &seq_operations_modlist);
}

static const struct proc_ops proc_entry_fops = {
    .proc_open = modlist_open,
    .proc_read = seq_read, // aquí es dónde remplazamos read por seq_read, que ya está implementada
    .proc_write = modlist_write,
    .proc_release = seq_release, // Para liberar la memoria reservada por seq_open
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
    // Liberar memoria:  Cleanup de los elementos
    struct list_item* item=NULL;
    struct list_head* cur_node=NULL;
    struct list_head* next_node = NULL;

    remove_proc_entry("modlist", NULL);

    list_for_each_safe(cur_node,next_node,&mylist){
        item = list_entry(cur_node,struct list_item,links);
        list_del(cur_node);
        kfree(item);
    }    

    printk(KERN_INFO "Modlist: Module unloaded.\n");
}

module_init(init_modlist_module);
module_exit(exit_modlist_module);
