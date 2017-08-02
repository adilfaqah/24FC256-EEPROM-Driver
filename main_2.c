/*
CSE 438 - Embedded Systems Programming
Project: 3
Description: This is the user space test program for the EEPROM 'i2c_flash' driver.

Author: Adil Faqah
Date: 22nd March 2016
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "i2c_flash_defs.h"

//Function to return a random number that falls within the given range
unsigned int get_rand(unsigned int minimum, unsigned int maximum)
{
    int rndm;
    const unsigned int range = 1 + maximum - minimum;
    const unsigned int chunks = RAND_MAX / range;
    const unsigned int limit = chunks * range;
    do
    {
        rndm = rand();
    }
    while (rndm >= limit);
    return (minimum + (rndm/chunks));
}

//Function for generating a random string
char *generate_string(void)
{
    unsigned int i, str_length, toss;
    static char str_buff[65]; //Create a buffer
    str_length = 64;

    for(i = 0; i < str_length-1; i++)
    {
        toss = get_rand(0, 2);
        if (toss != 0)
            str_buff[i] = get_rand(65, 90); //Concatenate random ASCII characterters between 32 and 126 inclusive to the string.
        else
            str_buff[i] = ' ';
    }
    str_buff[str_length] = '\0';
    return(str_buff);
}

int main(void)
{
    int fd = 0;
    unsigned long new_addr;
    int ret = 0;
    int filled_pages = 0;
    int MPAGES = 512;
    int i, j = 0;
    int errsv;


    //Open the device
    fd = open("/dev/i2c_flash", O_RDWR);

    //Allocate memory for flash pages struct
    flash_pages *my_pages_r = (flash_pages*)malloc(sizeof(flash_pages));
    flash_pages *my_pages_w = (flash_pages*)malloc(sizeof(flash_pages));

    //Generate 512 random pages (each page has one string)
    for(i = 0; i < MPAGES; i++)
    {
        strcpy(my_pages_w->pages[i].data, generate_string());
        filled_pages += 1;
    }

    printf(KGRN"\n TEST -> Writing 1 page"RESET);

    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Write the page
    ret = write(fd, my_pages_w, 1);
    if (ret < 0)
    {
        if (errno == EBUSY)
            printf(KRED"\n Error: EBUSY - EEPROM is busy, write added to queue."RESET);
        else
        {
            printf(KRED"\n Error >> Failed to write to flash!"RESET);
            close(fd);
            return 0;
        }
    }

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    printf("\nFollowing page was succesfully written to flash memory:");
    for(i = 0; i < 1; i++)
    {
        printf("\n W > %s.", my_pages_w->pages[i].data);
    }

    printf(KGRN"\n\n TEST -> Reading 1 page"RESET);

    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);

    //Read the page and check for errors in the return value
    while (1)
    {
        ret = read(fd, my_pages_r, 1);
        errsv = errno;
        if (ret == -1)
        {
            if (errsv == EAGAIN)
            {
                printf(KRED"\n Error: EAGAIN - Read job submitted."RESET);
            }
            else if (errsv == EBUSY)
            {
                printf(KRED"\n Error: EBUSY - EEPROM is busy."RESET);
            }
        }
        else
        {
            printf("\nFollowing page was read succesfully:");
            for(i = 0; i < 1; i++)
            {
                printf("\n R > %s.", my_pages_r->pages[i].data);
            }
            break;
        }
        usleep(500000);
    }
    //Compare the data that was written to the data that was read to verify if the test was successful or not
    if (strcmp(my_pages_r->pages[0].data, my_pages_w->pages[0].data) == 0)
        printf(KBLU"\n  Test successful!"RESET);
    else
        printf(KRED"\n Test failed :("RESET);


    printf(KGRN"\n\n TEST -> Writing 8 pages"RESET);

    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Write the page
    ret = write(fd, my_pages_w, 8);
    if (ret < 0)
    {
        if (errno == EBUSY)
            printf(KRED"\n Error: EBUSY - EEPROM is busy, write added to queue."RESET);
        else
        {
            printf(KRED"\n Error >> Failed to write to flash!"RESET);
            close(fd);
            return 0;
        }
    }

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    printf("\nFollowing pages were succesfully written to flash memory:");
    for(i = 0; i < 8; i++)
    {
        printf("\n W > %s.", my_pages_w->pages[i].data);
    }

    printf(KGRN"\n\n TEST -> Reading 8 pages"RESET);

    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Read the page and check for errors in the return value
    while (1)
    {
        ret = read(fd, my_pages_r, 8);
        errsv = errno;
        if (ret == -1)
        {
            if (errsv == EAGAIN)
            {
                printf(KRED"\n Error: EAGAIN - Read job submitted."RESET);
            }
            else if (errsv == EBUSY)
            {
                printf(KRED"\n Error: EBUSY - EEPROM is busy."RESET);
            }
        }
        else
        {
            printf("\nFollowing pages were succesfully read from memory:");
            for(i = 0; i < 8; i++)
            {
                printf("\n R > %s.", my_pages_r->pages[i].data);
            }
            break;
        }
        usleep(500000);
    }
    //Compare the data that was written to the data that was read to verify if the test was successful or not
    if (strcmp(my_pages_r->pages[0].data, my_pages_w->pages[0].data) == 0 &&
            strcmp(my_pages_r->pages[1].data, my_pages_w->pages[1].data) == 0 &&
            strcmp(my_pages_r->pages[2].data, my_pages_w->pages[2].data) == 0 &&
            strcmp(my_pages_r->pages[3].data, my_pages_w->pages[3].data) == 0 &&
            strcmp(my_pages_r->pages[4].data, my_pages_w->pages[4].data) == 0 &&
            strcmp(my_pages_r->pages[5].data, my_pages_w->pages[5].data) == 0 &&
            strcmp(my_pages_r->pages[6].data, my_pages_w->pages[6].data) == 0 &&
            strcmp(my_pages_r->pages[7].data, my_pages_w->pages[7].data) == 0)
        printf(KBLU"\n  Test successful!"RESET);
    else
        printf(KRED"\n Test failed :("RESET);

    printf(KGRN"\n\n TEST -> Testing read and write wrapping/rollover"RESET);

    //Set the address pointer to 32320
    new_addr = 32320;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Write the page
    ret = write(fd, my_pages_w, 8);
    if (ret < 0)
    {
        if (errno == EBUSY)
            printf(KRED"\n Error: EBUSY - EEPROM is busy, write added to queue."RESET);
        else
        {
            printf(KRED"\n Error >> Failed to write to flash!"RESET);
            close(fd);
            return 0;
        }
    }

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    printf("\nFollowing pages were succesfully written to flash memory:");
    for(i = 0; i < 8; i++)
    {
        printf("\n W > %s.", my_pages_w->pages[i].data);
    }

    //Set the address pointer to 0
    new_addr = 32320;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Read the page and check for errors in the return value
    while (1)
    {
        ret = read(fd, my_pages_r, 8);
        errsv = errno;
        if (ret == -1)
        {
            if (errsv == EAGAIN)
            {
                printf(KRED"\n Error: EAGAIN - Read job submitted."RESET);
            }
            else if (errsv == EBUSY)
            {
                printf(KRED"\n Error: EBUSY - EEPROM is busy."RESET);
            }
        }
        else
        {
            printf("\nFollowing pages were succesfully read from memory:");
            for(i = 0; i < 8; i++)
            {
                printf("\n R > %s.", my_pages_r->pages[i].data);
            }
            break;
        }
        usleep(500000);
    }
    //Compare the data that was written to the data that was read to verify if the test was successful or not
    if (strcmp(my_pages_r->pages[0].data, my_pages_w->pages[0].data) == 0 &&
            strcmp(my_pages_r->pages[1].data, my_pages_w->pages[1].data) == 0 &&
            strcmp(my_pages_r->pages[2].data, my_pages_w->pages[2].data) == 0 &&
            strcmp(my_pages_r->pages[3].data, my_pages_w->pages[3].data) == 0 &&
            strcmp(my_pages_r->pages[4].data, my_pages_w->pages[4].data) == 0 &&
            strcmp(my_pages_r->pages[5].data, my_pages_w->pages[5].data) == 0 &&
            strcmp(my_pages_r->pages[6].data, my_pages_w->pages[6].data) == 0 &&
            strcmp(my_pages_r->pages[7].data, my_pages_w->pages[7].data) == 0)
        printf(KBLU"\n  Test successful!"RESET);
    else
        printf(KRED"\n Test failed :("RESET);

    printf(KGRN"\n\n TEST -> Testing FLASHERASE"RESET);

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            ioctl(fd, FLASHERASE);
            break;
        }
        //Else try again...
        usleep(500000);
    }

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    while (1)
    {
        ret = read(fd, my_pages_r, 512);
        errsv = errno;
        if (ret == -1)
        {
            if (errsv == EAGAIN)
            {
                printf(KRED"\n Error: EAGAIN - Read job submitted."KRED);
            }
            else if (errsv == EBUSY)
            {
                printf(KRED"\n Error: EBUSY - EEPROM is busy."RESET);
            }
        }
        else
        {
            printf("\nDisplaying first and last page to show that the flash was erased.");
            printf("\n R > ");
            for(i = 0; i < 64; i++)
            {
                printf("%c", my_pages_r->pages[0].data[i]);
            }
			printf("\n R > ");
            for(i = 0; i < 64; i++)
            {
                printf("%c", my_pages_r->pages[511].data[i]);
            }
            break;
        }
        usleep(500000);
    }

    for(i = 0; i<64; i++)
    {
        if (my_pages_r->pages[0].data[i] == 0xFF)
            j++;
    }
	for(i = 0; i<64; i++)
    {
        if (my_pages_r->pages[511].data[i] == 0xFF)
            j++;
    }
    if (j == 128)
        printf(KBLU"\n  Test successful!"RESET);
    else
        printf(KRED"\n Test failed :("RESET);

    printf(KGRN"\n\n TEST -> Submitting multiple (2) write jobs back to back"RESET);

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }


    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Write the page
    strcpy(my_pages_w->pages[0].data, "This is the first message to be written! It is 64 bytes long.:)\0");
    ret = write(fd, my_pages_w, 1);
    if (ret < 0)
    {
        if (errno == EBUSY)
            printf(KRED"\n Error: EBUSY - EEPROM is busy, write added to queue."RESET);
        else
        {
            printf(KRED"\n Error >> Failed to write to flash!"RESET);
            close(fd);
            return 0;
        }
    }

    printf("\nFollowing page was succesfully written to flash memory:");
    for(i = 0; i < 1; i++)
    {
        printf("\n W > %s.", my_pages_w->pages[i].data);
    }

    //Write the page
    strcpy(my_pages_w->pages[0].data, "This is the second message to be written! It is 64 bytes long:)\0");
    ret = write(fd, my_pages_w, 1);
    if (ret < 0)
    {
        if (errno == EBUSY)
            printf(KRED"\n Error: EBUSY - EEPROM is busy, write added to queue."RESET);
        else
        {
            printf(KRED"\n Error >> Failed to write to flash!"RESET);
            close(fd);
            return 0;
        }
    }

    printf("\nFollowing page was succesfully written to flash memory:");
    for(i = 0; i < 1; i++)
    {
        printf("\n W > %s.", my_pages_w->pages[i].data);
    }

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }


    //Set the address pointer to 0
    new_addr = 0;
    ioctl(fd, FLASHSETP, new_addr);
    printf(KYEL"\nAddress pointer set to: %x"RESET, new_addr);

    //Read the page and check for errors in the return value
    while (1)
    {
        ret = read(fd, my_pages_r, 2);
        errsv = errno;
        if (ret == -1)
        {
            if (errsv == EAGAIN)
            {
                printf(KRED"\n Error: EAGAIN - Read job submitted."RESET);
            }
            else if (errsv == EBUSY)
            {
                printf(KRED"\n Error: EBUSY - EEPROM is busy."RESET);
            }
        }
        else
        {
            printf("\nFollowing pages were read succesfully:");
            for(i = 0; i < 2; i++)
            {
                printf("\n R > %s.", my_pages_r->pages[i].data);
            }
            break;
        }
        usleep(500000);
    }

    printf(KBLU"\n  Test successful!"RESET);

    while(1)
    {
        //If the EEPROM is not currently busy
        if (ioctl(fd, FLASHGETS) == 0)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    printf(KGRN"\n\n TEST -> Testing FLASHGETP"RESET);

    while(1)
    {
        ret = ioctl(fd, FLASHGETP, new_addr);
        if (ret > -1)
        {
            break;
        }
        //Else try again...
        usleep(500000);
    }

    printf(KYEL"\nAddress pointer read from driver: 0x%x, should be 0x80 after last two writes in previous test."RESET, ret);

    if (ret == 128)
        printf(KBLU"\n  Test successful!"RESET);
    else
        printf(KRED"\n Test failed :("RESET);


    printf("\n");
    free(my_pages_w);
    free(my_pages_r);
    close(fd);

    return 0;
}
