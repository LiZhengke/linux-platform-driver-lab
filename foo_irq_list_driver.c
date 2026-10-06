#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

struct foo_event {
	struct list_head node;
	u32 seq;
};

struct foo_dev {
	struct device *dev;
	int irq;

	u32 irq_cnt;
	spinlock_t lock;

	struct list_head events;
	u32 dropped_cnt;
	u32 event_cnt;
};

static ssize_t events_show(struct device *dev,
		           struct device_attribute *attr,
			   char *buf)
{
	struct foo_dev *foo = dev_get_drvdata(dev);
	unsigned long flags;
	u32 irq_cnt;
	u32 event_cnt;
	u32 dropped_cnt;

	spin_lock_irqsave(&foo->lock, flags);

	irq_cnt = foo->irq_cnt;
	event_cnt = foo->event_cnt;
	dropped_cnt = foo->dropped_cnt;

	spin_unlock_irqrestore(&foo->lock, flags);

	return sysfs_emit(buf, "irq_cnt=%u, event_cnt=%u,dropped_cnt=%u\n",
			irq_cnt, event_cnt, dropped_cnt);
}

static DEVICE_ATTR_RO(events);

static irqreturn_t foo_irq_handler(int irq, void *data)
{
	struct foo_dev *foo = data;
	struct foo_event *event;

	event = kmalloc(sizeof(*event), GFP_ATOMIC);

	spin_lock(&foo->lock);

	++foo->irq_cnt;

	if(!event) {
		++foo->dropped_cnt;
		spin_unlock(&foo->lock);
		return IRQ_HANDLED;
	}

	event->seq = foo->irq_cnt;

	list_add_tail(&event->node, &foo->events);
	++foo->event_cnt;

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
	INIT_LIST_HEAD(&foo->events);
	foo->dev = &pdev->dev;
	platform_set_drvdata(pdev, foo);

	foo->irq = platform_get_irq(pdev, 0);
	if(foo->irq < 0)
		return foo->irq;

	ret = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo_dev", foo);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_events);
	if(ret)
		return ret;

	dev_info(&pdev->dev, "driver is registered.\n");

	return 0;
}

static void foo_remove(struct platform_device *pdev)
{
	struct foo_dev *foo = platform_get_drvdata(pdev);
	struct foo_event *evt, *tmp;

	device_remove_file(&pdev->dev, &dev_attr_events);

	disable_irq(foo->irq);

	list_for_each_entry_safe(evt, tmp, &foo->events, node) {
		list_del(&evt->node);
		kfree(evt);
	}
}

static struct platform_driver foo_driver = {
	.probe = foo_probe,
	.remove_new = foo_remove,
	.driver = {
		.name = "foo",
	},
};

module_platform_driver(foo_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxwell Li");
