/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <asm/uaccess.h>

#include <linux/regulator/consumer.h>

#include <linux/spi/irda_spi.h>

#include <linux/workqueue.h>

#include <mach/gpiomux.h>

//#define ICE40_IOCTL_CMD_POWER_CTRL		_IOR(SPI_IOC_MAGIC, 10, __u8)

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

/*
static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
*/

static struct msm_gpiomux_config msm_irda_configs[] = {
	{
		.gpio      = 86,		/* BLSP1 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 87,		/* BLSP1 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 89,		/* BLSP1 QUP4 SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
};


/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define IRDADEV_MAJOR			0	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY)

struct spidev_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
	u8			*bufferrx;
};

struct spidev_data	 *spi_d = NULL;
struct clk *irda_clk = NULL;
int irda_clk_enabled = 0;
//struct regulator	*vdd_io = NULL;
//int vdd_io_enabled = 0;
int is_initialized = 0;
int clk_regulator_enabled = 0;
int firmware_downloaded = 0;
struct timer_list firmware_timer;
int irda_lattice_mode = 0;
struct workqueue_struct *irda_workqueue;
struct work_struct       irda_work_data;
int screen_on_stabled = 1;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

/*
 * This can be used for testing the controller, given the busnum and the
 * cs required to use. If those parameters are used, spidev is
 * dynamically added as device on the busnum, and messages can be sent
 * via this interface.
 */
static int busnum = -1;
module_param(busnum, int, S_IRUGO);
MODULE_PARM_DESC(busnum, "bus num of the controller");

static int chipselect = -1;
module_param(chipselect, int, S_IRUGO);
MODULE_PARM_DESC(chipselect, "chip select of the desired device");

static int maxspeed = 10000000;
module_param(maxspeed, int, S_IRUGO);
MODULE_PARM_DESC(maxspeed, "max_speed of the desired device");

static int spimode = SPI_MODE_3;
module_param(spimode, int, S_IRUGO);
MODULE_PARM_DESC(spimode, "mode of the desired device");

static int icedev_major = IRDADEV_MAJOR;
  
static  char fw_path[]= "/system/etc/firmware/irda/irda_top_bitmap.bin";
//uint8_t pirdata[32216]={0};
uint8_t pirdata[32316]={0};

//#define MEMBLOCK                32216
#define MEMBLOCK                32316

static void firmware_timer_func(unsigned long data);
static int ir_config_ice40(struct spi_device *spi, int type, char *pfw_path);
static void ir_config_workq(struct work_struct *work);

ssize_t spiphy_dev_write(struct spi_device *spi,u8 bits_per_word,u32 speed_hz,const char *buf,size_t count);
		
#define GPIO_SPI_CS             88
#define GPIO_3V3_EN             83
//#define GPIO_1V2_EN             67
#define GPIO_RESET_IR           75
#define GPIO_CDONE_IR           84

void ir_spi_cs(int level)
{

  gpio_set_value(GPIO_SPI_CS, level);
  mdelay(2);
  
  return;
}

static int check_ice40_state(void)
{
  int level = -1;
  level = gpio_get_value(GPIO_CDONE_IR);

  printk("check_ice40_state: level = %d \n", level);
  
  return level;
}

static void ir_reset(void)
{
  gpio_set_value(GPIO_RESET_IR, 1);
  mdelay(2);
  
  gpio_set_value(GPIO_RESET_IR, 0);
  mdelay(2);
  
  gpio_set_value(GPIO_RESET_IR, 1);
  mdelay(2);
  
  return;
}

static void ir_enable(void)
{
  printk(KERN_INFO "ice40: ir_enable enter \n");

  gpio_set_value(GPIO_3V3_EN, 1);
  mdelay(2);
/*
  gpio_set_value(GPIO_1V2_EN, 1);
  mdelay(2);
*/
  return;
}

static void ir_disable(void)
{
  printk(KERN_INFO "ice40: ir_disable enter \n");
/*
  gpio_set_value(GPIO_1V2_EN, 0);
*/
  gpio_set_value(GPIO_3V3_EN, 0);

  mdelay(10);
  return;
}

/*-------------------------------------------------------------------------*/

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void spidev_complete(void *arg)
{
       printk("spidev_complete\r\n");

	complete(arg);
}

static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = spidev_complete;
	message->context = &done;

