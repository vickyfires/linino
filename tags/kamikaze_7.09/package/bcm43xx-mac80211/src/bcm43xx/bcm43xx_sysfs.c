/*

  Broadcom BCM43xx wireless driver

  SYSFS support routines

  Copyright (c) 2006 Michael Buesch <mb@bu3sch.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "bcm43xx_sysfs.h"
#include "bcm43xx.h"
#include "bcm43xx_main.h"
#include "bcm43xx_phy.h"

#include <linux/capability.h>


#define GENERIC_FILESIZE	64


static int get_integer(const char *buf, size_t count)
{
	char tmp[10 + 1] = { 0 };
	int ret = -EINVAL;

	if (count == 0)
		goto out;
	count = min(count, (size_t)10);
	memcpy(tmp, buf, count);
	ret = simple_strtol(tmp, NULL, 10);
out:
	return ret;
}

static int get_boolean(const char *buf, size_t count)
{
	if (count != 0) {
		if (buf[0] == '1')
			return 1;
		if (buf[0] == '0')
			return 0;
		if (count >= 4 && memcmp(buf, "true", 4) == 0)
			return 1;
		if (count >= 5 && memcmp(buf, "false", 5) == 0)
			return 0;
		if (count >= 3 && memcmp(buf, "yes", 3) == 0)
			return 1;
		if (count >= 2 && memcmp(buf, "no", 2) == 0)
			return 0;
		if (count >= 2 && memcmp(buf, "on", 2) == 0)
			return 1;
		if (count >= 3 && memcmp(buf, "off", 3) == 0)
			return 0;
	}
	return -EINVAL;
}

static ssize_t bcm43xx_attr_interfmode_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct bcm43xx_wldev *wldev = dev_to_bcm43xx_wldev(dev);
	ssize_t count = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	mutex_lock(&wldev->wl->mutex);

	switch (wldev->phy.interfmode) {
	case BCM43xx_INTERFMODE_NONE:
		count = snprintf(buf, PAGE_SIZE, "0 (No Interference Mitigation)\n");
		break;
	case BCM43xx_INTERFMODE_NONWLAN:
		count = snprintf(buf, PAGE_SIZE, "1 (Non-WLAN Interference Mitigation)\n");
		break;
	case BCM43xx_INTERFMODE_MANUALWLAN:
		count = snprintf(buf, PAGE_SIZE, "2 (WLAN Interference Mitigation)\n");
		break;
	default:
		assert(0);
	}

	mutex_unlock(&wldev->wl->mutex);

	return count;
}

static ssize_t bcm43xx_attr_interfmode_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct bcm43xx_wldev *wldev = dev_to_bcm43xx_wldev(dev);
	unsigned long flags;
	int err;
	int mode;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	mode = get_integer(buf, count);
	switch (mode) {
	case 0:
		mode = BCM43xx_INTERFMODE_NONE;
		break;
	case 1:
		mode = BCM43xx_INTERFMODE_NONWLAN;
		break;
	case 2:
		mode = BCM43xx_INTERFMODE_MANUALWLAN;
		break;
	case 3:
		mode = BCM43xx_INTERFMODE_AUTOWLAN;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&wldev->wl->mutex);
	spin_lock_irqsave(&wldev->wl->irq_lock, flags);

	err = bcm43xx_radio_set_interference_mitigation(wldev, mode);
	if (err) {
		printk(KERN_ERR PFX "Interference Mitigation not "
				    "supported by device\n");
	}
	mmiowb();
	spin_unlock_irqrestore(&wldev->wl->irq_lock, flags);
	mutex_unlock(&wldev->wl->mutex);

	return err ? err : count;
}

static DEVICE_ATTR(interference, 0644,
		   bcm43xx_attr_interfmode_show,
		   bcm43xx_attr_interfmode_store);

static ssize_t bcm43xx_attr_preamble_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct bcm43xx_wldev *wldev = dev_to_bcm43xx_wldev(dev);
	ssize_t count;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	mutex_lock(&wldev->wl->mutex);

	if (wldev->short_preamble)
		count = snprintf(buf, PAGE_SIZE, "1 (Short Preamble enabled)\n");
	else
		count = snprintf(buf, PAGE_SIZE, "0 (Short Preamble disabled)\n");

	mutex_unlock(&wldev->wl->mutex);

	return count;
}

static ssize_t bcm43xx_attr_preamble_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct bcm43xx_wldev *wldev = dev_to_bcm43xx_wldev(dev);
	unsigned long flags;
	int value;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	value = get_boolean(buf, count);
	if (value < 0)
		return value;
	mutex_lock(&wldev->wl->mutex);
	spin_lock_irqsave(&wldev->wl->irq_lock, flags);

	wldev->short_preamble = !!value;

	spin_unlock_irqrestore(&wldev->wl->irq_lock, flags);
	mutex_unlock(&wldev->wl->mutex);

	return count;
}

static DEVICE_ATTR(shortpreamble, 0644,
		   bcm43xx_attr_preamble_show,
		   bcm43xx_attr_preamble_store);

int bcm43xx_sysfs_register(struct bcm43xx_wldev *wldev)
{
	struct device *dev = wldev->dev->dev;
	int err;

	assert(bcm43xx_status(wldev) == BCM43xx_STAT_INITIALIZED);

	err = device_create_file(dev, &dev_attr_interference);
	if (err)
		goto out;
	err = device_create_file(dev, &dev_attr_shortpreamble);
	if (err)
		goto err_remove_interfmode;

out:
	return err;
err_remove_interfmode:
	device_remove_file(dev, &dev_attr_interference);
	goto out;
}

void bcm43xx_sysfs_unregister(struct bcm43xx_wldev *wldev)
{
	struct device *dev = wldev->dev->dev;

	device_remove_file(dev, &dev_attr_shortpreamble);
	device_remove_file(dev, &dev_attr_interference);
}
