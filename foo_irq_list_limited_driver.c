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
    struct list_head free_events;

    struct foo_event *event_pool;
    u32 event_cnt;

    u32 overwritten_cnt;
};

static ssize_t events_show(struct device *dev,
        struct device_attribute *att, char *buf)
{
    struct foo_dev *foo = dev_get_drvdata(dev);
    unsigned long flags;
    struct foo_event *evt;
    u32 irq_cnt, event_cnt, overwritten_cnt;
    u32 seq[LIST_EVENT_SIZE];
    u32 n=0, i;
    int len;

    spin_lock_irqsave(&foo->lock, flags);

    irq_cnt = foo->irq_cnt;
    event_cnt = foo->event_cnt;
    overwritten_cnt = foo->overwritten_cnt;

    list_for_each_entry(evt, &foo->events, node) {
        if(n < LIST_EVENT_SIZE)
            seq[n++] = evt->seq;
    }
    spin_unlock_irqrestore(&foo->lock, flags);

    len = sysfs_emit(buf, "irq_cnt=%u, event_cnt=%u, overwritten_cnt=%u\n",
            irq_cnt, event_cnt, overwritten_cnt);

    len += sysfs_emit_at(buf, len, "seq:");

    for(i = 0; i < n; ++i)
        len += sysfs_emit_at(buf, len, " %u", seq[i]);

    len += sysfs_emit_at(buf, len, "\n");
    return len;
}

static DEVICE_ATTR_RO(events);

static irqreturn_t foo_irq_handler(int irq, void *data)
{
    struct foo_dev *foo = data;
    struct foo_event *evt;

    spin_lock(&foo->lock);

    ++foo->irq_cnt;

    if(!list_empty(&foo->free_events)) {
        evt = list_first_entry(&foo->free_events, struct foo_event, node);
        list_move_tail(&evt->node, &foo->events);
        ++foo->event_cnt;
    } else {
        evt = list_first_entry(&foo->events, struct foo_event, node);
        list_move_tail(&evt->node, &foo->events);
        ++foo->overwritten_cnt;
    }
    evt->seq = foo->irq_cnt;

    spin_unlock(&foo->lock);

    return IRQ_HANDLED;
}

static int foo_probe(struct platform_device *pdev)
{
    struct foo_dev *foo;
    int ret;
    int i;

    foo = devm_kzalloc(&pdev->dev, sizeof(*foo), GFP_KERNEL);
    if(!foo)
        return -ENOMEM;

    spin_lock_init(&foo->lock);

    foo->event_pool = devm_kcalloc(&pdev->dev, LIST_EVENT_SIZE, sizeof(struct foo_event), GFP_KERNEL);
    if(!foo->event_pool)
        return -ENOMEM;

    INIT_LIST_HEAD(&foo->events);
    INIT_LIST_HEAD(&foo->free_events);

    for(i = 0; i < LIST_EVENT_SIZE; ++i)
        list_add_tail(&foo->event_pool[i].node, &foo->free_events);

    foo->dev = &pdev->dev;
    platform_set_drvdata(pdev, foo);

    ret = device_create_file(&pdev->dev, &dev_attr_events);
    if(ret)
        return ret;

    foo->irq = platform_get_irq(pdev, 0);
    if(foo->irq < 0)
        goto err_pirq;

    ret  = devm_request_irq(&pdev->dev, foo->irq, foo_irq_handler, 0, "foo_dev", foo);
    if(ret)
        goto err_rirq;

    dev_info(&pdev->dev, "driver is registered\n");

    return 0;

err_pirq:
    ret = foo->irq;
err_rirq:
    device_remove_file(&pdev->dev, &dev_attr_events);
    return ret;
}

static void foo_remove(struct platform_device *pdev)
{
    struct foo_dev *foo = platform_get_drvdata(pdev);

    devm_free_irq(&pdev->dev, foo->irq, foo);

    device_remove_file(&pdev->dev, &dev_attr_events);

    dev_info(&pdev->dev, "driver is removed, irq_cnt=%u, event_cnt=%u, overwritten_cnt=%u\n",
            foo->irq_cnt, foo->event_cnt, foo->overwritten_cnt);
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
