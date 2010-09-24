/*
 * COPYRIGHT (C) 2009 Sean Bruno and Tyler Hayes, Portland State University
 *
 * test.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "test.h"


//function delarations
void checkQuit (char * input);
int checkSyntax (char * input);
void makeIoctl (int light, int color, int lighty_device);


/*---------------------------------- MAIN ------------------------------------*/

int main () {
    int lighty_device;  //file desciptor for device
    char buffer[MAX_BUF_LENGTH+1];
    char * fgetsCatcher;
    int light = 0;
    int color = 0;

    //open the device
    lighty_device = open("/dev/misc/lighty0", O_RDWR);
    if (lighty_device < 0)
        perror("open");
    while (1) {
        outputPrompt();
        //get user input
        fgetsCatcher = fgets(buffer, MAX_BUF_LENGTH, stdin);
        //check fgets error
        if (!fgetsCatcher) {
            printf("\n***Error in fgets(). Exiting.\n\n");
            exit(-1);
        }
        if (buffer[0] == '\n')
            continue;
        //check for user quitting (typing 'q')
        checkQuit(buffer);
        //check syntax of input
        if (checkSyntax(buffer) < 0) {
            printf("\n***Syntax error. Try again.***\n");
            continue;
        }
        //get the light number
        light = buffer[0]-'0';

        //get the color
        switch (buffer[2]) {
            case 'r': color = RED;
                      break;
            case 'g': color = GREEN;
                      break;
            case 'b': color = BLUE;
                      break;
        }

        //make the ioctl call using the given light and color
        makeIoctl(light, color, lighty_device);
    }

    //this will never execute
    exit(0);
}


/*-------------------------------- functions ---------------------------------*/

/*
 * checkQuit.  Exit program if user enters "q".
 */
void checkQuit (char * input)
{
    if (strcmp(input, "q\n") == 0) {
        printf("\n");
        exit(0);
    }
}


/*
 * checkSyntax.  Checks the syntax of user's input.  Returns -1 on error.
 */
int checkSyntax (char * input)
{
    //check light is 1 or 2
    if (input[0] != '1' && input[0] != '2')
        return -1;
    //check for '='
    if (input[1] != '=')
        return -1;
    //check color is 'r', 'g', or 'b'
    if (input[2] != 'r' &&
        input[2] != 'g' &&
        input[2] != 'b')
        return -1;

    return 0;
}


/*
 * makeIoctl.  Takes the given light and color and makes the appropriate ioctl
 * call.
 */
void makeIoctl (int light, int color, int lighty_device)
{
    int ioctl_return = 0;
    int command = (light << 3) | color;

    switch (command) {
        case RED1: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_1RED);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
        case GREEN1: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_1GREEN);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
        case BLUE1: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_1BLUE);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
        case RED2: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_2RED);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
        case GREEN2: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_2GREEN);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
        case BLUE2: ioctl_return = ioctl(lighty_device, LIGHTY_IOCTL_2BLUE);
                   if (ioctl_return < 0)
                       perror("ioctl");
                   break;
    }
}


/*-------------------------------- EOF ---------------------------------------*/
