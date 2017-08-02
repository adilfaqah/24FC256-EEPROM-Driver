/*
CSE 438 - Embedded Systems Programming
Project: 3
Description: This is the kernel space program for the EEPROM 'i2c_flash' driver.

Author: Adil Faqah
Date: 22nd March 2016
*/

#include <linux/module.h>  // Module Defines and Macros (THIS_MODULE)
#include <linux/kernel.h>  // 
#include <linux/fs.h>	   // Inode and File types
#include <linux/cdev.h>    // Character Device Types and functions.
#include <linux/types.h>
#include <linux/slab.h>	   // Kmalloc/Kfree
#include <asm/uaccess.h>   // Copy to/from user space
#include <linux/string.h>
#include <linux/device.h>  // Device Creation / Destruction functions
#include <linux/i2c.h>     // i2c Kernel Interfaces
#include <linux/i2c-dev.h>
#include <linux/ioctl.h>

#include <linux/gpio.h>
#include <asm/gpio.h>

#include <linux/init.h>
#include <linux/moduleparam.h> // Passing parameters to modules through insmod
#include <linux/delay.h>
#include <linux/unistd.h>

#include <linux/workqueue.h>

#include "i2c_flash_defs.h"

#define DEVICE_NAME "i2c_flash"  // device name to be created and registered
#define DEVICE_ADDR 0x54 // Device address
#define I2CMUX 29 // Clear for I2C function
#define BUSY_LED 26 //GPIO to be used to indicate 'busy' status

//Flags for different operations
int flash_mem_address = 0;
int flag_data_ready = 0;
int flag_eeprom_busy = 0;
int flag_erase = 0;

flash_pages *q_ready_data;

/* per device structure */
struct tmp_dev
{
    struct cdev cdev;               /* The cdev structure */
    // Local variables
    struct i2c_client *client;
    struct i2c_adapter *adapter;
    struct workqueue_struct *my_wq;
} *tmp_devp;

typedef struct
{
    struct work_struct my_work;
    unsigned int w_count;
    unsigned int r_count;
    int ap;
    flash_pages work_flash_pages;
} my_work_t;


static dev_t tmp_dev_number;      /* Allotted device number */
struct class *tmp_dev_class;          /* Tie with the device model */
static struct device *tmp_dev_device;

//Read function for the work queue
void setap_wq_function(struct work_struct *work)
{
    //The work queue structure
    my_work_t *my_work = (my_work_t*)work;
    flag_eeprom_busy = 1;
    flash_mem_address = my_work->ap;
    flag_eeprom_busy = 0;
    //printk("\nAP updated to: %d", flash_mem_address);
}

//Read function for the work queue
void read_wq_function(struct work_struct *work)
{
    //The work queue structure
    my_work_t *my_work = (my_work_t*)work;

    //Variables and char arrays used while reading/writing to the flash
    int ret, i, count; //j
    unsigned char read_address[2];
    unsigned char write_buffer[2];
    unsigned char read_buffer[64];

    flag_eeprom_busy = 1;
    //printk("\nPerforming read work!");

    count = my_work->r_count;

    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(read_address, 0x00, sizeof(read_address));
    memset(read_buffer, 0x00, sizeof(read_buffer));

    //printk("\nStarting address: %d", flash_mem_address);

    gpio_set_value_cansleep(BUSY_LED, 0);

    //Read page by page
    for(i = 0; i < count; i++)
    {
        //printk("\nReading from 0x%x - Page: %d.", flash_mem_address, i);
        //If the end of flash is reached, roll over
        if (flash_mem_address == 0x8000)
            flash_mem_address = 0;

        //Split the address into two bytes
        read_address[0] = (unsigned char)(flash_mem_address>>8) & 0xFF;
        read_address[1] = (unsigned char)flash_mem_address & 0xFF;

        //Copy to buffer
        memcpy(write_buffer, read_address, 2);

        //Write the address
        ret = i2c_master_send(tmp_devp->client, write_buffer, 2);
        if(ret < 0)
        {
            //printk("\nError: Could not write address to flash.\n");
            return;
        }

        udelay(5);

        //Read the data byte
        ret = i2c_master_recv(tmp_devp->client, read_buffer, 64);
        if(ret < 0)
        {
            //printk("\nError: Could not read from flash.\n");
            return;
        }

        //Store the read data to the structure
        memcpy(q_ready_data->pages[i].data, read_buffer, 64);

        flash_mem_address += 64;

        udelay(5);

    }
    //printk("\nEnding address: %d", flash_mem_address);
    gpio_set_value_cansleep(BUSY_LED, 1);

    kfree(work);



    //printk("\nFinished read work!");
    flag_data_ready = 1;
    flag_eeprom_busy = 0;
    return;
}


