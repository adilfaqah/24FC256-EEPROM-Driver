#ifndef i2c_flash_defs_h
#define i2c_flash_defs_h

#include <linux/ioctl.h>

#define MAGIC_NUMBER 0x87
#define FLASHGETS _IO(MAGIC_NUMBER, 1)
#define FLASHGETP _IO(MAGIC_NUMBER, 2)
#define FLASHSETP _IO(MAGIC_NUMBER, 3)
#define FLASHERASE _IO(MAGIC_NUMBER, 4)

//Preprocessor directives to change the color of console output text.
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define RESET "\033[0m"


typedef struct
{
    unsigned char data[64];
} page;

typedef struct
{
    page pages[512];
}flash_pages;

#endif