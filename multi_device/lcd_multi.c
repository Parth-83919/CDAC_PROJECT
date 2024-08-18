#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/gpio.h>  // linux gpio interface
#include <linux/delay.h> // delay
#include <linux/string.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include "bbb_lcd.h"
#include "bbb_ioctl.h"

static struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .open = lcd_open,
    .release = lcd_close,
    .read = lcd_read,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ioctl
};

struct lcd
{
    dev_t lcd_devno;
    struct cdev cdev;
    struct kfifo dev_buf;
    struct mutex lock;
};

static int lcd_pin[] = {
    LCD_RS,
    LCD_EN,
    LCD_D4,
    LCD_D5,
    LCD_D6,
    LCD_D7
};

static struct class *pclass;
static int major;

static struct lcd *dev;
static int dev_cnt = 1;
module_param(dev_cnt, int, 0100);

static __init int lcd_init(void)
{
    int i, ret, minor;
    struct device *pdevice;
    dev_t devno;

    printk(KERN_INFO "%s : lcd_init() is called\n", THIS_MODULE->name);

    // depending upon the device count(dev_cnt) allocting the memory for device(s)
    dev = (struct lcd *)kmalloc(dev_cnt * sizeof(struct lcd), GFP_KERNEL);
    if (dev == NULL)
    {
        ret = -ENOMEM;
        printk(KERN_INFO "%s : kmalloc is failed\n", THIS_MODULE->name);
        goto dev_kmalloc_failed;
    }
    printk(KERN_INFO "%s : kamlloc is success\n", THIS_MODULE->name);

    //for each device alloctig the kfifo memory 
    for (i = 0; i < dev_cnt; i++)
    {
        ret = kfifo_alloc(&dev[i].dev_buf, FIFO_SIZE, GFP_KERNEL);
        if (ret != 0)
        {
            printk(KERN_INFO "%s : kfifo_alloc is failed for %d lcd device\n", THIS_MODULE->name, i);
            goto kfifo_alloc_failed;
        }
    }
    printk(KERN_INFO "%s : kfifo_alloc is success\n", THIS_MODULE->name);

    // allocating character device number to the device driver
    ret = alloc_chrdev_region(&devno, 0, dev_cnt, "bbb_lcd");
    if (ret < 0)
    {
        printk(KERN_INFO "%s : alloc_chrdev_region() failed\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    major = MAJOR(devno);
    minor = MINOR(devno);
    printk(KERN_INFO "%s : alloc_chrdev_region() is success. devno: %d/%d\n", THIS_MODULE->name, major, minor);

    // creating the class for device(s)
    pclass = class_create(THIS_MODULE, "bbb_lcd");
    if (IS_ERR(pclass))
    {
        printk(KERN_INFO "%s : class_create() failed\n", THIS_MODULE->name);
        goto class_create_failed;
    }
    printk(KERN_INFO "%s : class_create() is success. \n", THIS_MODULE->name);

    // creating the multiple devices for lcd in sysfs
    for (i = 0; i < dev_cnt; i++)
    {
        dev[i].lcd_devno = MKDEV(major, i);
        pdevice = device_create(pclass, NULL, dev[i].lcd_devno, NULL, "bbb_lcd%d", i);
        if (IS_ERR(pdevice))
        {
            printk(KERN_INFO "%s : device_create() no.%d is failed\n", THIS_MODULE->name, i);
            ret = -1;
            goto device_create_failed;
        }
        printk(KERN_INFO "%s : device_create() no.%d is success.\n", THIS_MODULE->name, i);
    }

    // filling cdev structure of particular devices with their file_operation and then adding into global cdev_map
    for (i = 0; i < dev_cnt; i++)
    {
        cdev_init(&dev[i].cdev, &f_ops);
        ret = cdev_add(&dev[i].cdev, dev[i].lcd_devno, 1);
        if (ret != 0)
        {
            printk(KERN_INFO "%s : cdev_add() failed\n", THIS_MODULE->name);
            goto cdev_add_failed;
        }
        printk(KERN_INFO "%s : cdev_add() is success. \n", THIS_MODULE->name);
    }

    // allocting the mutex for each device(s) 
    for(i=0; i<dev_cnt; i++)
        mutex_init(&dev[i].lock);
    printk(KERN_INFO "%s: mutex_init() initialized mutex lock for all devices.\n", THIS_MODULE->name);

    // initializing the BBB pin
    ret = lcd_all_pin_init();
    if (ret != 0)
    {
        printk(KERN_INFO "%s : Lcd_all_pin_init is failed\n", THIS_MODULE->name);
        goto Lcd_all_pin_init_failed;
    }
    // initializing the lcd
    lcd_initialize();
    printk(KERN_INFO "%s : Lcd_init() success \n", THIS_MODULE->name);
    
    return 0;

Lcd_all_pin_init_failed:
cdev_add_failed:
    for (i = i - 1; i >= 0; i--)
        cdev_del(&dev[i].cdev);

    i = dev_cnt;
device_create_failed:
    for (i = i - 1; i >= 0; i--)
        device_destroy(pclass, dev[i].lcd_devno);

    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_region_failed:
    i = dev_cnt;
kfifo_alloc_failed:
    for (i = dev_cnt - 1; i >= 0; i--)
        kfifo_free(&dev[i].dev_buf);

    kfree(dev);
dev_kmalloc_failed:
    return ret;
}

static __exit void lcd_exit(void)
{
    int i;
    dev_t devno = MKDEV(major, 0);
    printk(KERN_INFO "%s : lcd_exit() is called\n", THIS_MODULE->name);

    lcd_all_pin_free();
    printk(KERN_INFO "%s : Lcd_all_pin_free pin are free\n", THIS_MODULE->name);

    for(i=dev_cnt-1; i>=0; i--)
    mutex_destroy(&dev[i].lock);
    printk(KERN_INFO "%s: mutex_destroy() destroyed mutex locks for all devices.\n", THIS_MODULE->name);

    for (i =dev_cnt-1; i >=0 ; i--)
        cdev_del(&dev[i].cdev);
    printk(KERN_INFO "%s : cdev_del() is successful \n", THIS_MODULE->name);

    for (i = dev_cnt-1; i >= 0; i--)
        device_destroy(pclass, dev[i].lcd_devno);
    printk(KERN_INFO "%s : device_destroy() is successful\n", THIS_MODULE->name);

    class_destroy(pclass);
    printk(KERN_INFO "%s : class_destroy() is successful \n", THIS_MODULE->name);

    unregister_chrdev_region(devno, dev_cnt);
    printk(KERN_INFO "%s : unregister_chrdev_region()  is successful\n", THIS_MODULE->name);

    for (i = dev_cnt-1; i >= 0; i--)
        kfifo_free(&dev[i].dev_buf);

    printk(KERN_INFO "%s : all device kfifo are release\n", THIS_MODULE->name);

    kfree(dev);
    printk(KERN_INFO "%s : kfree released device private struct memory \n", THIS_MODULE->name);

    printk(KERN_INFO "%s : lcd_exit() is completed\n", THIS_MODULE->name);
}

static int lcd_open(struct inode *pinode, struct file *pfile)
{   // 
    struct lcd *pdev = container_of(pinode->i_cdev, struct lcd, cdev);
    printk(KERN_INFO "%s : lcd_open is called\n", THIS_MODULE->name);
    pfile->private_data = pdev;
    // locking the device when particular device driver is perforing operation on the lcd so race conditon do not occurs.
    mutex_lock(&pdev->lock);
    printk(KERN_INFO "%s: mutex lock is acqired \n", THIS_MODULE->name);

    return 0;
}
static int lcd_close(struct inode *pinode, struct file *pfile)
{
    struct lcd *pdev = (struct lcd*)pfile->private_data;
    printk(KERN_INFO "%s : lcd_close is called\n", THIS_MODULE->name);
    // as the device driver operation is finished unloking(relasing) the device.
    mutex_unlock(&pdev->lock);
    printk(KERN_INFO "%s: mutex is unlock\n", THIS_MODULE->name);

    return 0;
}
static ssize_t lcd_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset)
{   // You can't read data from lcd so this operation function is not implemented
    printk(KERN_INFO "%s : lcd_read is called\n", THIS_MODULE->name);
    return size;
}
static ssize_t lcd_write(struct file *pfile, const char __user *ubuf, size_t size, loff_t *poffset)
{
    int ret, nbytes;
    struct lcd *pdev = (struct lcd *)pfile->private_data;
    printk(KERN_INFO "%s : lcd_write is called\n", THIS_MODULE->name);

    // making sure that before writing into kfifo it is NULL/reset.
    kfifo_reset(&pdev->dev_buf);
    printk("inside lcd_write: size value= %d\n", size);
    ret = kfifo_from_user(&pdev->dev_buf, ubuf, size, &nbytes);
    if (ret != 0)
    {
        printk(KERN_ERR "%s : bytes not copied from user buffer %d\n", THIS_MODULE->name, ret);
        return -EFAULT;
    }

    //before writing data on the lcd clearing the lcd display
    lcd_clearDisplay();

    lcd_print(&pdev->dev_buf, LCD_LINE_NUM_ONE);
    printk(KERN_INFO "%s : data written into lcd\n", THIS_MODULE->name);

    return nbytes;
}

static long lcd_ioctl(struct file *pfile, unsigned cmd, unsigned long param)
{
    unsigned int i;
    switch (cmd)
    {
    case LCD_CLEAR_IOCTL:
        lcd_clearDisplay();
        printk(KERN_INFO "lcd_ioctl : lcd_clear is called\n");
        break;
    case LCD_SHIFT_LEFT:
        i=(unsigned int)param;
        printk(KERN_INFO "lcd_ioctl : lcd_shift_left is called\n");
        while (i>0)
        {
            lcd_shift_left();
            i--;
        }     
        break;
    case LCD_SHIFT_RIGHT:
        i=(unsigned int)param;
        printk(KERN_INFO "lcd_ioctl : lcd_shift_right is called\n");
        while (i>0)
        {
            lcd_shift_right();
            i--;
        }
        break;
    default:
        printk(KERN_INFO "%s : Invaild cmd\n", THIS_MODULE->name);
        return -EINVAL;
        break;
    }
    return 0;
}

static int lcd_all_pin_init(void)
{
    int i, ret;
    int size = ARRAY_SIZE(lcd_pin); // ARRAY_SIZE() is macro define the kernel and it returns the number of elements inside the array
    bool valid;
    char *lcd_pin_name[] = {"LCD_RS", "LCD_EN", "LCD_D4", "LCD_D5", "LCD_D6", "LCD_D7"};

    for (i = 0; i < size; i++)
    {
        valid = gpio_is_valid(lcd_pin[i]);
        if (!valid)
        {
            printk(KERN_INFO "%s: GPIO pin %d is invalid\n", THIS_MODULE->name, lcd_pin[i]);
            ret = -EINVAL;
            goto gpio_invalid;
        }
        printk(KERN_INFO "%s : GPIO pin %d is valid\n", THIS_MODULE->name, lcd_pin[i]);

        ret = gpio_request(lcd_pin[i], lcd_pin_name[i]);
        if (ret != 0)
        {
            printk(KERN_INFO "%s : GPIO pid %d is busy\n", THIS_MODULE->name, lcd_pin[i]);
            ret = -EBUSY;
            goto gpio_invalid;
        }

        ret = gpio_direction_output(lcd_pin[i], 0);
        if (ret != 0)
        {
            printk(KERN_INFO "%s : GPIO pin %d direction is not set\n", THIS_MODULE->name, lcd_pin[i]);
            ret = -EIO;
            goto gpio_dirction_failed;
        }
        printk(KERN_INFO "%s : GPIO pin %d direction set as output\n", THIS_MODULE->name, lcd_pin[i]);
    }

    printk(KERN_INFO "%s : Lcd_all_pin_init is successful\n", THIS_MODULE->name);

    return 0;

gpio_dirction_failed:
    gpio_free(lcd_pin[i]);
gpio_invalid:
    for (i = i - 1; i > 0; i--)
        gpio_free(lcd_pin[i]);

    return ret;
}

static void lcd_all_pin_free(void)
{
    int i, size;
    size = ARRAY_SIZE(lcd_pin);

    for (i = 0; i < size; i++)
    {
        gpio_free(lcd_pin[i]);// releasing the all the gpio pin of BBB
    }
}

static void lcd_instruction(char command)
{
    int db7_data = 0;
    int db6_data = 0;
    int db5_data = 0;
    int db4_data = 0;

    usleep_range(2000, 3000); // added delay instead of busy checking

    // Upper 4 bit data (DB7 to DB4)
    db7_data = ((command) & (0x1 << 7)) >> (7);
    db6_data = ((command) & (0x1 << 6)) >> (6);
    db5_data = ((command) & (0x1 << 5)) >> (5);
    db4_data = ((command) & (0x1 << 4)) >> (4);

    gpio_set_value(LCD_D7, db7_data);
    gpio_set_value(LCD_D6, db6_data);
    gpio_set_value(LCD_D5, db5_data);
    gpio_set_value(LCD_D4, db4_data);

    // Set to command mode
    gpio_set_value(LCD_RS, LCD_CMD);
    usleep_range(5, 10);

    // Simulating falling edge triggered clock
    gpio_set_value(LCD_EN, 1);
    usleep_range(5, 10);
    gpio_set_value(LCD_EN, 0);
}

/*
 * description:		send a 1-byte ASCII character data to the HD44780 LCD controller.
 * @param data		a 1-byte data to be sent to the LCD controller. Both the upper 4 bits and the lower 4 bits are used.
 */
static void lcd_data(char data)
{
    int db7_data = 0;
    int db6_data = 0;
    int db5_data = 0;
    int db4_data = 0;

    // Part 1.  Upper 4 bit data (from bit 7 to bit 4)
    usleep_range(2000, 3000); // added delay instead of busy checking

    db7_data = ((data) & (0x1 << 7)) >> (7);
    db6_data = ((data) & (0x1 << 6)) >> (6);
    db5_data = ((data) & (0x1 << 5)) >> (5);
    db4_data = ((data) & (0x1 << 4)) >> (4);

    gpio_set_value(LCD_D7, db7_data);
    gpio_set_value(LCD_D6, db6_data);
    gpio_set_value(LCD_D5, db5_data);
    gpio_set_value(LCD_D4, db4_data);

    // Part 1. Set to data mode
    gpio_set_value(LCD_RS, LCD_DATA);
    usleep_range(5, 10);

    // Part 1. Simulating falling edge triggered clock
    gpio_set_value(LCD_EN, 1);
    usleep_range(5, 10);
    gpio_set_value(LCD_EN, 0);

    // Part 2. Lower 4 bit data (from bit 3 to bit 0)
    usleep_range(2000, 3000); // added delay instead of busy checking

    db7_data = ((data) & (0x1 << 3)) >> (3);
    db6_data = ((data) & (0x1 << 2)) >> (2);
    db5_data = ((data) & (0x1 << 1)) >> (1);
    db4_data = ((data) & (0x1));

    gpio_set_value(LCD_D7, db7_data);
    gpio_set_value(LCD_D6, db6_data);
    gpio_set_value(LCD_D5, db5_data);
    gpio_set_value(LCD_D4, db4_data);

    // Part 2. Set to data mode
    gpio_set_value(LCD_RS, LCD_DATA);
    usleep_range(5, 10);

    // Part 2. Simulating falling edge triggered clock
    gpio_set_value(LCD_EN, 1);
    usleep_range(5, 10);
    gpio_set_value(LCD_EN, 0);
}

static void lcd_initialize()
{
    usleep_range(41 * 1000, 50 * 1000); // wait for more than 40 ms once the power is on

    lcd_instruction(0x30);            // Instruction 0011b (Function set)
    usleep_range(5 * 1000, 6 * 1000); // wait for more than 4.1 ms

    lcd_instruction(0x30);  // Instruction 0011b (Function set)
    usleep_range(100, 200); // wait for more than 100 us

    lcd_instruction(0x30);  // Instruction 0011b (Function set)
    usleep_range(100, 200); // wait for more than 100 us

    lcd_instruction(0x20);  /* Instruction 0010b (Function set)
                   Set interface to be 4 bits long
                */
    usleep_range(100, 200); // wait for more than 100 us

    lcd_instruction(0x20); // Instruction 0010b (Function set)
    lcd_instruction(0x80); /* Instruction NF**b
                  Set N = 1, or 2-line display
                  Set F = 0, or 5x8 dot character font
                */
    usleep_range(41 * 1000, 50 * 1000);

    /* Display off */
    lcd_instruction(0x00); // Instruction 0000b
    lcd_instruction(0x80); // Instruction 1000b
    usleep_range(100, 200);

    /* Display clear */
    lcd_instruction(0x00); // Instruction 0000b
    lcd_instruction(0x10); // Instruction 0001b
    usleep_range(100, 200);

    /* Entry mode set */
    lcd_instruction(0x00); // Instruction 0000b
    lcd_instruction(0x60); /* Instruction 01(I/D)Sb -> 0110b
                  Set I/D = 1, or increment or decrement DDRAM address by 1
                  Set S = 0, or no display shift
               */
    usleep_range(100, 200);

    /* Initialization Completed, but set up default LCD setting here */

    /* Display On/off Control */
    lcd_instruction(0x00); // Instruction 0000b
    lcd_instruction(0xF0); /* Instruction 1DCBb
                  Set D= 1, or Display on
                  Set C= 1, or Cursor on
                  Set B= 1, or Blinking on
               */
    usleep_range(100, 200);
}

static void lcd_print(struct kfifo *msg, unsigned int lineNumber)
{
    int i, ret, len;
    unsigned int counter = 0;
    unsigned int lineNum = lineNumber;
    char buf[fifo_size + 1];

    printk(KERN_INFO "%s : lcd_print is called\n", THIS_MODULE->name);

    if (msg == NULL)
    {
        printk(KERN_INFO "%s: kfifo buffer is empty\n", THIS_MODULE->name);
        return;
    }

    if ((lineNum != 1) && (lineNum != 2))
    {
        printk(KERN_DEBUG "ERR: Invalid line number readjusted to 1 \n");
        lineNum = 1;
    }

    while (!kfifo_is_empty(msg))
    {
        i = 0;
        len = kfifo_len(msg);
        printk("inside lcd_print : kfifo_len = %d", len);
        ret = kfifo_out(msg, buf, len);
        buf[ret] = '\0';

        if (lineNum == 1)
        {
            lcd_setLinePosition(LCD_LINE_NUM_ONE);
        }
        else
        {
            lcd_setLinePosition(LCD_LINE_NUM_TWO);
        }

        for (i = 0; buf[i] != '\0'; i++)
        {
            if (counter >= NUM_CHARS_PER_LINE)
            {
                if (lineNum == 1)
                {
                    lineNum = 2;
                    lcd_setLinePosition(LCD_LINE_NUM_TWO);
                    counter = 0;
                }
                else
                    break;
            }
            lcd_data(buf[i]);
            counter++;
        }

        if (counter >= NUM_CHARS_PER_LINE)
        {
            printk(KERN_INFO "%s : Data inside the kfifo more then 32 character\n", THIS_MODULE->name);
            break;
        }
    }
}

static void lcd_setLinePosition(unsigned int line)
{
    if (line == 1)
    {
        lcd_instruction(0x80); // set position to LCD line 1
        lcd_instruction(0x00);
    }
    else if (line == 2)
    {
        lcd_instruction(0xC0);
        lcd_instruction(0x00);
    }
    else
    {
        printk(KERN_INFO "%s: Invalid line number. Select either 1 or 2 \n", THIS_MODULE->name);
    }
}

static void lcd_clearDisplay(void)
{
    lcd_instruction(0x00); // upper 4 bits of command
    lcd_instruction(0x10); // lower 4 bits of command

    printk(KERN_INFO "%s: display clear\n", THIS_MODULE->name);
}

static void lcd_shift_left(void)
{
    lcd_instruction(0x10);
    lcd_instruction(0x80);
    usleep_range(10,20);
    printk(KERN_INFO "%s: lcd_shift left is called\n", THIS_MODULE->name);
}

static void lcd_shift_right(void)
{
    lcd_instruction(0x10);
    lcd_instruction(0xC0);
    usleep_range(10,20);
    printk(KERN_INFO "%s: lcd_shift right is called\n", THIS_MODULE->name);
}


module_init(lcd_init);
module_exit(lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Parth");
MODULE_DESCRIPTION("This kernel module is for lcd");