void write_wq_function(struct work_struct *work)
{
    //Our work structure
    my_work_t *my_work = (my_work_t*)work;

    //Variables and char arrays used while reading/writing to the flash
    int ret, i, count;
    unsigned char write_address[2];
    unsigned char write_buffer[66];
    unsigned int erase[64];

    flag_eeprom_busy = 1;
    //printk("\nPerforming write work!");

    count = my_work->w_count;

    memset(write_buffer, 0x00, sizeof(write_buffer));
    memset(write_address, 0x00, sizeof(write_address));
    memset(erase, 0xFF, sizeof(erase));

    //printk("\nNumber of filled pages: %d", count);
    //printk("\nStarting address: %d", flash_mem_address);

    gpio_set_value_cansleep(BUSY_LED, 0);

    //Write page by page
    for(i = 0; i < count; i++)
    {
        //If the end of the flash has been reached, roll over
        if (flash_mem_address == 0x8000)
            flash_mem_address = 0;

        //Separate the address into two bytes
        write_address[0] = (unsigned char)(flash_mem_address>>8) & 0xFF;
        write_address[1] = (unsigned char)flash_mem_address & 0xFF;

        memcpy(write_buffer, write_address, 2);

        //If an erase is in progess, write 0xFF else write the correct data...
        if (flag_erase == 1)
            memcpy(write_buffer+2, erase, 64);
        else
            memcpy(write_buffer+2, my_work->work_flash_pages.pages[i].data, 64);

        //printk("\n Page: %d, My Page: %d Writing to address: 0x%x - %d.", flash_mem_address/64, i, flash_mem_address, flash_mem_address);

        //Write the address along with the data
        ret = i2c_master_send(tmp_devp->client, write_buffer, 66);
        if(ret < 0)
        {
            printk("\nError: Could not write to flash.\n");
            flag_eeprom_busy = 0;
            return;
        }

        flash_mem_address += 64;
        mdelay(5);
    }
    //printk("\nEnding address: %d", flash_mem_address);
    gpio_set_value_cansleep(BUSY_LED, 1);

    kfree(work);

    if (flag_erase == 1)
    {
        flag_erase = 0;
        flash_mem_address = 0;
    }
    //printk("\nFinished write work!");
    flag_eeprom_busy = 0;
    return;
}

/*
* Open driver
*/
int tmp_driver_open(struct inode *inode, struct file *file)
{
    struct tmp_dev *tmp_devp;

    /* Get the per-device structure that contains this cdev */
    tmp_devp = container_of(inode->i_cdev, struct tmp_dev, cdev);

    /* Easy access to tmp_devp from rest of the entry points */
    file->private_data = tmp_devp;

    return 0;
}

/*
 * Release driver
 */
int tmp_driver_release(struct inode *inode, struct file *file)
{
    //struct tmp_dev *tmp_devp = file->private_data;

    return 0;
}

/*
 * Write to driver
 */
ssize_t tmp_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    int ret;
    struct tmp_dev *tmp_devp = file->private_data;
    my_work_t *work;

    //If a write request is received, copy the data from the user and add it to the work queue
    work = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);
    INIT_WORK((struct work_struct*)work, write_wq_function);
    ret = copy_from_user(work->work_flash_pages.pages, buf, count*64);
    work->w_count = count;
    ret = queue_work(tmp_devp->my_wq, (struct work_struct *)work);

    //If the flash is currently busy, let the user know.
    if (flag_eeprom_busy == 1)
        return -(EBUSY);
    else
        return 0;
}
/*
 * Read from driver
 */
ssize_t tmp_driver_read(struct file *file, char *ubuf, size_t count, loff_t *ppos)
{
    int ret;
    struct tmp_dev *tmp_devp = file->private_data;
    my_work_t *work;

    //If the flash is not busy and there is no data ready, accept the read request and add it to the work queue
    if (flag_eeprom_busy == 0 && flag_data_ready == 0)
    {
        work = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);
        INIT_WORK((struct work_struct*)work, read_wq_function);
        work->r_count = count;
        ret = queue_work(tmp_devp->my_wq, (struct work_struct *)work);

        return -(EAGAIN);
    }

    //If the flash is busy and there is no data ready, let the user know.
    if (flag_eeprom_busy == 1 && flag_data_ready == 0)
        return -(EBUSY);

    //If the data is ready, copy to user
    if (flag_data_ready == 1)
    {
        ret = copy_to_user(ubuf, q_ready_data, count*64);
        if (ret != 0)
            return -1;
        //printk("\n Unable to copy succesfully to userspace. %d bytes not copied", ret);
        flag_data_ready = 0;
        return 0;
    }
    return -1;
}

