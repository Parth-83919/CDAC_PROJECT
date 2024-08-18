#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "bbb_lcd.h"
#include "bbb_ioctl.h"

int main(int argc, void *argv[])
{
    int choice, len, fd, ret, shift;
    char buf[BUF_SIZE];

    fd = open("/dev/bbb_lcd0", O_WRONLY);
    if (fd < 0)
    {
        perror("open() is failed\n");
    }

    memset(buf, '\0', BUF_SIZE);

    choice = atoi(argv[1]);

    switch (choice)
    {
    case LCD_CLEAR:
        ret = ioctl(fd, LCD_CLEAR_IOCTL);
        if (ret != 0)
        {
            perror("Lcd clear is failed\n");
            return ret;
        }
        printf("ioctl : lcd clear is executed\n");
        break;
    case LCD_WRITE:
        strcpy(buf, argv[2]);
        len = strlen(argv[2]);
        ret = write(fd, buf, len);
        if (ret < 0)
        {
            perror("write() failed\n");
        }
        printf("no. of bytes send %d, string=%s len=%d\n", ret, buf, len);
        break;
    case SHIFT_LEFT:
        shift = atoi(argv[2]);
        ret = ioctl(fd, LCD_SHIFT_LEFT, shift);
        if (ret != 0)
        {
            perror("Lcd shift left is failed\n");
            return ret;
        }
        printf("ioctl : lcd shift left is exeucted\n");
        break;
    case SHIFT_RIGHT:
        shift = atoi(argv[2]);
        ret = ioctl(fd, LCD_SHIFT_RIGHT, shift);
        if (ret != 0)
        {
            perror("Lcd shift right is failed\n");
            return ret;
        }
        printf("ioctl : lcd shift right is exeucted\n");
        break;
    default:
        printf("Invalid command is given. Below is right way of providing command for lcd is shown\n");
        printf("sudo ./a.out 0 <====== lcd clear\n");
        printf("sudo ./a.out 1 data_for_lcd <====== lcd_write\n");
        printf("sudo ./a.out 2 number_of_left_shift <====== lcd_left_shift\n");
        printf("sudo ./a.out 3 number_of_right_shift <====== lcd_right_shift\n");
        break;
    }

    close(fd);
    return 0;
}