/*
 * COPYRIGHT (C) 2009 Sean Bruno and Tyler Hayes, Portland State University
 *
 * lighty.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>      /* for copy_to_user() and copy_from_user() */
#include <linux/proc_fs.h>      /* for use of the /proc file system */
#include <linux/smp_lock.h>     /* for kern_un/lock() */
/*
 * The usb header file is needed for any usb driver, obviously.  The usb.h file
 * itself inludes the following:
 * - linux/mod_devicetable.h
 * - linux/usb/ch9.h
 * - linux/errno.h              (for -ENODEV)
 * - linux/delay.h              (for mdelay() )
 * - linux/interrupt.h          (for in_interrupt() )
 * - linux/list.h               (for struct list_head)
 * - linux/kref.h               (for struct kref)
 * - linux/device.h             (for struct device)
 * - linux/fs.h                 (for struct file_operations)
 * - linux/completion.h         (for struct completion)
 * - linux/sched.h              (for current && schedule_timeout)
 * - linux/mutex.h              (for struct mutex)
 *
 * The usb.h file itself defines just about all structs with the 'usb_' prefix
 * among other things.  linux/mod_devicetable.h defines struct usb_device_id.
 */
#include <linux/usb.h>
#include "lighty.h"             /* struct lighty, vendor/product id's, etc. */

//---------------------------------------------------------------------------

/*
 * Function prototypes
 */
static int __init lighty_init(void);
static int lighty_probe(struct usb_interface *interface,
                                                const struct usb_device_id *id);
int lighty_open(struct inode *inode, struct file *filp);
int lighty_proc_XXX(char *page, char **start, off_t offset, int count,
                                                        int *eof, void *data);
int lighty_ioctl(struct inode *i_node, struct file *file, unsigned int ui,
                                                            unsigned long ul);
void lighty_disconnect(struct usb_interface *intf);
static void lighty_delete(struct kref *kref);
int lighty_release(struct inode *inode, struct file *filp);
static void __exit lighty_cleanup_and_exit(void);

//---------------------------------------------------------------------------

/*
 * Global variables
 */
//used for creating a /proc directory (used in init() and cleanup_and_exit())
struct proc_dir_entry * lighty;

/*
 * This table conatins a list of all of the different kinds of USB devices this
 * driver can accept. If this is not set, the probe() function is never called.
 * Here, each usb_device_id struct (which is only one in this case--simulating a
 * driver that controls only a single device from a single vendor) is
 * initialized by the USB_DEVICE macro using the vendor and product ID's given
 * in the lighty.h file.  This is not the only macro you can use to initialize
 * the usb_device_id structs; see "Linux Device Drivers" 3rd. Ed. page 347. 
 */
static struct usb_device_id lighty_table [] = {
	{ USB_DEVICE(USB_LIGHTY_VENDOR_ID, USB_LIGHTY_PRODUCT_ID) },
	{ } //Terminating entry
};

/*
 * This is the main structure that all USB drivers must create.  It consists of
 * function pointers (i.e. probe, disconnect, etc), and variables that describe
 * the driver to the USB Core.
 * The name (which must be unique among all drivers in the kernel) will show up
 * in sysfs under /sys/bus/usb/drivers/ when the driver is loaded.
 */
static struct usb_driver lighty_driver = {
    .name = "lighty",
    .id_table = lighty_table,
    .probe = lighty_probe,
    .disconnect = lighty_disconnect
};

/*
 * FOPS (file operations). This struct is a collection of function pointers
 * that point to a driver's methods.
 */
struct file_operations lighty_fops = {
    .owner = THIS_MODULE,
    .open = lighty_open,
    .ioctl = lighty_ioctl,
    .release = lighty_release
};

/* 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core.  This is
 * used in the probe() function with a call to usb_register_dev().
 * 'name' is the name that sysfs uses to describe the device.
 */
static struct usb_class_driver lighty_class = {
    .name = "usb/lighty%d",
    .fops = &lighty_fops,
    .minor_base = LIGHTY_MINOR_BASE
};

//---------------------------------------------------------------------------

/*
 * This macro uses the table that was just initialized and is necessary to allow
 * user-space tools to figure put what devices this driver can control.  Of
 * course, this macro is not restricted to USB, so be sure to use the string
 * 'usb' as the first argument.
 */
