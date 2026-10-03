#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/completion.h>

struct foo {
	struct device *dev;
	int irq;
	unsigned int irq_cnt;

	struct completion cmpl;
};

static ssize_t wait_irq_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct foo *foo = dev_get_drvdata(dev);
	long ret;

	reinit_completion(&foo->cmpl);

	dev_info(dev, "waiting for IRQ...\n");

	ret = wait_for_completion_interruptible_timeout(&foo->cmpl, msecs_to_jiffies(15000));

	if(ret <0)
		return ret;

	if(ret == 0)
		return sysfs_emit(buf, "timeout\n");

	return sysfs_emit(buf, "IRQ completed,irq_cnt=%u\n", foo->irq_cnt);
}

static DEVICE_ATTR_RO(wait_irq);

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;

	++foo->irq_cnt;
	complete(&foo->cmpl);

	return IRQ_HANDLED;
}

static int foo_probe(struct platform_device *pdev)
{
	struct foo *foo;
	int ret;

	foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
	if(!foo) {
		return -ENOMEM;
	}

	init_completion(&foo->cmpl);

	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0)
		return foo->irq;

	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo", foo);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_wait_irq);
	if(ret)
		return ret;

	dev_info(&pdev->dev, "driver is registered.\n");
	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_wait_irq);
	disable_irq(foo->irq);

	dev_info(&pdev->dev, "driver is unregistered irq cnt=%u.\n", foo->irq_cnt);
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
MODULE_AUTHOR("Maxwell Li");