	spin_lock_irq(&spidev->spi_lock);
	if (spidev->spi == NULL)
		status = -ESHUTDOWN;
	else{
              printk("GO spidev_sync\r\n");

		status = spi_async(spidev->spi, message);
	}
	spin_unlock_irq(&spidev->spi_lock);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= spidev->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= spidev->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
ice40dev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

       ir_spi_cs(0);

	mutex_lock(&spidev->buf_lock);
	status = spidev_sync_read(spidev, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, spidev->buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&spidev->buf_lock);

       ir_spi_cs(1);

	return status;
}

/* Write-only message with current device setup */
static ssize_t
ice40dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

       ir_spi_cs(0);
	   
	mutex_lock(&spidev->buf_lock);
	missing = copy_from_user(spidev->buffer, buf, count);
	if (missing == 0) {
		status = spidev_sync_write(spidev, count);
	} else
		status = -EFAULT;
	mutex_unlock(&spidev->buf_lock);

	ir_spi_cs(1);
	   
	return status;
}

static int spidev_message(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total;
	u8			*buf, *bufrx;
	int			status = -EFAULT;
	
       printk(KERN_INFO "ice40: spidev_message enter \n");

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	buf = spidev->buffer;
	bufrx = spidev->bufferrx;
	total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		if (total > bufsiz) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			k_tmp->rx_buf = bufrx;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
		}
		if (u_tmp->tx_buf) {
			k_tmp->tx_buf = buf;
			if (copy_from_user(buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
		}
		buf += k_tmp->len;
		bufrx += k_tmp->len;

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			"  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
              printk(KERN_INFO 	" xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : spidev->spi->max_speed_hz);

		INIT_LIST_HEAD(&k_tmp->transfer_list);

		spi_message_add_tail(k_tmp, &msg);
	}

       printk(KERN_INFO "ice40: before spidev_sync \n");

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

       printk(KERN_INFO "ice40: after spidev_sync \n");

	/* copy any rx data out of bounce buffer */
	buf = spidev->bufferrx;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
		}
		buf += u_tmp->len;
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static long
ice40dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct spidev_data	*spidev;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct spi_ioc_transfer	*ioc;
       int i;
	u8			buf[4];

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;
	/*
       printk(KERN_INFO "ice40: spi->max_speed_hz = %d,  spi->chip_select = %d,  spi->bits_per_word = %d, spi->modalias = %s, spi->master->bus_num = %d, spi->mode = %d \n", 
		spi->max_speed_hz, spi->chip_select, spi->bits_per_word, spi->modalias, spi->master->bus_num, spi->mode);

       printk(KERN_INFO "ice40: cmd = %d \n", cmd);
	*/
	
	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_LATTICE_RX_MODE:
		irda_lattice_mode = 1;
		break;
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case ICE40_IOCTL_CMD_POWER_CTRL:
		retval = __get_user(tmp, (u32 __user *)arg);
		if(retval == 0){
		if(0 == tmp){
			del_timer(&firmware_timer);
			
			if(clk_regulator_enabled){
				clk_regulator_enabled = 0;
			/*
				if((vdd_io_enabled == 1) && (vdd_io != NULL)){
					vdd_io_enabled = 0;
					regulator_disable(vdd_io);
				}
				else
					printk("%s: vdd_io is null \n", __func__);
			*/
				if((irda_clk_enabled == 1) && irda_clk != NULL){
					irda_clk_enabled = 0;
				clk_disable_unprepare(irda_clk);
				}
				else
					printk("%s: irda_clk is null \n", __func__);

				ir_disable();

				firmware_downloaded = 0;
				irda_lattice_mode = 0;
			}
		}
		else{
			if(0 == firmware_downloaded)
				ir_config_ice40(spi_d->spi,1,fw_path);	
		}
		}
		
		break;
	case SPI_IOC_LATTICE_TX_MODE:
		irda_lattice_mode = 0;
		break;
	case SPI_IOC_WR_MODE:
		retval = __get_user(tmp, (u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u8)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->max_speed_hz = save;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
		}
		break;

	default:
              printk(KERN_INFO "ice40: cmd is default \n");

		/* segmented and/or full-duplex I/O request */
		if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
				|| _IOC_DIR(cmd) != _IOC_WRITE) {
			retval = -ENOTTY;
			break;
		}

		tmp = _IOC_SIZE(cmd);
		if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) {
			retval = -EINVAL;
			break;
		}
		n_ioc = tmp / sizeof(struct spi_ioc_transfer);
		if (n_ioc == 0)
			break;

		/* copy into scratch area */
		ioc = kmalloc(tmp, GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			break;
		}
		if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		for(i = 0; i < n_ioc; i++){
			//printk(KERN_INFO "ice40: ioc->delay_usecs = %d, ioc[i].speed_hz = %d \n", ioc[i].delay_usecs, ioc[i].speed_hz);

              	ioc[i].delay_usecs =10;
              	ioc[i].speed_hz = 1000000;
		}
	
		if( (1 == n_ioc)
			&&  ioc[0].tx_buf
			&&  (4 == ioc[0].len) ){
			if (copy_from_user(buf, (const u8 __user *)
						(uintptr_t) (ioc[0].tx_buf), 4)){
				printk(KERN_INFO "ice40: copy_from_user failed!");
			}
			else{
				if( (buf[0] == 0x02)
					&& (buf[1] == 0x00)
					&& (buf[2] == 0x00)
					&& (buf[3] == 0x02) ){
					irda_lattice_mode = 1;

					printk(KERN_INFO "ice40: RX mode entered! \n");
				}
				else{
					irda_lattice_mode = 0;

					printk(KERN_INFO "ice40: RX mode exit! \n");
				}
			}
		}
		
		ir_spi_cs(0);

		/* translate to spi_message, execute */
		retval = spidev_message(spidev, ioc, n_ioc);
		kfree(ioc);

	       ir_spi_cs(1);

		break;
	}

	mutex_unlock(&spidev->buf_lock);

	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
