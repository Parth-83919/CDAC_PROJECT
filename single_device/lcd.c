#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/gpio.h> 
#include <linux/delay.h>
#include "bbb_ioctl.h"
#include "bbb_lcd.h"

static struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .open = lcd_open,
    .release = lcd_close,
    .read = lcd_read,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ioctl
};

static int lcd_pin[] = {
    LCD_RS,
    LCD_EN,
    LCD_D4,
    LCD_D5,
    LCD_D6,
    LCD_D7
};

static dev_t devno;
static struct cdev cdev;
static struct class *pclass;
static int major;
static char kbuf[BUF_SIZE];

static __init int lcd_init(void)
{
    int ret, minor;
    struct device *pdevice;

    // allocting char device number for lcd device driver module
    ret = alloc_chrdev_region(&devno, 0, 1, "bbb_lcd");
    if (ret < 0)
    {
        printk(KERN_INFO "%s : alloc_chrdev_region() failed\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    major = MAJOR(devno);
    minor = MINOR(devno);
    printk(KERN_INFO "%s : alloc_chrdev_region() is success. devno: %d/%d\n", THIS_MODULE->name, major, minor);

    // creating the device class in sysfs
    pclass = class_create(THIS_MODULE, "bbb_lcd");
    if (IS_ERR(pclass))
    {
        printk(KERN_INFO "%s : class_create() failed\n", THIS_MODULE->name);
        goto class_create_failed;
    }
    printk(KERN_INFO "%s : class_create() is success. \n", THIS_MODULE->name);

    // creating the device in devfs
    pdevice = device_create(pclass, NULL, devno, NULL, "bbb_lcd0");
    if (IS_ERR(pdevice))
    {
        printk(KERN_INFO "%s : device_create() is failed\n", THIS_MODULE->name);
        goto device_create_failed;
    }
    printk(KERN_INFO "%s : device_create() is success.\n", THIS_MODULE->name);

    // mapping the cdev structure with device operations
    cdev_init(&cdev, &f_ops);
    ret = cdev_add(&cdev, devno, 1);// adding cdev structure into cdev_map
    if (ret != 0)
    {
        printk(KERN_INFO "%s : cdev_add() failed\n", THIS_MODULE->name);
        goto cdev_add_failed;
    }
    printk(KERN_INFO "%s : cdev_add() is success. \n", THIS_MODULE->name);

    // initializing all pin of lcd
    ret = lcd_all_pin_init();
    if(ret!=0){
        printk(KERN_INFO"%s : lcd_all_pin_init is failed\n",THIS_MODULE->name);
        goto lcd_all_pin_init_failed;
    }
    //initializing the lcd
    lcd_initialize();
    printk(KERN_INFO "%s : Lcd_init() success \n", THIS_MODULE->name);

    return 0;

lcd_all_pin_init_failed:
cdev_add_failed:
    device_destroy(pclass, devno);
device_create_failed:
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_region_failed:
    return ret;
}

static __exit void lcd_exit(void)
{
    printk(KERN_INFO "%s : lcd_exit() is called\n", THIS_MODULE->name);
    lcd_all_pin_free();
    printk(KERN_INFO "%s : Lcd_all_pin_free pin are free\n", THIS_MODULE->name);
    cdev_del(&cdev);
    printk(KERN_INFO "%s : cdev_del() is successful \n", THIS_MODULE->name);
    device_destroy(pclass, devno);
    printk(KERN_INFO "%s : device_destroy() is successful\n", THIS_MODULE->name);
    class_destroy(pclass);
    printk(KERN_INFO "%s : class_destroy() is successful \n", THIS_MODULE->name);
    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "%s : unregister_chrdev_region()  is successful\n", THIS_MODULE->name);
    printk(KERN_INFO "%s : lcd_exit() is completed\n", THIS_MODULE->name);
}

int lcd_open(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO "%s : lcd_open is called\n", THIS_MODULE->name);
    return 0;
}
int lcd_close(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO "%s : lcd_close is called\n", THIS_MODULE->name);
    return 0;
}
ssize_t lcd_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset)
{
    // You can't read the data from the lcd so this function is not implemented
    printk(KERN_INFO "%s : lcd_read is called\n", THIS_MODULE->name);
    return size;
}
ssize_t lcd_write(struct file *pfile, const char __user *ubuf, size_t size, loff_t *poffset)
{
    int ret;
    printk(KERN_INFO "%s : lcd_write is called\n", THIS_MODULE->name);
    memset(kbuf,'\0',sizeof(kbuf)); // initializing kernel space buffer to NULL.
    ret = copy_from_user(&kbuf, ubuf, size); // coping the data from user space buffer(ubuf) to kernle space buffer(kbuf)
    if (ret != 0)
    {
        printk(KERN_ERR "%s : bytes not copied from user buffer %d\n", THIS_MODULE->name, ret);
    }
    
    lcd_clearDisplay(); // before sending data on to lcd clearing the exist data

    lcd_print(kbuf,LCD_LINE_NUM_ONE); // calling lcd_print() function for sending data to lcd
    printk(KERN_INFO "%s : lcd data write\n", THIS_MODULE->name);
    return size;
}

