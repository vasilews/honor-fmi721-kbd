// SPDX-License-Identifier: GPL-2.0
/*
 * Honor X14 Plus 2024 Keyboard Backlight Driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/wmi.h>

#define DRIVER_NAME "honor_fmi721_kbd"
#define EC_OFFSET 0xF6

#define HONOR_WMI_EVENT_GUID "ABBC0F5C-8EA1-11D1-A000-C90629100000"

static const u8 brightness_to_ec[] = { 0x02, 0x03, 0x04 };

struct honor_kbd_data {
	struct led_classdev led;
	struct wmi_device *wdev;
	enum led_brightness last_brightness;
};

static const struct dmi_system_id honor_dmi_table[] = {
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HONOR"),
			DMI_MATCH(DMI_PRODUCT_NAME, "FMI-"),
		},
	},
	{ }
};

static enum led_brightness ec_val_to_brightness(u8 val)
{
	switch (val) {
	case 0x02:
		return 0;
	case 0x03:
		return 1;
	case 0x04:
		return 2;
	default:
		return 0;
	}
}

static int kbd_backlight_set(struct led_classdev *led_cdev,
			     enum led_brightness brightness)
{
	if (brightness > 2)
		brightness = 2;

	return ec_write(EC_OFFSET, brightness_to_ec[brightness]);
}

static enum led_brightness kbd_backlight_get(struct led_classdev *led_cdev)
{
	u8 val;

	if (ec_read(EC_OFFSET, &val) < 0)
		return 0;

	return ec_val_to_brightness(val);
}

static void honor_wmi_notify(struct wmi_device *wdev, union acpi_object *obj)
{
	struct honor_kbd_data *data = dev_get_drvdata(&wdev->dev);
	enum led_brightness brightness;

	if (!obj || obj->type != ACPI_TYPE_INTEGER)
		return;

	switch (obj->integer.value) {
	case 0x2b1:
		brightness = 0;
		break;
	case 0x2b2:
		brightness = 1;
		break;
	case 0x2b3:
		brightness = 2;
		break;
	default:
		return;
	}

	data->led.brightness = brightness;
	led_classdev_notify_brightness_hw_changed(&data->led, brightness);
}

static int honor_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct honor_kbd_data *data;
	u8 val;
	int ret;

	if (!dmi_check_system(honor_dmi_table)) {
		dev_warn(&wdev->dev, "Unsupported hardware\n");
		return -ENODEV;
	}

	ret = ec_read(EC_OFFSET, &val);
	if (ret < 0) {
		dev_err(&wdev->dev, "EC not accessible (err=%d)\n", ret);
		return -ENODEV;
	}

	data = devm_kzalloc(&wdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->wdev = wdev;

	data->led.name = "honor::kbd_backlight";
	data->led.brightness_set_blocking = kbd_backlight_set;
	data->led.brightness_get = kbd_backlight_get;
	data->led.max_brightness = 2;
	data->led.flags = LED_CORE_SUSPENDRESUME | LED_BRIGHT_HW_CHANGED;

	ret = devm_led_classdev_register(&wdev->dev, &data->led);
	if (ret < 0) {
		dev_err(&wdev->dev, "Failed to register LED: %d\n", ret);
		return ret;
	}

	data->led.brightness = kbd_backlight_get(&data->led);

	dev_set_drvdata(&wdev->dev, data);

	dev_info(&wdev->dev, "Keyboard backlight registered\n");
	return 0;
}

static void honor_wmi_remove(struct wmi_device *wdev)
{
	dev_info(&wdev->dev, "Keyboard backlight removed\n");
}

static const struct wmi_device_id honor_wmi_id_table[] = {
	{ HONOR_WMI_EVENT_GUID, NULL },
	{}
};
MODULE_DEVICE_TABLE(wmi, honor_wmi_id_table);

static struct wmi_driver honor_wmi_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.id_table = honor_wmi_id_table,
	.probe = honor_wmi_probe,
	.remove = honor_wmi_remove,
	.notify = honor_wmi_notify,
	.no_singleton = true,
};

module_wmi_driver(honor_wmi_driver);

MODULE_AUTHOR("Vasily <105429138+vasilews@users.noreply.github.com>");
MODULE_DESCRIPTION("Honor MagicBook Keyboard Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("dmi:*:svnHONOR:pnFMI-*");
