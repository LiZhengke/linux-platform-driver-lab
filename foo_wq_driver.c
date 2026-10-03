#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wait.h>

struct foo {
	struct device *dev;
	int irq;
	unsigned int irq_cnt;

	spinlock_t lock;
	wait_queue_head_t wq;
	bool ready;
	bool stopped;
};

static ssize_t wq_irq_show(struct device *dev, struct device_attribute *attr,
						   char *buf)
{
	struct foo *foo = dev_get_drvdata(dev);
	unsigned long flags;
	unsigned int irq_cnt;
	int ret;

	ret = wait_event_interruptible(foo->wq, READ_ONCE(foo->ready) || READ_ONCE(foo->stopped));
	if(ret)
		return ret;

	spin_lock_irqsave(&foo->lock, flags);
	if(foo->stopped) {
		spin_unlock_irqrestore(&foo->lock, flags);
		return -ENODEV;
	}

	irq_cnt = foo->irq_cnt;
	foo->ready = false;
	spin_unlock_irqrestore(&foo->lock, flags);

	return sysfs_emit(buf, "irq cnt=%u\n", irq_cnt);
}

static DEVICE_ATTR_RO(wq_irq);

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;

	spin_lock(&foo->lock);

	++foo->irq_cnt;
	foo->ready = true;

	spin_unlock(&foo->lock);

	wake_up_interruptible(&foo->wq);

	return IRQ_HANDLED;
}

static int foo_probe(struct platform_device *pdev)
{
	struct foo *foo;
	int ret;

	foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
	if(!foo)
		return -ENOMEM;

	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	spin_lock_init(&foo->lock);
	init_waitqueue_head(&foo->wq);

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq <0)
		return foo->irq;

	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo", foo);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_wq_irq);
	if(ret)
		return ret;

	dev_info(&pdev->dev, "driver is registered.\n");

	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);
	unsigned long flags;

	disable_irq(foo->irq);

	spin_lock_irqsave(&foo->lock, flags);
	foo->stopped = true;
	spin_unlock_irqrestore(&foo->lock, flags);

	wake_up_interruptible(&foo->wq);
	device_remove_file(&pdev->dev, &dev_attr_wq_irq);

	dev_info(&pdev->dev, "driver is removed, irq cnt=%u, ready=%d\n",
		foo->irq_cnt, foo->ready);

	return 0;
}

static struct platform_driver foo_driver = {
	.probe  = foo_probe,
	.remove = foo_remove,
	.driver = {
		.name = "foo",
	},
};

module_platform_driver(foo_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxwell Li");
