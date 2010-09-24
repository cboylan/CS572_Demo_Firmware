/*
 * COPYRIGHT (C) 2009 Sean Bruno and Tyler Hayes, Portland State University
 *
 * test.h
 */

/*----------------------------------------------------------------------------*/

/*
 * CONSTANTS
 */

//maximum length of buffer to hold user's input 
#define MAX_BUF_LENGTH 25
//colors
#define RED 0x1
#define GREEN 0x2
#define BLUE 0x4
//ioctl command shortcuts
#define RED1 0x9
#define GREEN1 0xA
#define BLUE1 0xC
#define RED2 0x11
#define GREEN2 0x12
#define BLUE2 0x14

/*----------------------------------------------------------------------------*/

/*
 * MACROS
 */

//outputs a command line prompt
#define outputPrompt() printf(" > ")

/*----------------------------------------------------------------------------*/

/*
 * IOCTL DEFINITIONS
 */

#define LIGHTY_IOCTL_MAGIC 0xFF
#define LIGHTY_IOCTL_1RED _IO(LIGHTY_IOCTL_MAGIC, 0)
#define LIGHTY_IOCTL_1GREEN _IO(LIGHTY_IOCTL_MAGIC, 1)
#define LIGHTY_IOCTL_1BLUE _IO(LIGHTY_IOCTL_MAGIC, 2)
#define LIGHTY_IOCTL_2RED _IO(LIGHTY_IOCTL_MAGIC, 3)
#define LIGHTY_IOCTL_2GREEN _IO(LIGHTY_IOCTL_MAGIC, 4)
#define LIGHTY_IOCTL_2BLUE _IO(LIGHTY_IOCTL_MAGIC, 5)


/*-------------------------------- EOF ---------------------------------------*/