static long lcd_ioctl(struct file *pfile, unsigned cmd, unsigned long param){

    unsigned int i, ret;
    struct ioctl_msg msg;
    printk(KERN_INFO"%s : lcd_ioctl() is called\n", THIS_MODULE->name);
    
    // checking if parameter is provided or not
    if((void *)param == NULL){
        printk(KERN_INFO"%s : lcd_ioctl parameter is NULL\n",THIS_MODULE->name);
        return -EINVAL;
    }

    memset(&msg, '\0', sizeof(struct ioctl_msg));// initializing all member of structure to NULL.

    ret = copy_from_user(&msg, (void*)param, sizeof(struct ioctl_msg));// coping data into kernel space struct ioctl_msg
    if(ret != 0)
    {
        printk(KERN_INFO "%s : copy_from_user failed to copy %d bytes from user space\n",THIS_MODULE->name,ret);
        return -EINVAL;
    }

    switch (cmd)
    {
    case LCD_CLEAR_IOCTL:
        lcd_clearDisplay();
        printk(KERN_INFO "lcd_ioctl : lcd_clear is called\n");
        break;
    case LCD_SHIFT_LEFT:
        i=(unsigned int)msg.shift;
        printk(KERN_INFO "lcd_ioctl : lcd_shift_left is called\n");
        while (i>0)
        {
            lcd_shift_left();
            i--;
        }     
        break;
    case LCD_SHIFT_RIGHT:
        i=(unsigned int)msg.shift;
        printk(KERN_INFO "lcd_ioctl : lcd_shift_right is called\n");
        while (i>0)
        {
            lcd_shift_right();
            i--;
        }
        break;
    case LCD_PRINT_ON_FIRST_LINE:
        lcd_print(msg.buf,msg.line_number);
        printk(KERN_INFO"%s : print data on first line of lcd\n",THIS_MODULE->name);
        break;
    case LCD_PRINT_ON_SECOND_LINE:
        lcd_print(msg.buf,msg.line_number);
        printk(KERN_INFO"%s : print data on second line of lcd\n",THIS_MODULE->name);
        break;
    default:
        printk(KERN_INFO"%s : Invaild cmd\n", THIS_MODULE->name);
        return -EINVAL;
        break;
    }
    return 0;
}

