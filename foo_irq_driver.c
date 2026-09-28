#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

struct foo {
	struct device *dev;
	int irq;
	spinlock_t lock;
	unsigned int irq_cnt;
};

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;
		
	spin_lock(&foo->lock);
	++foo->irq_cnt;
	spin_unlock(&foo->lock);
	return IRQ_HANDLED;
}

static int foo_probe(struct platform_device *pdev)
{
	struct foo *foo;
	int ret;

	foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
	
	if(!foo)
		return -ENOMEM;

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0)
		return foo->irq;

	spin_lock_init(&foo->lock);

	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev,foo);

	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo", foo);
	if(ret)
		return ret;

	dev_info(&pdev->dev,"foo driver with irq %d is registered\n",foo->irq);
	
	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);
	dev_info(&pdev->dev, "foo driver is unregistered , irq conter: %d\n", foo->irq_cnt);
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

