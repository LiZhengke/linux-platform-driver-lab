#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/irq_sim.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

static struct platform_device *foo_pdev;
static struct irq_domain *foo_irq_domain;
static unsigned int foo_irq;

static ssize_t trigger_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf,
                             size_t count)
{
    bool fire;
    int ret;

    ret = kstrtobool(buf, &fire);
    if (ret)
        return ret;

    if (!fire)
        return count;

    ret = irq_set_irqchip_state(foo_irq,
                                IRQCHIP_STATE_PENDING,
                                true);
    if (ret)
        return ret;

    dev_info(dev, "fake IRQ %u triggered\n", foo_irq);

    return count;
}

static DEVICE_ATTR_WO(trigger);

static int __init foo_device_init(void)
{
	struct resource res;
	int ret;
	
	foo_irq_domain = irq_domain_create_sim(NULL, 1);
	if(IS_ERR(foo_irq_domain))
		return PTR_ERR(foo_irq_domain);
		
	foo_irq = irq_create_mapping(foo_irq_domain, 0);
	if(!foo_irq) {
		ret = -ENOMEM;
		goto err_domain;
	}
	
	pr_info("foo_device: simulated IRQ= %u\n", foo_irq);
	
	res = (struct resource) {
		.start = foo_irq,
		.end   = foo_irq,
		.flags = IORESOURCE_IRQ,
		.name  = "foo-irq",
	};
	
	foo_pdev = platform_device_register_simple("foo", PLATFORM_DEVID_NONE, &res, 1);
	if(IS_ERR(foo_pdev)) {
		ret = PTR_ERR(foo_pdev);
		goto err_mapping;
	}

	ret = device_create_file(&foo_pdev->dev, &dev_attr_trigger);
	if(ret)
		goto err_device_create;
		
	pr_info("foo_device: registered\n");

	return 0;

err_device_create:
	platform_device_unregister(foo_pdev);
	
err_mapping:
	irq_dispose_mapping(foo_irq);

err_domain:
	irq_domain_remove_sim(foo_irq_domain);
	
	return ret;
}

static void __exit foo_device_exit(void)
{
	device_remove_file(&foo_pdev->dev, &dev_attr_trigger);
	platform_device_unregister(foo_pdev);
	irq_dispose_mapping(foo_irq);
	irq_domain_remove_sim(foo_irq_domain);
	
	pr_info("foo_device: unregistered\n");
}

module_init(foo_device_init);
module_exit(foo_device_exit);

MODULE_LICENSE("GPL");

	
	
	
	