ice40dev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return ice40dev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define ice40dev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int ice40dev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(spidev, &device_list, device_entry) {
		if (spidev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (!spidev->bufferrx) {
			spidev->bufferrx = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->bufferrx) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				kfree(spidev->buffer);
				spidev->buffer = NULL;
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			spidev->users++;
			filp->private_data = spidev;
			nonseekable_open(inode, filp);
		}

		if((0 == firmware_downloaded) && (1 == screen_on_stabled))
			ir_config_ice40(spi_d->spi,1,fw_path);	

	} else
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int ice40dev_release(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			status = 0;

	mutex_lock(&device_list_lock);
	spidev = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	spidev->users--;
	if (!spidev->users) {
		int		dofree;

		kfree(spidev->buffer);
		spidev->buffer = NULL;
		kfree(spidev->bufferrx);
		spidev->bufferrx = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&spidev->spi_lock);
		dofree = (spidev->spi == NULL);
		spin_unlock_irq(&spidev->spi_lock);

		if (dofree)
			kfree(spidev);
	}
	mutex_unlock(&device_list_lock);

	return status;
}

static const struct file_operations ice40_dev_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write =	ice40dev_write,
	.read =		ice40dev_read,
	.unlocked_ioctl = ice40dev_ioctl,
	.compat_ioctl = ice40dev_compat_ioctl,
	.open =		ice40dev_open,
	.release =	ice40dev_release,
	.llseek =	no_llseek,
};

static int ir_get_fw_block(char *buf, int len, void *image)
{
  struct file *fp = (struct file *)image;
  int rdlen;

  if (!image)
    return 0;

  rdlen = kernel_read(fp, fp->f_pos, buf, len);
  if (rdlen > 0)
    fp->f_pos += rdlen;

  return rdlen;
}

static void * ir_open_fw(char *filename)
{
  struct file *fp;

  printk("filename = %s \n", filename);

  fp = filp_open(filename, O_RDONLY, 0);
  if (IS_ERR(fp))
  {
    printk("filp_open fail \n");
    fp = NULL;
  }

  return fp;
}

static void ir_close_fw(void *fw)
{
  if (fw)
    filp_close((struct file *)fw, NULL);

  return;
}

struct completion  transfer_in_process;
 
static void spiphy_dev_complete(void *arg)
{
  printk("[spi_phy] spiphy_dev_complete\r\n");
  complete(&transfer_in_process);
  //ir_spi_cs(1);
}

