/*
 * COPYRIGHT (C) 2009 Sean Bruno and Tyler Hayes, Portland State University
 *
 * lighty.h
 */


//----------------------------------------------------------------------------

/*
 * MAJOR/MINOR NUMBERS
 *
 * USB has a reserved major number of 180, so we do not need to set up our own
 * major number, or have one setup dynamically like a char device would require.
 * Instead we just have a base number for minor numbers, which here is
 * arbitrary.  This is used in the usb_class_driver struct, which is used when
 * registering the device with a call to usb_register_dev() in the probe()
 * function.
 * All devices associated with this driver are created with unique, increasing
 * minor numbers beginning with this value.
 */
#define LIGHTY_MINOR_BASE 100

//----------------------------------------------------------------------------

/*
 * IOCTL DEFINITIONS
 *
 * 0xFF is chosen as the driver's "magic number" simply because it's not listed
 * as being used in the Documentaion/ioctl/ioctl-number.txt file.  (See
 * "Linux Device Drivers" 3rd Ed. pgs. 137-140 for more detail,
 * asm-generic/ioctl.h for macro definitions, and Documentation/ioctl/ioctl-
 * number.txt for magic numbers)
 * Each of these commands turns a specific light (1 or 2) to a specific color
 * (red, green, or blue). 
 */
#define LIGHTY_IOCTL_MAGIC 0xFF
#define LIGHTY_IOCTL_1RED _IO(LIGHTY_IOCTL_MAGIC, 0)
#define LIGHTY_IOCTL_1GREEN _IO(LIGHTY_IOCTL_MAGIC, 1)
#define LIGHTY_IOCTL_1BLUE _IO(LIGHTY_IOCTL_MAGIC, 2)
#define LIGHTY_IOCTL_2RED _IO(LIGHTY_IOCTL_MAGIC, 3)
#define LIGHTY_IOCTL_2GREEN _IO(LIGHTY_IOCTL_MAGIC, 4)
#define LIGHTY_IOCTL_2BLUE _IO(LIGHTY_IOCTL_MAGIC, 5)
/*
 * this max value is used in driver's ioctl() to test that user's command number
 * passed in is valid.  The number corresponds to the largest command number.
 * Each command is given a sequential number (using the _IO, IOR, _IOW, or _IOWR
 * macros) starting with 0.
 */
#define LIGHTY_IOCTL_MAX 5

//----------------------------------------------------------------------------

/*
 * STRUCT DEFINITIONS
 */

//the device structure
struct usb_lighty {
	struct usb_device * udev;         /* the usb device for this device */
	struct usb_interface * interface; /* the interface for this device */
	unsigned char * intr_in_buffer;   /* the buffer to receive data */
	size_t intr_in_size;              /* the size of the receive buffer */
	__u8 intr_in_endpointAddr;        /* the address of the intr in endpoint */
	__u8 intr_out_endpointAddr;       /* the address of the intr out endpoint */
	struct kref refcount;             /* a reference count for this struct */
};

//----------------------------------------------------------------------------

/*
 * MISC. DEFINITIONS
 */

//16c0:0482
//Vendor and product ID's for the device
#define USB_LIGHTY_VENDOR_ID	0x16c0
#define USB_LIGHTY_PRODUCT_ID	0x0fff
//macro to get the lighty_device struct given a kref object
#define to_lighty_dev(d) container_of(d, struct usb_lighty, refcount)
