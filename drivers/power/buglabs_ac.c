#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#define AC_GPIO  43

static enum power_supply_property buglabs_ac_props[] =
{
	POWER_SUPPLY_PROP_ONLINE
};

static int buglabs_ac_get_property(struct power_supply *psy,
				      enum power_supply_property psp,
				      union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = gpio_get_value(AC_GPIO) ? 1 : 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static irqreturn_t buglabs_ac_irq_hdlr(int irq, void *data)
{
	struct power_supply *sply = data;

	power_supply_changed(sply);
	return(IRQ_HANDLED);
}

static int buglabs_ac_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct power_supply *sply;

	printk(KERN_INFO "Bug Labs AC Supply probe.\n");
	sply = kzalloc(sizeof(struct power_supply), GFP_KERNEL);
	if (sply == NULL)
		return(-ENOMEM);
	
	ret = gpio_request(AC_GPIO, "bug_ac");
	if (ret)
		printk(KERN_ERR "%s: Problem requesting GPIO...\n",__FUNCTION__);
	ret = gpio_direction_input(AC_GPIO);
	sply->type = POWER_SUPPLY_TYPE_MAINS;
	sply->name = "ac";
	sply->properties = buglabs_ac_props;
	sply->num_properties = ARRAY_SIZE(buglabs_ac_props);
	sply->get_property = buglabs_ac_get_property;
	
	ret = power_supply_register(&pdev->dev, sply);
	if (ret)
		dev_err(&pdev->dev, "Couldnt register supply.\n");

	ret = request_irq(gpio_to_irq(AC_GPIO), buglabs_ac_irq_hdlr,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "buglabs_ac", sply);
	platform_set_drvdata(pdev, sply);
	return(0);
}

static int buglabs_ac_remove(struct platform_device *pdev)
{
	struct power_supply *sply;

	sply = platform_get_drvdata(pdev);
	
	power_supply_unregister(sply);
	kfree(sply);

	return(0);
}

static struct platform_driver buglabs_ac_driver = {
	.probe		= buglabs_ac_probe,
	.remove		= buglabs_ac_remove,
	.driver		= {
		.name	= "buglabs_ac",
		.owner	= THIS_MODULE,
		},
};

static int __init buglabs_ac_init(void)
{
	platform_driver_register(&buglabs_ac_driver);
	return(0);
}

static void __exit buglabs_ac_exit(void)
{
	platform_driver_unregister(&buglabs_ac_driver);
}
module_init(buglabs_ac_init);
module_exit(buglabs_ac_exit);

MODULE_ALIAS("platform:buglabs_ac");
MODULE_DESCRIPTION("Buglabs Power supply driver");
MODULE_AUTHOR("Matt Isaacs");
MODULE_LICENSE("GPL");
