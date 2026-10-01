#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kfifo.h>

#define FIFO_SIZE 32

struct foo_event {
	unsigned int seq;
};

struct foo {
	struct device *dev;
	int irq;

	spinlock_t lock;
	unsigned int irq_cnt;
	unsigned int dropped_cnt;

	struct work_struct work;
	unsigned int work_cnt;
	atomic_t work_queued_cnt;

	DECLARE_KFIFO(fifo, struct foo_event, FIFO_SIZE);
};

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo *foo = data;

	spin_lock(&foo->lock);

	++foo->irq_cnt;
	struct foo_event evt ={
		.seq = foo->irq_cnt,
	};

	if(!kfifo_put(&foo->fifo, evt))
		++foo->dropped_cnt;

	spin_unlock(&foo->lock);

	if(schedule_work(&foo->work))
		atomic_inc(&foo->work_queued_cnt);

	return IRQ_HANDLED;
}

static void foo_work_handler(struct work_struct *work)
{
	struct foo *foo = container_of(work, struct foo, work);

	unsigned long flags;
	unsigned int irq_cnt;

	struct foo_event evt;
	unsigned int evt_cnt;

	for(;;) {
		spin_lock_irqsave(&foo->lock, flags);
		irq_cnt = foo->irq_cnt;
		evt_cnt = kfifo_get(&foo->fifo, &evt);
		spin_unlock_irqrestore(&foo->lock, flags);

		if(!evt_cnt)
			break;

		++foo->work_cnt;

		dev_info(foo->dev, "foo irq cnt: %u, work cnt: %u, evt->seq=%u\n",
			irq_cnt, foo->work_cnt, evt.seq);
		msleep(300);
	}
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
	INIT_KFIFO(foo->fifo);
	atomic_set(&foo->work_queued_cnt, 0);

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
	flush_work(&foo->work);

	dev_info(&pdev->dev,
		"foo driver is unregistered,irq=%u, processed=%u, dropped=%u, queued=%u\n"
		,foo->irq_cnt, foo->work_cnt, foo->dropped_cnt, atomic_read(&foo->work_queued_cnt));

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


