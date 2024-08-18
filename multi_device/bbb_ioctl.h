#ifndef __BBB_IOCTL
#define __BBB_IOCTL

#include<linux/ioctl.h>

#define BUF_SIZE    32

#define LCD_CLEAR_IOCTL _IO('x',1)
#define LCD_SHIFT_LEFT  _IOW('x',2,int)  //18
#define LCD_SHIFT_RIGHT _IOW('x',3,int)  //1C

#define LCD_CLEAR       0
#define LCD_WRITE       1
#define SHIFT_LEFT  2
#define SHIFT_RIGHT 3


#endif