long tmp_driver_ioctl(struct file *file, unsigned int request, unsigned long argp)
{
    my_work_t *work;

    if (request == FLASHSETP)
    {
        //printk("\nRequest to set address pointer to: %lu", argp);
        work = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);
        INIT_WORK((struct work_struct*)work, setap_wq_function);
        work->ap = argp;
        queue_work(tmp_devp->my_wq, (struct work_struct *)work);
        //flash_mem_address = argp;
        return 0;
    }

    else if (request == FLASHGETP)
    {
        //printk("\nRequest to read address pointer");
        if (flag_eeprom_busy == 0)
        {
            //printk("\nRequest ack");
            return flash_mem_address;
        }
        else
            return -(EBUSY);
    }

    else if (request == FLASHGETS)
    {
        if (flag_eeprom_busy == 0)
            return 0;
        else
            return -(EBUSY);
    }

    else if (request == FLASHERASE)
    {
        //If the flash is currently busy, let the user know
        if (flag_eeprom_busy == 1)
            return -(EBUSY);
        //Else, reset the address pointer, raise the erase flag and add the write (erase) request to the work queue
        flash_mem_address = 0;
        work = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);
        INIT_WORK((struct work_struct*)work, write_wq_function);
        flag_erase = 1;
        work->w_count = 512;
        queue_work(tmp_devp->my_wq, (struct work_struct *)work);
        return 0;
    }

    return -1;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations tmp_fops =
{
    .owner		= THIS_MODULE,           /* Owner */

    .open		= tmp_driver_open,        /* Open method */
    .release  	= tmp_driver_release,     /* Release method */
    .write		= tmp_driver_write,       /* Write method */
    .read		= tmp_driver_read,        /* Read method */
    .unlocked_ioctl = tmp_driver_ioctl

};

/*
 * Driver Initialization
 */
int __init tmp_driver_init(void)
{
    int ret;

    /* Request dynamic allocation of a device major number */
    if (alloc_chrdev_region(&tmp_dev_number, 0, 1, DEVICE_NAME) < 0)
    {
        printk(KERN_DEBUG "Can't register device\n");
        return -1;
    }

    /* Populate sysfs entries */
    tmp_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

    /* Allocate memory for the per-device structure */
    tmp_devp = kmalloc(sizeof(struct tmp_dev), GFP_KERNEL);

    if (!tmp_devp)
    {
        printk("Bad Kmalloc\n");
        return -ENOMEM;
    }

    /* Request I/O region */

    /* Connect the file operations with the cdev */
    cdev_init(&tmp_devp->cdev, &tmp_fops);
    tmp_devp->cdev.owner = THIS_MODULE;

    /* Connect the major/minor number to the cdev */
    ret = cdev_add(&tmp_devp->cdev, (tmp_dev_number), 1);

    if (ret)
    {
        printk("Bad cdev\n");
        return ret;
    }

    /* Send uevents to udev, so it'll create /dev nodes */
    tmp_dev_device = device_create(tmp_dev_class, NULL, MKDEV(MAJOR(tmp_dev_number), 0), NULL, DEVICE_NAME);

    tmp_devp->my_wq = create_workqueue("my_queue");

    q_ready_data = (flash_pages*)kmalloc(sizeof(flash_pages), GFP_KERNEL);

    ret = gpio_request(I2CMUX, "I2CMUX");
    if(ret)
    {
        printk("GPIO %d is not requested.\n", I2CMUX);
    }

    ret = gpio_direction_output(I2CMUX, 0);
    if(ret)
    {
        printk("GPIO %d is not set as output.\n", I2CMUX);
    }
    gpio_set_value_cansleep(I2CMUX, 0); // Direction output didn't seem to init correctly.

    // Create Adapter using:
    tmp_devp->adapter = i2c_get_adapter(0); // /dev/i2c-0
    if(tmp_devp->adapter == NULL)
    {
        printk("Could not acquire i2c adapter.\n");
        return -1;
    }

    // Create Client Structure
    tmp_devp->client = (struct i2c_client*) kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
    tmp_devp->client->addr = DEVICE_ADDR; // Device Address (set by hardware)
    snprintf(tmp_devp->client->name, I2C_NAME_SIZE, "i2c_tmp102");
    tmp_devp->client->adapter = tmp_devp->adapter;

    // Setup GPIOs
    ret = gpio_request(BUSY_LED, "gpio_LED");
    if(ret < 0)
    {
        printk("Error Requesting LED Pin.\n");
        return -1;
    }

    ret = gpio_direction_output(BUSY_LED, 0);
    if(ret < 0)
    {
        printk("Error Setting LED Pin Output.\n");
    }
    //printk("i2c_flash succesfully initialized!");
    return 0;
}
/* Driver Exit */
void __exit tmp_driver_exit(void)
{
    //Flush and destroy the work queue on driver exit
    flush_workqueue(tmp_devp->my_wq);
    destroy_workqueue(tmp_devp->my_wq);

    // Close and cleanup
    i2c_put_adapter(tmp_devp->adapter);
    kfree(tmp_devp->client);

    /* Release the major number */
    unregister_chrdev_region((tmp_dev_number), 1);

    /* Destroy device */
    device_destroy (tmp_dev_class, MKDEV(MAJOR(tmp_dev_number), 0));
    cdev_del(&tmp_devp->cdev);
    kfree(tmp_devp);

    /* Destroy driver_class */
    class_destroy(tmp_dev_class);

    //Free the GPIOs on driver exit
    gpio_free(I2CMUX);
    gpio_free(BUSY_LED);
}

module_init(tmp_driver_init);
module_exit(tmp_driver_exit);
MODULE_LICENSE("GPL v2");
