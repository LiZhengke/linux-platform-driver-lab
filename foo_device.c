#include <linux/module.h>
#include <linux/platform_device.h>

static struct platform_device *foo_pdev;

static int __init foo_device_init(void)
{
	foo_pdev = platform_device_register_simple("foo", PLATFORM_DEVID_NONE, NULL, 0);
	if(IS_ERR(foo_pdev)){
		return PTR_ERR(foo_pdev);
	}
	
	pr_info("foo platform device is registered\n");	
	return 0;
}

static void __exit foo_device_exit(void)
{
	platform_device_unregister(foo_pdev);
	
	pr_info("foo platform device is unregistered\n");	
}

module_init(foo_device_init);
module_exit(foo_device_exit);

MODULE_LICENSE("GPL");

