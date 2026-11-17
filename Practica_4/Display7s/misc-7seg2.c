#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

MODULE_DESCRIPTION("Practica 4, Parte c: Modulo display7s SMP-safe y thread-safe");
MODULE_AUTHOR("Daniel Manjon Caballero y Daniel Martin del Castillo");
MODULE_LICENSE("GPL");

/* Bits associated with each segment */
#define DS_A 0x80
#define DS_B 0x40
#define DS_C 0x20
#define DS_D 0x10
#define DS_E 0x08
#define DS_F 0x04
#define DS_G 0x02
#define DS_DP 0x01
#define SEGMENT_COUNT 8

// Definimos el mutex y variable para ver si esta en uso
DEFINE_MUTEX(mutex7s);
static int en_uso = 0;   // 0 si libre y 1 si alguien lo tiene abierto
// Mutex para gestionar la funcion write
DEFINE_MUTEX(mutexW);

/* Indices of GPIOs used by this module */
enum
{
	SDI_IDX = 0,
	RCLK_IDX,
	SRCLK_IDX,
	NR_GPIO_DISPLAY
};

/* Pin numbers */
const int display_gpio[NR_GPIO_DISPLAY] = {18, 23, 24};

/* Array to hold GPIO descriptors */
struct gpio_desc *gpio_descriptors[NR_GPIO_DISPLAY];

const char *display_gpio_str[NR_GPIO_DISPLAY] = {"sdi", "rclk", "srclk"};

// El array con todos los valores validos
static const unsigned char digits[16] = {
	DS_A | DS_B | DS_C | DS_D | DS_E | DS_F, // 0
	DS_B | DS_C, // 1
	DS_A | DS_B | DS_D | DS_E | DS_G, // 2
	DS_A | DS_B | DS_C | DS_D | DS_G, // 3
	DS_B | DS_C | DS_F | DS_G, // 4
	DS_A | DS_C | DS_D | DS_F | DS_G, // 5
	DS_A | DS_C | DS_D | DS_E | DS_F | DS_G, // 6
	DS_A | DS_B | DS_C, // 7
	DS_A | DS_B | DS_C | DS_D | DS_E | DS_F | DS_G, // 8
	DS_A | DS_B | DS_C | DS_D | DS_F | DS_G, // 9
	DS_A | DS_B | DS_C | DS_E | DS_F | DS_G, // A
	DS_C | DS_D | DS_E | DS_F | DS_G, // b
	DS_A | DS_D | DS_E | DS_F, // C
	DS_B | DS_C | DS_D | DS_E | DS_G, // d
	DS_A | DS_D | DS_E | DS_F | DS_G, // E
	DS_A | DS_E | DS_F | DS_G // F
};

#define DEVICE_NAME "display7s" /* Device name */

/*
 *  Prototypes
 */
static ssize_t display7s_write(struct file *, const char *, size_t, loff_t *);
static int display7s_misc_open(struct inode *, struct file *);
static int display7s_misc_release(struct inode *, struct file *);

/* Simple initialization of file_operations interface with a single operation */
static struct file_operations fops = {
	.write = display7s_write,
	.open = display7s_misc_open,
	.release = display7s_misc_release,
};

static struct miscdevice display7s_misc = {
	.minor = MISC_DYNAMIC_MINOR, /* kernel dynamically assigns a free minor# */
	.name = DEVICE_NAME,		 /* when misc_register() is invoked, the kernel
								  * will auto-create device file;
								  * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,				 /* ... dev node perms set as specified here */
	.fops = &fops,				 /* connect to this driver's 'functionality' */
};


/* Update the 7-segment display with the configuration specified by the data parameter */
static void update_7sdisplay(unsigned char data)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < SEGMENT_COUNT; i++)
	{
		/* Explore current bit (from most significant to least significant) */
		if (0x80 & (data << i))
			value = 1;
		else
			value = 0;

		/* Set value of serial input */
		gpiod_set_value(gpio_descriptors[SDI_IDX], value);
		/* Generate clock cycle in shift register */
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 1);
		msleep(1);
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 0);
	}

	/* Generate clock cycle in output register to update 7-seg display */
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 1);
	msleep(1);
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 0);
}

static ssize_t
display7s_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char kbuf[8];
	unsigned int val;

	// Miramos que el tamanio del buffer sea correcto
	if (len == 0 || len >= sizeof(kbuf))
		return -EINVAL;
	// Lo copiamos en vex de usarlo directamente
	if (copy_from_user(kbuf, buff, len))
		return -EFAULT;
	kbuf[len] = '\0';

	if(kstrtouint(kbuf, 16, &val) != 0){
		return -EINVAL;
	}
	if(val > 15){
		return -EINVAL;
	}


	// Actualizamos (ahora con mutex)
	mutex_lock(&mutexW);
	update_7sdisplay(digits[val]);
	mutex_unlock(&mutexW);

	return len;
}

static int __init display7s_misc_init(void)
{
	int i, j;
	int err = 0;
	struct device *device;

	for (i = 0; i < NR_GPIO_DISPLAY; i++)
	{

		/* Request the GPIO */
		if ((err = gpio_request(display_gpio[i], display_gpio_str[i])))
		{
			pr_err("Failed GPIO[%d] %d request\n", i, display_gpio[i]);
			goto err_handle;
		}

		/* Transform number into descriptor */
		if (!(gpio_descriptors[i] = gpio_to_desc(display_gpio[i])))
		{
			pr_err("GPIO[%d] %d is not valid\n", i, display_gpio[i]);
			err = -EINVAL;
			goto err_handle;
		}

		/* Configure as an output pin */
		gpiod_direction_output(gpio_descriptors[i], 0);
	}

	/* Set everything as LOW */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_set_value(gpio_descriptors[i], 0);

	/* Register misc device that exposes 7-segment display */
	err = misc_register(&display7s_misc);

	if (err)
	{
		pr_err("Couldn't register misc device\n");
		goto err_handle;
	}

	device = display7s_misc.this_device;

	dev_info(device, "Display7s driver registered succesfully. To talk to\n");
	dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
	dev_info(device, "Remove the module when done.\n");

	return 0;
err_handle:
	for (j = 0; j < i; j++)
		gpiod_put(gpio_descriptors[j]);
	return err;
}

static int display7s_misc_open(struct inode *n, struct file *f){
	mutex_lock(&mutex7s);

	// Si ya esta en uso desbloqueamos el mutex
	if(en_uso == 1){
		mutex_unlock(&mutex7s);
		return -EBUSY;
	}

	// Fallo al incrementar el contador del modulo
	if (!try_module_get(THIS_MODULE)) {
		mutex_unlock(&mutex7s);
		return -EACCES;
	}

	// Lo ocupamos nosotros
	en_uso = 1;
	mutex_unlock(&mutex7s);

	return 0;
}

static int display7s_misc_release(struct inode *n, struct file *f){
	// Liberamos 
	mutex_lock(&mutex7s);

	module_put(THIS_MODULE);
	en_uso = 0;

	mutex_unlock(&mutex7s);

	return 0;
}

static void __exit display7s_misc_exit(void)
{
	int i = 0;

	/* Unregister character device */
	misc_deregister(&display7s_misc);

	/* Clear display */
	update_7sdisplay(0);

	/* Free up pins */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_put(gpio_descriptors[i]);

	pr_info("Display7s driver deregistered. Bye\n");
}

module_init(display7s_misc_init);
module_exit(display7s_misc_exit);
