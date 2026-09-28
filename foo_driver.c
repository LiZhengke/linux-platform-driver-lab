#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct foo {
	struct device *dev;
	int counter;
};

static int foo_probe(struct platform_device *pdev)
{
	struct foo *foo;
	
	foo = devm_kzalloc(&pdev->dev,sizeof(*foo),GFP_KERNEL);
	
	if(!foo)
		return -ENOMEM;
	
	foo->dev = &pdev->dev;
	
	platform_set_drvdata(pdev, foo);
	
	dev_info(&pdev->dev,"foo device probed.\n");
	return 0;	
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);
	
	dev_info(&pdev->dev,"foo counter %d\n",foo->counter);
	
	return 0;
}

static struct platform_driver foo_driver = {
	.probe = foo_probe,
	.remove = foo_remove,
	.driver = {
		.name = "foo",
	},
};

module_platform_driver(foo_driver);

MODULE_LICENSE("GPL");

