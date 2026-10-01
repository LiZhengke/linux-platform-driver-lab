#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct foo {
	struct device *dev;
	spinlock_t lock;

	int irq;
	unsigned int irq_cnt;
	unsigned int thread_irq_cnt;
};

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;

	spin_lock(&foo->lock);
	++foo->irq_cnt;
	spin_unlock(&foo->lock);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t foo_irq_thread_fn(int irq, void *data)
{
	struct foo *foo = data;
	unsigned long flags;
	unsigned int irq_cnt;
	unsigned int thread_cnt;

	spin_lock_irqsave(&foo->lock, flags);
	++foo->thread_irq_cnt;
	irq_cnt = foo->irq_cnt;
	thread_cnt = foo->thread_irq_cnt;
	spin_unlock_irqrestore(&foo->lock, flags);

	msleep(300);
	dev_info(foo->dev, "irq=%u, thread_cnt=%u\n", irq_cnt, thread_cnt);

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

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0) {
		return foo->irq;
	}

	spin_lock_init(&foo->lock);

	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	ret = devm_request_threaded_irq(&pdev->dev, foo->irq, foo_irq_handler,
		foo_irq_thread_fn, IRQF_ONESHOT, "foo", foo);
	if(ret)
		return ret;

	dev_info(&pdev->dev, "driver is registered\n");

	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);

	disable_irq(foo->irq);

	dev_info(&pdev->dev, "drvier is unregistered,hard_irq_cnt=%u,thread_runs=%u\n",
		foo->irq_cnt, foo->thread_irq_cnt);

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

