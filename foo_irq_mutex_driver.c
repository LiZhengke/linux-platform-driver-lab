#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct foo_dev {
	struct device *dev;
	int irq;
	u32 irq_cnt;

	spinlock_t lock;
	struct mutex mtx;
};

static u32 foo_irq_cnt(struct foo_dev *foo)
{
	u32 irq_cnt;
	unsigned long flags;

	spin_lock_irqsave(&foo->lock, flags);
	irq_cnt = foo->irq_cnt;
	spin_unlock_irqrestore(&foo->lock, flags);

	return irq_cnt;
}

static ssize_t status_show(struct device *dev,
						   struct device_attribute *attr,
                           char *buf)
{
	struct foo_dev *foo = dev_get_drvdata(dev);
	u32 irq_cnt;

	irq_cnt = foo_irq_cnt(foo);

	return sysfs_emit(buf, "irq cnt=%u\n", irq_cnt);
}
static DEVICE_ATTR_RO(status);

static ssize_t reset_store(struct device *dev,
                           struct device_attribute *attr,
                           const char* buf,
                           size_t count)
{
	struct foo_dev *foo = dev_get_drvdata(dev);
	unsigned long flags;
	bool reset;
	int ret;

	ret = kstrtobool(buf, &reset);
	if(ret)
		return ret;

	if(!reset)
		return count;

	ret = mutex_lock_interruptible(&foo->mtx);
	if(ret)
		return ret;

	disable_irq(foo->irq);

	dev_info(dev, "reset...\n");
	msleep(1000);

	spin_lock_irqsave(&foo->lock, flags);
	foo->irq_cnt = 0;
	spin_unlock_irqrestore(&foo->lock, flags);

	enable_irq(foo->irq);
	dev_info(dev, "reset done\n");
	mutex_unlock(&foo->mtx);

	return count;
}
static DEVICE_ATTR_WO(reset);

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo_dev *foo = data;

	spin_lock(&foo->lock);
	++foo->irq_cnt;
	spin_unlock(&foo->lock);

	return IRQ_HANDLED;
}

static int foo_probe(struct platform_device *pdev)
{
	struct foo_dev *foo;
	int ret;

	foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
	if(!foo)
		return -ENOMEM;

	spin_lock_init(&foo->lock);
	mutex_init(&foo->mtx);

	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0)
		return foo->irq;

	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo_dev", foo);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_status);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_reset);
	if(ret) {
		device_remove_file(&pdev->dev, &dev_attr_status);
		return ret;
	}

	dev_info(&pdev->dev, "driver is registered\n");
	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo_dev *foo = platform_get_drvdata(pdev);
	u32 irq_cnt;

	disable_irq(foo->irq);

	device_remove_file(&pdev->dev, &dev_attr_status);
	device_remove_file(&pdev->dev, &dev_attr_reset);

	irq_cnt = foo_irq_cnt(foo);
	dev_info(&pdev->dev, "driver is removed, irq cnt=%u\n", irq_cnt);
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
