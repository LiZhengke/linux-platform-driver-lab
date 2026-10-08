#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#define LIST_EVENT_SIZE 32

struct foo_event {
    struct list_head node;
    u32 seq;
};

struct foo_dev {
    struct device *dev;

    spinlock_t lock;
    int irq;
    u32 irq_cnt;

    struct list_head events;
    u32 event_cnt;

    u32 dropped_cnt;
};

static irqreturn_t foo_irq_handler(int irq, void *data)
{
    struct foo_dev *foo = data;

    struct foo_event *evt;
    struct foo_event *tmp;

    spin_lock(&foo->lock);
    ++foo->irq_cnt;

    if(foo->event_cnt >= LIST_EVENT_SIZE) {
        tmp = list_first_entry(&foo->events, struct foo_event, node);
        tmp->seq = foo->irq_cnt;
        list_move_tail(&tmp->node, &foo->events);
        spin_unlock(&foo->lock);

        return IRQ_HANDLED;
    } else {
        evt = kmalloc(sizeof(*evt), GFP_ATOMIC);
        if(!evt) {
            ++foo->dropped_cnt;
            spin_unlock(&foo->lock);
            return IRQ_HANDLED;
        }
        list_add_tail(&evt->node, &foo->events);
        evt->seq = foo->irq_cnt;
        ++foo->event_cnt;
    }

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

    ret  = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo_dev", foo);
    if(ret)
        return ret;

    dev_info(&pdev->dev, "driver is registered\n");
    return 0;
}

static void foo_remove(struct platform_device *pdev)
{
    struct foo_dev *foo = platform_get_drvdata(pdev);
    struct foo_event *event, *tmp;

    disable_irq(foo->irq);

    list_for_each_entry_safe(event, tmp, &foo->events, node) {
        list_del(&event->node);
        kfree(event);
    }

    dev_info(&pdev->dev, "driver is removed, irq_cnt=%u, event_cnt=%u, dropped_cnt=%u\n",
            foo->irq_cnt, foo->event_cnt, foo->dropped_cnt);
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
