#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

struct foo {
	struct device *dev;
	int irq;
	
	spinlock_t lock;
	unsigned int irq_cnt;
	
	struct work_struct work;
	unsigned int work_cnt;
	unsigned int work_queued_cnt;
};

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;
	
	spin_lock(&foo->lock);
	++foo->irq_cnt;
	spin_unlock(&foo->lock);

	if(schedule_work(&foo->work))
		++foo->work_queued_cnt;

	return IRQ_HANDLED;
}

static void foo_work_handler(struct work_struct *work)
{
	struct foo *foo = container_of(work, struct foo, work);
	
	unsigned long flags;
	unsigned int irq_cnt;
	
	msleep(300);
	
	spin_lock_irqsave(&foo->lock, flags);
	irq_cnt = foo->irq_cnt;
	spin_unlock_irqrestore(&foo->lock, flags);

	++foo->work_cnt;

	dev_info(foo->dev, "foo irq cnt: %u, work cnt: %u\n",irq_cnt, foo->work_cnt);
}

static int foo_probe(struct platform_device *pdev)
{
	struct foo *foo;
	int ret;

	foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
	if(!foo)
		return -ENOMEM;
	
	spin_lock_init(&foo->lock);
	INIT_WORK(&foo->work, foo_work_handler);
	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0)
		return foo->irq;
	
	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo", foo);
	if(ret)
		return ret;

	dev_info(&pdev->dev, "foo driver is registered\n");
	
	return 0;
}

static int foo_remove(struct platform_device *pdev)
{
	struct foo *foo = platform_get_drvdata(pdev);
	
	disable_irq(foo->irq);	
	cancel_work_sync(&foo->work);

	dev_info(&pdev->dev, "foo driver is unregistered,irq cnt=%u, work_cnt=%u, \
		work_queued_cnt=%u\n",foo->irq_cnt, foo->work_cnt, foo->work_queued_cnt);
	
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