MODULE_DEVICE_TABLE (usb, lighty_table);

//---------------------------------------------------------------------------

/*
 * INIT
 */
static int __init lighty_init(void)
{
    int result;

    result = usb_register(&lighty_driver);
    if (result)
        err("usb_register failed.  Error number %d", result);

    return result;
}

//---------------------------------------------------------------------------

/*
 * PROBE
 * 
 * This function is called by the USB Core when it thinks it has a struct
 * usb_interface that this driver can handle.  A pointer to the struct
 * usb_device_id that the USB Core used to make this decision is also passed to
 * this function.  If the driver claims the interface that is passed to it, it
 * should initialize the device properly and return 0.  If the driver does not
 * want to claim the device (or an error occurs, of course), it should return a
 * negative error value. [taken from "Linux Device Drivers" 3rd. Ed. page 348]
 */
static int lighty_probe(struct usb_interface *interface,
                                                const struct usb_device_id *id)
{
    struct usb_lighty *dev = NULL;              //our device struct
    struct usb_host_interface *iface_desc;      //temp pointer
    struct usb_endpoint_descriptor *endpoint;   //temp pointer
    size_t buffer_size;     			// the device's buffer for interrupt transfers
    int i;
    int retval;

    //allocate memory for the device (struct usb_lighty)
    dev = kmalloc(sizeof(struct usb_lighty), GFP_KERNEL);
    if (dev == NULL)
        return -ENOMEM;
    //clean the memory allocated with zeroes
    memset(dev, 0x00, sizeof (*dev));
    //initialize the reference count of this device
    kref_init(&dev->refcount);

    //connect usb_device struct passed in by the core with this device struct
    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    //connect the interface passed in by the usb core to this device struct
    dev->interface = interface;

    /*
     * traverse the array of endpoints that the currently active interface
     * of the device contains in order to find the appropriate endpoints for
     * this driver.  The active interface (a usb_host_interface struct)
     * consists of a struct usb_interface_descriptor ('desc', which we use to
     * see how many endpoints this interface has), and an array of struct
     * usb_host_endpoint ('endpoint').
     */
    //use temporary pointer to get device's current interface setting
    iface_desc = interface->cur_altsetting;
    //traverse the endpoint array
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        /*
         * use temporary usb_endpoint_descriptor to point to current endpoint.
         * (a usb_host_endpoint struct contains a usb_endpoint_descriptor struct
         * ('desc'). 
         */
        endpoint = &iface_desc->endpoint[i].desc;

        /*
         * check for 3 things for each endpoint we're looking for:
         *   1. our device struct has not already found an appropriate endpoint
         *   2. the device endpoint's direction is correct
         *   3. the device endpoint is of correct type (i.e. bulk transfer, etc)
         * If all are true, the appropriate endpoint has been found.
         */
        //check for bulk IN endpoint
        if (!dev->intr_in_endpointAddr &&
            (endpoint->bEndpointAddress & USB_DIR_IN) &&
            ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                == USB_ENDPOINT_XFER_INT)) {
            //grab the size of the device's buffer for interrupt transfer
            buffer_size = endpoint->wMaxPacketSize;
            //set that size in our device struct
            dev->intr_in_size = buffer_size;
            //set the address of the endpoint in our device struct
            dev->intr_in_endpointAddr = endpoint->bEndpointAddress;
            //allocate space of buffer size for our device struct intr IN buffer
            dev->intr_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
            //check for kmalloc error
            if (!dev->intr_in_buffer) {
                printk(KERN_NOTICE "Could not allocate intr_in_buffer");
                //decrement our device struct reference count
                kref_put(&dev->refcount, lighty_delete);
                return -ENOMEM;
            }
        }
        //check for intr OUT endpoint
        if (!dev->intr_out_endpointAddr &&
            !(endpoint->bEndpointAddress & USB_DIR_IN) &&
            ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                == USB_ENDPOINT_XFER_INT)) {
			//set the address of the endpoint in our device struct
            dev->intr_out_endpointAddr = endpoint->bEndpointAddress;
        }
    }
    if (!(dev->intr_in_endpointAddr && dev->intr_out_endpointAddr)) {
        printk(KERN_NOTICE "Could not find both intr-in and intr-out endpoints");
		//decrement our device struct reference count
        kref_put(&dev->refcount, lighty_delete);
        //not sure what error value to return here...
        return -ENOMEM;
    }

	//save our data pointer in this device interface for later use
    usb_set_intfdata(interface, dev);

	//now we can register the device
    retval = usb_register_dev(interface, &lighty_class);
    //check return value of registration
    if (retval) {
        printk(KERN_NOTICE "Not able to get a minor for this device.");
        //reset our data pointer
        usb_set_intfdata(interface, NULL);
		//decrement our device struct reference count
        kref_put(&dev->refcount, lighty_delete);
        //not sure what error value to return here...
        return -ENOMEM;
    }

    //let the user know what node this device is now attached to
    printk(KERN_NOTICE "USB lighty device now attached to USBlighty-%d\n", interface->minor);
    return 0;
}