/* Write-only message with current device setup */
ssize_t
spiphy_dev_write(struct spi_device *spi,u8 bits_per_word,u32 speed_hz,const char *buf,
		size_t count)
{
  int status;     
  int res;
  struct spi_message message;                                       
  struct spi_transfer transfer;   
                    
  //printk("[spi_phy]++++++++++++ spiphy_dev_write++++++++++++ count=%d\r\n",count) ;
  memset(&transfer, 0, sizeof(transfer));
  spi_message_init(&message);       

  init_completion(&transfer_in_process);                  
  message.spi= spi;
  message.complete=spiphy_dev_complete;
  //message.context=spiphy_dev;

  transfer.tx_buf = buf;           
  transfer.bits_per_word = bits_per_word;   
  transfer.speed_hz = speed_hz;
  transfer.len = count;  
  transfer.delay_usecs =10;
  INIT_LIST_HEAD(&transfer.transfer_list);
  spi_message_add_tail(&transfer, &message);  
                                     
  status = spi->master->transfer (spi, &message);  
  if(status)
  {
    printk(KERN_INFO"[spi_phy]spiphy_dev_write transfer failed!!!status=%d \n",status);
  }
  res=wait_for_completion_timeout(&transfer_in_process, 4 * HZ);
  if(!res)
  {
    printk(KERN_INFO"[spi_phy]spiphy_dev_write  timeout!\n");
    complete(&transfer_in_process);
  }
  else{
    printk(KERN_INFO"[spi_phy]spiphy_dev_write  successful!\n");
  }
  //printk("[spi_phy]------------- spiphy_dev_write-----------\r\n") ;
  return count;
}

static int ir_download_fw(struct spi_device *spi, void * ptr)
{
  //uint8_t buf[100]={0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  //int i;
  
  //send 8 clocks
  //for(i=0; i<8; i++)
  /*
  {
    spiphy_dev_write(spi,8,6000000,buf,8);
  }
  */

  //send data from bin file
  //for(i=0; i<MEMBLOCK; i++)
  {
    spiphy_dev_write(spi,8,18000000,ptr,MEMBLOCK);
  }
  /*

  //send 100 clocks
  for(i=0; i<100; i++)
  {
    spiphy_dev_write(spi,8,6000000,buf,100);
  }
  */
  
  return 0;
}

