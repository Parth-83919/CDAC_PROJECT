#ifndef __BBB_IOCTL
#define __BBB_IOCTL

#include<linux/ioctl.h>
#define BUF_SIZE    32

struct ioctl_msg{
    char buf[BUF_SIZE];
    unsigned int line_number;
    unsigned int shift;
};

#define LCD_CLEAR_IOCTL _IOW('x',1, struct ioctl_msg)
#define LCD_SHIFT_LEFT  _IOW('x',2,int) 
#define LCD_SHIFT_RIGHT _IOW('x',3,int)  
#define LCD_PRINT_ON_FIRST_LINE _IOW('x',4,struct ioctl_msg)
#define LCD_PRINT_ON_SECOND_LINE _IOW('x',5,struct ioctl_msg)

#define LCD_CLEAR               0 
#define LCD_WRITE               1
#define SHIFT_LEFT              2
#define SHIFT_RIGHT             3
#define PRINT_ON_FIRST_LINE     4
#define PRINT_ON_SECOND_LINE    5    

#endif