//---------------------------------------------------------------------------

/*
 * OPEN
 */

int lighty_open(struct inode *inode, struct file *filp)
{
    struct usb_lighty *dev;
    struct usb_interface *interface;
    int subminor;

    //get the minor number from the inode
    subminor = iminor(inode);

    //use the minor number to get the assocaited interface
    interface = usb_find_interface(&lighty_driver, subminor);
    //make sure interface was found
    if (!interface) {
        printk (KERN_NOTICE "%s - error, can't find device for minor %d",  __FUNCTION__,
                                                                    subminor);
        return -ENODEV;
    }

    //get our data structure that was saved in the device interface in probe()
    dev = usb_get_intfdata(interface);
    if (!dev) {
	printk (KERN_NOTICE "Could not get interface data\n");
        return -ENODEV;
	}
	
    //increment our device reference counter
    kref_get(&dev->refcount);

    //save our device struct (pointer) in the file's private data structure
    filp->private_data = dev;

    //return success
    return 0;
}

//---------------------------------------------------------------------------

/*
 * PROC: XXX
 */
int lighty_proc_XXX(char *page, char **start, off_t offset, int count,
        int *eof, void *data)
{
    return 0;
}

//---------------------------------------------------------------------------

static void lighty_write_intr_callback(struct urb *urb, struct pt_regs *regs)
{
    /* sync/async unlink faults aren't errors */
    if (urb->status &&
        !(urb->status == -ENOENT ||
          urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN)) {
        dbg("%s - nonzero write bulk status received: %d",
            __FUNCTION__, urb->status);
    }
    /* free up our allocated buffer */
    usb_buffer_free(urb->dev, urb->transfer_buffer_length,
            urb->transfer_buffer, urb->transfer_dma);
}

/*
 * IOCTL
 */