static int ir_config_ice40(struct spi_device *spi, int type, char *pfw_path)
{
  void *fw = NULL;
  int len ;
  int ret = -1;
	
	//struct spi_ioc_transfer *wr_txfer;
	uint8_t Data_array[4];
	int tmp;
	
  printk(KERN_INFO "ice40: ir_config_ice40 ++++++ \n");

  printk(KERN_INFO "ice40: spi->max_speed_hz = %d,  spi->chip_select = %d,  spi->bits_per_word = %d, spi->modalias = %s, spi->master->bus_num = %d, spi->mode = %d \n", 
		spi->max_speed_hz, spi->chip_select, spi->bits_per_word, spi->modalias, spi->master->bus_num, spi->mode);

  printk(KERN_INFO "ice40:  dev_name(spi->dev) = %s \n", dev_name(&spi->dev));
  
	if(is_initialized){
		if((irda_clk_enabled == 0) && (irda_clk != NULL)){
			irda_clk_enabled = 1;
			ret = clk_prepare_enable(irda_clk);
			if(ret)
				printk(KERN_INFO "ice40: Lattice gcc_gp1_clk prepare enable  failed\n");
			else
				printk(KERN_INFO "ice40: Lattice gcc_gp1_clk prepare enable  successfully\n");
		}
		
		/*
		if((vdd_io_enabled == 0) && (vdd_io != NULL))
		{
			vdd_io_enabled = 1;
			regulator_enable(vdd_io);
		}
  		mdelay(2);
		*/
		msm_gpiomux_install(msm_irda_configs, ARRAY_SIZE(msm_irda_configs));
		
		clk_regulator_enabled = 1;

		ir_enable();

  		// read fw to ram
 		 fw = ir_open_fw(pfw_path);
  		if(fw == NULL)
  		{
    			printk("ir_open_fw fail !\n");
    			goto err;
  		}
  		while ((len = ir_get_fw_block((char*)pirdata, MEMBLOCK, fw))) {
      			//printk("len=%d \n", len);
  		}

 		 ir_spi_cs(0);
		
  		ir_reset();

  		printk(KERN_INFO "ice40: check_ice40_state return %d \n", check_ice40_state());
  		ir_download_fw(spi, pirdata);

  		// Verify successful configuration
  		ret = check_ice40_state();
		if(1 == ret){
			printk(KERN_INFO "ice40: firmware download successful \n");
		}else{
			printk(KERN_INFO "ice40: firmware download failed \n");
		}

  		ir_spi_cs(1);
		
		if(1 == irda_lattice_mode){
			tmp = 0;
			tmp |= spi_d->spi->mode & ~SPI_MODE_MASK;
			spi_d->spi->mode = (u8)tmp;
			spi_d->spi->mode &= ~SPI_LSB_FIRST;
			spi_d->spi->bits_per_word = 8;
			spi_d->spi->max_speed_hz = 1000000;
			spi_setup(spi_d->spi);
/*
			tmp = 1 * sizeof(struct spi_ioc_transfer);
			wr_txfer = kmalloc(tmp, GFP_KERNEL);
			
			wr_txfer[0].tx_buf = (unsigned long) Data_array;
			wr_txfer[0].len = 4;
			wr_txfer[0].speed_hz = 1000000;
			wr_txfer[0].rx_buf = (unsigned long)NULL;
			wr_txfer[0].bits_per_word = 8;
			wr_txfer[0].cs_change =0;
			wr_txfer[0].delay_usecs =10;
*/			
		// Select SPI Write operation 
			Data_array[0] = (uint8_t) 0x02;
			
		// Write Command Register Bit1 Low 
			Data_array[1] = (uint8_t) 0x00; 		     //High Byte
			Data_array[2] = (uint8_t) 0x00;      //LOW Byte
			Data_array[3] = 0x00;

	  		ir_spi_cs(0);
			
			spiphy_dev_write(spi_d->spi,8,1000000,Data_array,4);

			//spidev_message(spi_d, wr_txfer, 1);

	  		ir_spi_cs(1);

		// Write Command Register Bit1 HIGH (Start learning) 
			Data_array[1] = (uint8_t) 0x00; 		     //High Byte
			Data_array[2] = (uint8_t) 0x00;      //LOW Byte
			Data_array[3] = 0x02;
			
	  		ir_spi_cs(0);
			
			spiphy_dev_write(spi_d->spi,8,1000000,Data_array,4);
			
			//spidev_message(spi_d, wr_txfer, 1);

	  		ir_spi_cs(1);

			//kfree(wr_txfer);

			printk(KERN_INFO "ice40: RX mode enabled. \n");
		}
		
  		//mdelay(1000);

		firmware_downloaded = 1;
	}

  	printk(KERN_INFO "ice40: ir_config_ice40 ------ \n");

	return 1;

 err:
  if (fw)
    ir_close_fw(fw);
  return 0;
 }


static int lattice_ir_gpio_init(void)
{
  int rc = -1;

  printk(KERN_INFO "ice40: lattice_ir_gpio_init ++++++ \n");

  rc = gpio_request(GPIO_CDONE_IR, "CDONE_IR");
  if (rc) {
    printk("gpio_request CDONE_IR error\n");
    return rc;
  }

  rc = gpio_direction_input(GPIO_CDONE_IR);
  if (rc) {
    printk("gpio_direction_input GPIO_CDONE_IR error\n");
    return rc;
  }

  rc = gpio_request(GPIO_RESET_IR, "RESET_IR");
  if (rc) {
    printk("gpio_request RESET_IR error\n");
    return rc;
  }

  rc = gpio_direction_output(GPIO_RESET_IR, 1);
  if (rc) {
    printk("GPIO_RESET_IR Unable to set direction\n");
    return rc;
  }

  rc = gpio_request(GPIO_3V3_EN, "EN_IR");
  if (rc) {
    printk("gpio_request EN_IR error\n");
    return rc;
  }

  rc = gpio_direction_output(GPIO_3V3_EN, 0);
  if (rc) {
    printk("EN_IR Unable to set direction\n");
    return rc;
  }
/*
  rc = gpio_request(GPIO_1V2_EN, "EN_IR0");
  if (rc) {
    printk("gpio_request EN_IR0 error\n");
    return rc;
  }

  rc = gpio_direction_output(GPIO_1V2_EN, 0);
  if (rc) {
    printk("EN_IR0 Unable to set direction\n");
    return rc;
  }
*/  
  rc = gpio_request(GPIO_SPI_CS, "SPI_CS");
  if (rc) {
    printk("gpio_request SPI_CS error\n");
    return rc;
  }

  rc = gpio_direction_output(GPIO_SPI_CS, 0);
  if (rc) {
    printk("SPI_CS Unable to set direction\n");
    return rc;
  }

  ir_spi_cs(1);

  printk(KERN_INFO "ice40: lattice_ir_gpio_init ------ \n");

	return rc;
}

