#ifndef __BBB_LCD
#define __BBB_LCD
 
#define LCD_RS   67  // P8_8
#define LCD_EN   44  // P8_12
#define LCD_D4   26  // P8_14
#define LCD_D5   46  // P8_16
#define LCD_D6   65  // P8_18
#define LCD_D7   61  // P8_26

#define LCD_LINE_NUM_ONE    1
#define LCD_LINE_NUM_TWO    2
#define LCD_LINE1_ADD 		0x80
#define LCD_LINE2_ADD 		0xC0
#define NUM_CHARS_PER_LINE  16

#define LCD_CMD		    0
#define LCD_DATA	    1
#define BUF_SIZE       32

static int lcd_all_pin_init(void);
static void lcd_all_pin_free(void);
static void lcd_instruction(char command);
static void lcd_data(char data);
static void lcd_initialize(void);
static void lcd_print(char * msg, unsigned int lineNumber);
static void lcd_set_line_position(unsigned int line);
static void lcd_clear_display(void);
static void lcd_shift_left(void);
static void lcd_shift_right(void);


static int lcd_open(struct inode *pinode, struct file *pfile);
static int lcd_close(struct inode *pinode, struct file *pfile);
static ssize_t lcd_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset);
static ssize_t lcd_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset);
static long lcd_ioctl(struct file *, unsigned int, unsigned long param);


#endif