int lighty_ioctl(struct inode * i_node, struct file * file, unsigned int cmd,
                                                            unsigned long arg)
{
	int err = 0;
	int retval = 0;
	struct urb *usb_led;
	char command; // a, b, c ,d, e, f
	char *buf;
	struct usb_lighty *dev = file->private_data;
       /*
        * extract the type and number bitfields, and don't decode
        * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
        */
        if (_IOC_TYPE(cmd) != LIGHTY_IOCTL_MAGIC)
        {
                printk(KERN_NOTICE
                        "lighty_ioctl: !lighty_IOC_MAGIC\n");
                return -ENOTTY;
        }
        if (_IOC_NR(cmd) > LIGHTY_IOCTL_MAX)
        {
                printk(KERN_NOTICE
                        "lighty_ioctl:  > lighty_IOC_MAXNR\n");
                return -ENOTTY;
        }

       /*
        * If not root/sysadmin, go away 
        */
        if (! capable (CAP_SYS_ADMIN))
                return -EPERM;

        /*
         * the direction is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. `Type' is user-oriented, while
         * access_ok is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
        if (err)
        {
                printk(KERN_NOTICE
                        "lighty_ioctl: access !ok\n");
                return -EFAULT;
        }


        switch(cmd) {
                case LIGHTY_IOCTL_1RED:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_1RED\n");
			command = 'a';
			break;
                case LIGHTY_IOCTL_1GREEN:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_1GREEN\n");
			command = 'b';
			break;
                case LIGHTY_IOCTL_1BLUE:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_1BLUE\n");
			command = 'c';
			break;
                case LIGHTY_IOCTL_2RED:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_2RED\n");
			command = 'd';
			break;
                case LIGHTY_IOCTL_2GREEN:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_2GREEN\n");
			command = 'e';
			break;
                case LIGHTY_IOCTL_2BLUE:
                        printk(KERN_NOTICE
                                "LIGHTY_IOCTL_2BLUE\n");
			command = 'f';
			break;
		default:
			printk(KERN_NOTICE
				"Not a known command %x\n", cmd);
			return -ENOMEM;
	}
	usb_led = usb_alloc_urb(0, GFP_KERNEL);
	if (!usb_led) {
		return -ENOMEM;
	}
	buf = usb_buffer_alloc(dev->udev, 64, GFP_KERNEL, &usb_led->transfer_dma);
	if (!buf) {
		printk (KERN_NOTICE "usb_buffer_alloc failed\n");
    		usb_buffer_free(dev->udev, usb_led->transfer_buffer_length,
            		usb_led->transfer_buffer, usb_led->transfer_dma);
		return -ENOMEM;
	}
	buf[0] = command; // a, b, c, d, e or f
	usb_fill_int_urb(usb_led, dev->udev, usb_sndintpipe(dev->udev, dev->intr_out_endpointAddr), buf,
				64, (usb_complete_t)lighty_write_intr_callback, dev, 250);
	usb_led->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	if( (retval = usb_submit_urb(usb_led, GFP_KERNEL)) ) {
		err("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
	}
	return 0;
}

//---------------------------------------------------------------------------

/*
 * DISCONNECT
 *
 * This function is called by the USB Core when the struct usb_interface has
 * been removed from the system or when the driver is being unloaded from the
 * USB Core. [taken from "Linux Device Drivers" 3rd. Ed. page 348]
 */

void lighty_disconnect(struct usb_interface *interface)
{
    struct usb_lighty *dev;
    int minor = interface->minor;

    //acquire the big kernel lock to prevent open() from racing disconnect()
    lock_kernel();

    //get our data structure that was saved in the device interface in probe()
    dev = usb_get_intfdata(interface);
    //now set it to NULL
    usb_set_intfdata(interface, NULL);

    //unregister the device to give back the minor number to the usb core
    usb_deregister_dev(interface, &lighty_class);

    //release the kernel lock
    unlock_kernel();

    //decrement our reference counter
    kref_put(&dev->refcount, lighty_delete);

    //output disconnect message
    printk(KERN_NOTICE "USB lighty #%d now disconnected\n", minor);
}

//---------------------------------------------------------------------------

/*
 * DELETE
 */
static void lighty_delete(struct kref *kref)
{
    struct usb_lighty *dev;

    //get the lighty_device struct that the kref object refers to
    dev = to_lighty_dev(kref);

    if (dev) {
        //release a use of the usb device structure
        usb_put_dev(dev->udev);
        //free the memory allocated for the intr in transfer buffer
        if (dev->intr_in_buffer)
            kfree (dev->intr_in_buffer);
        //free the device structure
        kfree (dev);
    }
}

//---------------------------------------------------------------------------

/*
 * RELEASE
 */
int lighty_release(struct inode *inode, struct file *filp)
{
    struct usb_lighty *dev;

    //get the lighty_device struct that the file's private data points to
    dev = (struct usb_lighty *)filp->private_data;
    if (!dev)
        return -ENODEV;

    //decrement our device reference counter
    kref_put(&dev->refcount, lighty_delete);
    //return success
    return 0;
}

//---------------------------------------------------------------------------

/*
 * EXIT
 */
static void __exit lighty_cleanup_and_exit(void)
{
    /*
     * unregister the lighty_driver struct from the kernel.  When this call
     * happens, any USB interfaces that were currently bound to this driver are
     * disconnected, and lighty_disconnect() is called for them.
     * ["Linux Device Drivers" 3rd. Ed. page 349] 
     */
    usb_deregister(&lighty_driver);
}

//---------------------------------------------------------------------------


//tells kernel which functions run when driver is loaded/removed
module_init(lighty_init);
module_exit(lighty_cleanup_and_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Sean Bruno and Tyler Hayes");