static char ir_status[16] = "recv on";
static ssize_t ir_recv_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
  int ret = 0;

  if (NULL == page)	
  {
    return 0;
  }
  ret = sprintf(page, "%s\n", ir_status);	

  return ret;
}

static ssize_t ir_control_proc(struct file *filp, const char *buff, size_t len, loff_t *off)
{
  char messages[32] = {0};
  char buffer[7] = {0};
//  int state;
  
  memset(messages,0,sizeof(messages));

  memset(buffer,0,sizeof(buffer));
  if (len > 31)
    len = 31;
  if (copy_from_user(messages, buff, len))
    return -EFAULT;
  /*
  if (strncmp(messages, "rw_test", 7) == 0) 
  {
    memset(ir_status,0,sizeof(ir_status));
    strncpy(ir_status, "rw_test", 7);
    ir_file_rw_test();
  } 
  else if (strncmp(messages, "cdone", 5) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "cdone", 5);
    state = check_ice40_state();
    printk("cdone:%d \n", state);
  }
  else if (strncmp(messages, "spi_test", 8) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "spi_test", 8);
    spi_write_test();
  } 
  else */if (strncmp(messages, "ir_test", 7) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "ir_test", 7);
    ir_config_ice40(spi_d->spi,1,fw_path);
  }
  /*
  else if (strncmp(messages, "reset", 5) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "reset", 5);
    printk("ir_reset \n");
    //ir_reset();
  ir_spi_cs(0);
  }
  else if (strncmp(messages, "enable", 6) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "enable", 6);
    printk("ir_enable \n");
    ir_enable();
  }
  else if (strncmp(messages, "txtest", 6) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "txtest", 6);
    ir_out_tx();
  }
  else if (strncmp(messages, "rxtest", 6) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "rxtest", 6);
    ir_out_rx();
  }
  else if (strncmp(messages, "demo", 4) == 0)
  {     
    memset(ir_status,0,sizeof(ir_status));	
    strncpy(ir_status, "demo", 4);
    ir_demo_test();
  }
  */
  else 
  {
    printk("usage: echo {rw_test/cdone/spi_test} > /proc/driver/irda\n");
  }
  return len;
}

static void create_ir_proc_file(void)
{
  struct proc_dir_entry *ir_proc_file = create_proc_entry("driver/irda", 0777, NULL);
  if (ir_proc_file) 
  {
    ir_proc_file->read_proc = (read_proc_t *)ir_recv_proc;
    ir_proc_file->write_proc = (write_proc_t  *)ir_control_proc;
    printk(KERN_INFO "proc file create success!\n");
  } 
  else 
  {
    printk(KERN_INFO "proc file create failed!\n");
  }
}

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *irdadev_class;

/*-------------------------------------------------------------------------*/