static int lcd_all_pin_init(void)
{
    int i, ret;
    int size = ARRAY_SIZE(lcd_pin);
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
    size = ARRAY_SIZE(lcd_pin);//ARRAY_SIZE() is macro define in kernel which returns the no. elements inside array.
//  size = sizeof(lcd_pin)/sizeof(lcd_pin[0]) -------> inside ARRAY_SIZE macro this happen 


    for (i = 0; i < size; i++)
    {
        gpio_free(lcd_pin[i]);// releasing all gpio pin
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
    // Wait for more than 40 ms after the power is turned on, as required by the LCD datasheet.
    usleep_range(41*1000, 50*1000); 

    // Send Function Set command (0x30) to set the LCD in 8-bit mode.
    lcd_instruction(0x30);  
    // Wait for more than 4.1 ms before sending the next command.
    usleep_range(5*1000, 6*1000);  

    // Send Function Set command (0x30) again to confirm 8-bit mode.
    lcd_instruction(0x30);  
    // Wait for more than 100 microseconds.
    usleep_range(100, 200);  

    // Send Function Set command (0x30) one more time to ensure the LCD is in 8-bit mode.
    lcd_instruction(0x30);  
    // Wait for more than 100 microseconds.
    usleep_range(100, 200);  

    // Switch the LCD to 4-bit mode by sending 0x20. The interface is now set to 4-bit mode.
    lcd_instruction(0x20);  
    // Wait for more than 100 microseconds.
    usleep_range(100, 200);  

    // Send Function Set command in 4-bit mode (0x20) followed by 0x80 to configure the display.
    // 0x80 sets N=1 (2-line display) and F=0 (5x8 dot character font).
    lcd_instruction(0x20);  
    lcd_instruction(0x80);  
    usleep_range(41*1000, 50*1000);  // Wait for the instructions to be processed.

    // Turn the display off by sending Display On/Off Control command with D, C, B bits set to 0.
    lcd_instruction(0x00);  
    lcd_instruction(0x80);  // 0x80 turns off the display.
    usleep_range(100, 200);  

    // Clear the display by sending the Display Clear command (0x01).
    lcd_instruction(0x00);  
    lcd_instruction(0x10);  // 0x10 clears the display.
    usleep_range(100, 200);  

    // Set the Entry Mode, which controls how the cursor moves.
    // 0x60 sets I/D=1 (cursor moves right) and S=0 (no display shift).
    lcd_instruction(0x00);  
    lcd_instruction(0x60);  
    usleep_range(100, 200);  

    // Initialization is complete, but now set up the default display settings.

    // Turn on the display, cursor, and blinking by sending Display On/Off Control command.
    // 0xF0 sets D=1 (display on), C=1 (cursor on), and B=1 (blinking on).
    lcd_instruction(0x00);  
    lcd_instruction(0xF0);  
    usleep_range(100, 200);  
}

static void lcd_print(char *msg, unsigned int lineNumber)
{
	unsigned int counter = 1;
	unsigned int lineNum = lineNumber;

	if(msg == NULL){
		printk(KERN_INFO"Empty data for lcd_print \n");
		return;
	}

	if( (lineNum != 1) && (lineNum != 2) ) { 
		printk( KERN_INFO "Invalid line number readjusted to 1 \n");
		lineNum = 1;
	}

	if( lineNum == 1 )
	{
		lcd_setLinePosition( LCD_LINE_NUM_ONE );
		while( *(msg) != '\0' )
		{
			if(counter >=  NUM_CHARS_PER_LINE )
			{
				lineNum = 2;	// continue writing on the next line if the string is too long
				counter = 0;
				break;		
			}
			lcd_data(*msg);
			msg++;
			counter++;
		}
	}

	if( lineNum == 2)
	{
		lcd_setLinePosition( LCD_LINE_NUM_TWO);
		while( *(msg) != '\0' )
		{
			if(counter >=  NUM_CHARS_PER_LINE )
			{
				break;
			}
			lcd_data(*msg);
			msg++;
			counter++;
		}
	}
}

static void lcd_set_line_position(unsigned int line)
{
	if(line == 1){ // set position to LCD line 1
		lcd_instruction(0x80);	
		lcd_instruction(0x00);
	}
	else if(line == 2){ // set position to LCD line 2
		lcd_instruction(0xC0);  
		lcd_instruction(0x00);
	}
	else{
		printk(KERN_INFO"Invalid line number\n");
	}
}

static void lcd_clear_display()
{   // lcd clear instruction
	lcd_instruction( 0x00 ); 
	lcd_instruction( 0x10 ); 
	printk(KERN_INFO"%s : display clear\n",THIS_MODULE->name);
}

static void lcd_shift_left(void)
{
    lcd_instruction(0x10);
    lcd_instruction(0x80);
    usleep_range(10,20);
    printk(KERN_INFO"%s: lcd_shift left is called\n", THIS_MODULE->name);
}

static void lcd_shift_right(void)
{
    lcd_instruction(0x10);
    lcd_instruction(0xC0);
    usleep_range(10,20);
    printk(KERN_INFO"%s: lcd_shift right is called\n", THIS_MODULE->name);
}


module_init(lcd_init);
module_exit(lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Parth");
MODULE_DESCRIPTION("This kernel module is for lcd");