static int __devinit ice40_probe(struct spi_device *spi)
{
	struct spidev_data	*spidev;
	int			status;
	unsigned long		minor;
	
	struct clk *ir_clk = NULL;
	//struct regulator	*reg_io = NULL;
	//int ret = -1;
	
	printk(KERN_INFO "ice40: ice40_probe \n");

	/* Allocate driver data */
	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	/* Initialize the driver data */
	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	mutex_init(&spidev->buf_lock);

	INIT_LIST_HEAD(&spidev->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		spidev->devt = MKDEV(icedev_major, minor);
		dev = device_create(irdadev_class, &spi->dev, spidev->devt,
				    spidev, "ice40");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		printk(KERN_INFO "ice40: /dev/ice40 created successful \n");
		
		set_bit(minor, minors);
		list_add(&spidev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else
		kfree(spidev);

       spi_d = spidev;

	printk(KERN_INFO "ice40: spi->max_speed_hz = %d,  spi->chip_select = %d,  spi->bits_per_word = %d, spi->modalias = %s, spi->dev.kobj.name = %s \n", 
		spi->max_speed_hz, spi->chip_select, spi->bits_per_word, spi->modalias, spi->dev.kobj.name);

	 /*
	//spi->mode = SPI_MODE_3;
	
	printk(KERN_INFO "ice40: spi->max_speed_hz = %d,  spi->chip_select = %d,  spi->bits_per_word = %d, spi->modalias = %s \n", 
		spi->max_speed_hz, spi->chip_select, spi->bits_per_word, spi->modalias);
	*/
	
/*
  master =spi_busnum_to_master(4);  
  spi->master = master;  
  spi->max_speed_hz=12000 * 1000;
  spi->chip_select=0;
  spi_d->mode=SPI_MODE_3;
  //spi->mode=SPI_MODE_3 | SPI_LSB_FIRST;//ZTE:modifyed by liumx
  spi->bits_per_word=8;//32;//8;
  strcpy(spi->modalias, "spiphy_dev");
  */
       lattice_ir_gpio_init();

       create_ir_proc_file();
	   
	/*   
  	reg_io = regulator_get(&spi->dev, "8226_lvs1");
	if (IS_ERR(reg_io)) {
		ret = PTR_ERR(reg_io);
		printk(KERN_INFO "ice40: Get 8226_lvs1 ldo failed \n");

		goto initialize_faled;
	}
	else
	{
		printk(KERN_INFO "ice40: Get 8226_lvs1 ldo successfully \n");
		
		vdd_io = reg_io;
	}
	*/
	
	ir_clk = clk_get(&spi->dev, "core_clk");
	if(ir_clk == NULL){
		printk(KERN_INFO "ice40: Lattice gcc_gp1_clk get failed \n");
		
		goto initialize_faled;
	}
	else
	{
		printk(KERN_INFO "ice40: Lattice gcc_gp1_clk get successfully \n");
		
		irda_clk = ir_clk;
		clk_set_rate(irda_clk, 19200000);
		//clk_disable_unprepare(irda_clk);
	}

	is_initialized = 1;
	
	init_timer(&firmware_timer);
	
	INIT_WORK(&irda_work_data, ir_config_workq);
	irda_workqueue = create_singlethread_workqueue("ice40");	
/*
       ret = ir_config_ice40(spi,1,fw_path);
       if(ret == 0)
	   	printk(KERN_INFO "ice40: ice40 config successufl \n");
	else
		printk(KERN_INFO "ice40: ice40 config failed \n");
*/	
	return status;

	
initialize_faled:
	is_initialized = 0;

	return status;
		
}

static int __devexit ice40_remove(struct spi_device *spi)
{
	struct spidev_data	*spidev = spi_get_drvdata(spi);

	if(irda_workqueue)
		destroy_workqueue(irda_workqueue);
	
	//vdd_io = NULL;
	irda_clk = NULL;
	
	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&spidev->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	device_destroy(irdadev_class, spidev->devt);
	clear_bit(MINOR(spidev->devt), minors);
	if (spidev->users == 0)
		kfree(spidev);
	mutex_unlock(&device_list_lock);

	return 0;
}

int irda_suspend_screenoff(void)
{
	printk("%s enter\n", __func__);
	
	del_timer(&firmware_timer);
	
	if(clk_regulator_enabled){
		clk_regulator_enabled = 0;
/*
		if((vdd_io_enabled == 1) && (vdd_io != NULL)){
			vdd_io_enabled = 0;
			regulator_disable(vdd_io);
		}
		else
			printk("%s: vdd_io is null \n", __func__);
*/
		if((irda_clk_enabled == 1) && irda_clk != NULL){
			irda_clk_enabled = 0;
			clk_disable_unprepare(irda_clk);
		}
		else
			printk("%s: irda_clk is null \n", __func__);

		ir_disable();

		firmware_downloaded = 0;
	}

	screen_on_stabled = 0;

	return 0;
}

int ice40_resume_screenon(void)
{
//	int ret = -1;
	
	printk("%s enter\n", __func__);
/*
	if(is_initialized){
		if((irda_clk_enabled == 0) && (irda_clk != NULL)){
			irda_clk_enabled = 1;
			ret = clk_prepare_enable(irda_clk);
			if(ret)
				printk(KERN_INFO "ice40: Lattice BBCLK2 clk prepare enable  failed when resume\n");
			else
				printk(KERN_INFO "ice40: Lattice BBCLK2 clk prepare enable  successfully when resume\n");
		}
		if((vdd_io_enabled == 0) && (vdd_io != NULL)){
			vdd_io_enabled = 1;
			regulator_enable(vdd_io);
		}
		
		ir_enable();

		clk_regulator_enabled = 1;
		
		ir_config_ice40(spi_d->spi,1,fw_path);		
	}
*/	
	//ir_config_ice40(spi_d->spi,1,fw_path);

	if(is_initialized){
	firmware_timer.function = &firmware_timer_func ;
	firmware_timer.expires = jiffies + 2*HZ;
	firmware_timer.data = 0;
	add_timer(&firmware_timer);
	}	
	return 0;
}

static void firmware_timer_func(unsigned long data)
{
	//queue_work(irda_workqueue, &irda_work_data);
	screen_on_stabled = 1;
}
/*
static int ice40_suspend(struct spi_device *spi, pm_message_t mesg)
{
	printk("%s enter\n", __func__);
	
	if(clk_regulator_enabled){
		clk_regulator_enabled = 0;

		if(vdd_io != NULL)
			regulator_disable(vdd_io);

		if(irda_clk != NULL)
			clk_disable_unprepare(irda_clk);

		ir_disable();
	}

	return 0;
}

static int ice40_resume(struct spi_device *spi)
{
	int ret = -1;
	
	printk("%s enter\n", __func__);

	if(is_initialized){
		if(irda_clk != NULL){
			ret = clk_prepare_enable(irda_clk);
			if(ret)
				printk(KERN_INFO "ice40: Lattice BBCLK2 clk prepare enable  failed when resume\n");
			else
				printk(KERN_INFO "ice40: Lattice BBCLK2 clk prepare enable  successfully when resume\n");
		}
		
		if(vdd_io != NULL)
			regulator_enable(vdd_io);

		ir_enable();

		clk_regulator_enabled = 1;
		
		//ir_config_ice40(spi_d->spi,1,fw_path);		
	}
	
	return 0;
}
*/

static void ir_config_workq(struct work_struct *work)
{
	if(0 == firmware_downloaded){
		ir_config_ice40(spi_d->spi,1,fw_path);
	}
}

static const struct of_device_id ice40_spi_of_match[] = {
	{ 
		.compatible = "lattice,ice40_irda",
	},
	{}
};

static struct spi_driver ice40_spi_driver = {
	.driver = {
		.name =		"ice40_irda",
		.owner =	THIS_MODULE,
		.of_match_table = ice40_spi_of_match,
	},
	.probe =	ice40_probe,
	.remove =	__devexit_p(ice40_remove),
	/*
	.suspend = ice40_suspend,
	.resume = ice40_resume,
	*/
	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

static int __init lattice_ir_init(void)
{
	int status;

	printk(KERN_INFO "ice40: spi driver init\n");
	
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(0, "ice40", &ice40_dev_fops);
	if (status < 0){
		printk(KERN_INFO "ice40: register char device failed!\n");
		
		return status;
	}
	else if(status > 0)
		icedev_major = status;

	irdadev_class = class_create(THIS_MODULE, "irdadev");
	if (IS_ERR(irdadev_class)) {
		status = PTR_ERR(irdadev_class);
		goto error_class;
	}

	status = spi_register_driver(&ice40_spi_driver);
	if (status < 0){
		printk(KERN_INFO "ice40: can't register spi driver!\n");
		
		goto error_register;
	}
	
	printk(KERN_INFO "ice40: register spi driver with major number %d successed!\n", icedev_major);
	
	return 0;
	
error_register:
	class_destroy(irdadev_class);
error_class:
	unregister_chrdev(icedev_major, ice40_spi_driver.driver.name);
	return status;
}
module_init(lattice_ir_init);

static void __exit lattice_ir_exit(void)
{
	spi_unregister_driver(&ice40_spi_driver);
	class_destroy(irdadev_class);
	unregister_chrdev(icedev_major, ice40_spi_driver.driver.name);
}
module_exit(lattice_ir_exit);

MODULE_DESCRIPTION("LATTICE IR Control driver");
MODULE_LICENSE("GPL v2");
