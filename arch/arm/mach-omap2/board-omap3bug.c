/*
 * linux/arch/arm/mach-omap2/board-omap3bug.c
 *
 * Copyright (C) 2008 Texas Instruments
 *
 * Modified from mach-omap2/board-3430sdp.c
 *
 * Initial code: Syed Mohammed Khasim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/leds.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>

#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/spi/sc16is.h>
#include <linux/i2c.h>
#include <linux/i2c/twl4030.h>
#include <linux/i2c/pca953x.h>
#include <linux/mmc/host.h>
#include <linux/bmi/omap_bmi.h>
#include <linux/leds_pwm.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/usb/android_composite.h>

#include <plat/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/gpio.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/common.h>
#include <plat/mcspi.h>
#include <plat/nand.h>
#include <plat/gpmc.h>
#include <plat/mux.h>
#include <plat/display.h>

#include "sdram-micron-mt46h32m32lf-6.h"
#include "mmc-twl4030.h"

#define GPMC_CS0_BASE  0x60
#define GPMC_CS_SIZE   0x30
#define MAX_USB_SERIAL_NUM			17
#define OMAP3_BUG_TS_GPIO			175
#define BUGBASE_VENDOR_ID			0x22B8
#define BUGBASE_PRODUCT_ID			0x41D9
#define BUGBASE_ADB_PRODUCT_ID			0x41DB
#define BUGBASE_RNDIS_PRODUCT_ID		0x41E4
#define BUGBASE_RNDIS_ADB_PRODUCT_ID		0x41E5
#define DIE_ID_REG_BASE			(L4_WK_34XX_PHYS + 0xA000)
#define DIE_ID_REG_OFFSET		0x218

static char device_serial[MAX_USB_SERIAL_NUM];

static int omap3bug_twl_gpio_setup(struct device *dev,
               unsigned gpio, unsigned ngpio);

static int omap3bug_ioexp_gpio_setup(struct i2c_client *client,
				     unsigned gpio, unsigned ngpio, void *context);
static int omap3bug_ioexp_gpio_teardown(struct i2c_client *client,
				     unsigned gpio, unsigned ngpio, void *context);

static int omap3bug_spi_uart_gpio_setup(struct spi_device *spi,
				     unsigned gpio, unsigned ngpio, void *context);

/*
static struct omap_uart_config omap3_bug_uart_config __initdata = {
	.enabled_uarts	= ((1 << 0) | (1 << 1) | (1 << 2)),
};
*/

static struct platform_device omap3_bug_dss_device;
static struct omap_dss_device omap3_bug_lcd_device;


#define NAND_BLOCK_SIZE		SZ_128K

static struct mtd_partition omap3bug_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader",
		.offset		= 0,
		.size		= 4 * NAND_BLOCK_SIZE,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x80000 */
		.size		= 15 * NAND_BLOCK_SIZE,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot Env",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x260000 */
		.size		= 1 * NAND_BLOCK_SIZE,
	},
	{
		.name		= "Kernel",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x280000 */
		.size		= 32 * NAND_BLOCK_SIZE,
	},
	{
		.name		= "File System",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x680000 */
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct omap_nand_platform_data omap3bug_nand_data = {
	.options	= NAND_BUSWIDTH_16,
	.parts		= omap3bug_nand_partitions,
	.nr_parts	= ARRAY_SIZE(omap3bug_nand_partitions),
	.dma_channel	= -1,		/* disable DMA in OMAP NAND driver */
	.nand_setup	= NULL,
	.dev_ready	= NULL,
};

static struct resource omap3bug_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device omap3bug_nand_device = {
	.name		= "omap2-nand",
	.id		= -1,
	.dev		= {
		.platform_data	= &omap3bug_nand_data,
	},
	.num_resources	= 1,
	.resource	= &omap3bug_nand_resource,
};

static struct regulator_consumer_supply bug_vmmc_supplies[] = {
	{
		.supply			= "vmmc",
	},
	{
		.supply			= "vmmc",
	},
	{
		.supply			= "vmmc",
	},
};

static struct regulator_consumer_supply bug_vaux2_supply = {
	.supply			= "vaux2",
};

/* VMMC1 for MMC1 pins CMD, CLK, DAT0..DAT3 (20 mA, plus card == max 220 mA) */
static struct regulator_init_data bug_vmmc1 = {
	.constraints = {
		.name			= "VMMC1",
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(bug_vmmc_supplies),
	.consumer_supplies	= bug_vmmc_supplies,
};

/* VAUX2 for USB PHY (max 100 mA) */
static struct regulator_init_data bug_vaux2 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.boot_on 		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &bug_vaux2_supply,
};

/* Supply enable for digital video outputs */
static struct regulator_consumer_supply bug_disp_supplies[] = {
  {
    .supply= "vdds_dsi",
    .dev= &omap3_bug_dss_device.dev,
  }
};

static struct regulator_init_data bug_disp_data = {
  .constraints = {
    .always_on = 1,
  },
  .num_consumer_supplies= ARRAY_SIZE(bug_disp_supplies),
  .consumer_supplies= bug_disp_supplies,

};

static struct fixed_voltage_config bug_disp_pwr_pdata = {
	.supply_name   = "VLCD",
	.microvolts    = 5000000,
	.init_data     = &bug_disp_data,
	.gpio          = -1,
};

static struct platform_device bug_disp_pwr = {
	.name          = "reg-fixed-voltage",
	.id            = -1,
	.dev = {
		.platform_data = &bug_disp_pwr_pdata,
	},
};

static struct twl4030_gpio_platform_data omap3bug_gpio_data = {
  .gpio_base	= OMAP_MAX_GPIO_LINES,
  .irq_base	= TWL4030_GPIO_IRQ_BASE,
  .irq_end	= TWL4030_GPIO_IRQ_END,
  .pulldowns    = BIT(2) | BIT(6) | BIT(8) | BIT(13)
  | BIT(16) | BIT(17),
  .setup        = omap3bug_twl_gpio_setup,
};

static struct twl4030_usb_data omap3bug_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static struct twl4030_codec_audio_data omap3bug_audio_data = {
	.audio_mclk = 26000000,
};

static struct twl4030_codec_data omap3bug_codec_data = {
	.audio_mclk = 26000000,
	.audio = &omap3bug_audio_data,
};

static struct twl4030_madc_platform_data omap3bug_madc_data = {
	.irq_line	= 1,
};

static int omap3bug_keymap[] = {
	KEY(0, 0, KEY_VIDEO_PREV),
};

static struct matrix_keymap_data omap3bug_keymap_data = {
	.keymap			= omap3bug_keymap,
	.keymap_size		= ARRAY_SIZE(omap3bug_keymap),
};

static struct twl4030_keypad_data omap3bug_kp_data = {
	.keymap_data	= &omap3bug_keymap_data,
	.rows		= 1,
	.cols		= 1,
	.rep		= 1,
};

static struct twl4030_platform_data omap3bug_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.keypad		= &omap3bug_kp_data,
	.madc		= &omap3bug_madc_data,
	.usb		= &omap3bug_usb_data,
	//.power	= GENERIC3430_T2SCRIPTS_DATA,
	.vmmc1		= &bug_vmmc1,
	.vaux2		= &bug_vaux2,
	.gpio		= &omap3bug_gpio_data,
};

#define GOOGLE_VENDOR_ID               0x18d1
#define GOOGLE_PRODUCT_ID              0x9018
#define GOOGLE_ADB_PRODUCT_ID          0x9015

static char *usb_functions_adb[] = {
	"adb",
};

static char *usb_functions_all[] = {
	"adb",
};

static struct android_usb_product usb_products[] = {
	{
		.product_id     = GOOGLE_PRODUCT_ID,
		.num_functions  = ARRAY_SIZE(usb_functions_adb),
		.functions      = usb_functions_adb,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id = GOOGLE_VENDOR_ID,
	.product_id = GOOGLE_PRODUCT_ID,
	.functions = usb_functions_all,
	.products = usb_products,
	.version = 0x0100,
	.product_name = "BUGBase",
	.manufacturer_name = "Buglabs",
	.serial_number = device_serial,
	.num_functions = ARRAY_SIZE(usb_functions_all),
};

static struct platform_device androidusb_device = {
	.name = "android_usb",
	.id = -1,
	.dev = {
		.platform_data = &android_usb_pdata,
	},
};

static void bugbase_gadget_init(void)
{
	unsigned int val[2];
	unsigned int reg;

	reg = DIE_ID_REG_BASE + DIE_ID_REG_OFFSET;
	val[0] = omap_readl(reg);
	val[1] = omap_readl(reg + 4);

	snprintf(device_serial, MAX_USB_SERIAL_NUM, "%08X%08X", val[1], val[0]);

	platform_device_register(&androidusb_device);
}

static struct sc16is_gpio_platform_data bugbase_spi_gpio = {
  .gpio_base	= OMAP_MAX_GPIO_LINES + TWL4030_GPIO_MAX + 16,
  .setup	= omap3bug_spi_uart_gpio_setup,
};


static struct sc16is_uart_platform_data bugbase_spi_uart = {
  .irq_pin = 36,
};

static struct sc16is_platform_data bugbase_sc_data = {
  .gpios = &bugbase_spi_gpio,
  .uarts = &bugbase_spi_uart,
};

static struct spi_board_info __initdata omap3bug_spi_board_info[] = {
  {
    .modalias                   = "sc16is",
    .bus_num                    = 1,
    .chip_select                = 0,
    .mode                       = SPI_MODE_0,
    .max_speed_hz               = 2000000,
    .platform_data              = &bugbase_sc_data,
  },
  {
    .modalias			= "spi-lcd",
    .bus_num			= 3,
    .chip_select		= 0,
    .max_speed_hz		= 1000000,
    .controller_data		= NULL,
    .platform_data 		= &omap3_bug_lcd_device, //&lcd_mcspi_config,
    .mode			= SPI_MODE_0,
  },
};

static struct pca953x_platform_data omap3bug_ioexp_data = {
  .gpio_base	= OMAP_MAX_GPIO_LINES + TWL4030_GPIO_MAX,
  .setup	= omap3bug_ioexp_gpio_setup,
  .teardown	= omap3bug_ioexp_gpio_teardown,
};
  
static struct i2c_board_info __initdata omap3bug_i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps65930", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &omap3bug_twldata,
	},
};

static struct i2c_board_info __initdata omap3bug_i2c2_boardinfo[] = {
	{
	  I2C_BOARD_INFO("pca9555", 0x20),
	  //.irq = gpio_to_irq(63),
	  .platform_data = &omap3bug_ioexp_data,
	},
	{
	  I2C_BOARD_INFO("bq27200", 0x55),
	},
};

static struct i2c_board_info __initdata omap3bug_i2c3_boardinfo[] = {
	{
	  I2C_BOARD_INFO("pca9546",  0x70),
	},
};


static int __init omap3_bug_i2c_init(void)
{
	omap_register_i2c_bus(1, 2600, omap3bug_i2c1_boardinfo,
			ARRAY_SIZE(omap3bug_i2c1_boardinfo));
	omap_register_i2c_bus(2, 100, omap3bug_i2c2_boardinfo,
			ARRAY_SIZE(omap3bug_i2c2_boardinfo));
	omap_register_i2c_bus(3, 100, omap3bug_i2c3_boardinfo, 
			ARRAY_SIZE(omap3bug_i2c3_boardinfo));
	return 0;
}


/*
 * For new frame buffer driver based on DSS2 library
 */

#ifdef CONFIG_FB_OMAP2
static struct resource omap3bug_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
};
#else
static struct resource omap3bug_vout_resource[2] = {
};
#endif


static struct platform_device omap3bug_vout_device = {
	.name			= "omap_vout",
	.num_resources	        = ARRAY_SIZE(omap3bug_vout_resource),
	.resource 		= &omap3bug_vout_resource[0],
	.id		        = -1,
};


#define LCD_PANEL_LR		2
#define LCD_PANEL_UD		3
#define LCD_PANEL_INI		152


#define VIDEO_PIM_SW_ENABLE     232
#define VIDEO_PIM_ENABLE        227
#define LCD_PANEL_QVGA		154
#define LCD_PANEL_RESB		155

#define ENABLE_VDAC_DEDICATED	0x03
#define ENABLE_VDAC_DEV_GRP	0x20
#define ENABLE_VPLL2_DEDICATED	0x05
#define ENABLE_VPLL2_DEV_GRP	0xE0

static void __init omap3_bug_display_init(void)
{
	int r;
/*
	r  = gpio_request(VIDEO_PIM_ENABLE, "lcd_power");
	r |= gpio_request(VIDEO_PIM_SW_ENABLE, "l
cd_level_shifter");
*/
	r |= gpio_request(90,  "lcd_shutdown");
	r |= gpio_request(93,  "lcd_reset");
	r |= gpio_request(10,  "dvi_reset");
	r |= gpio_request(92,  "acc_reset");
	if (r) {
	  printk(KERN_INFO "gpio request failed...\n");
	}
	return;
}

static int omap3_bug_panel_enable_lcd(struct omap_dss_device *display)
{
  	omap_cfg_reg (LCD_MCSPI3_CLK);
	omap_cfg_reg (LCD_MCSPI3_SIMO);
	omap_cfg_reg (LCD_SHUTDOWN);
	omap_cfg_reg (LCD_MCSPI3_CS);
	omap_cfg_reg (ACC_RESET);
	omap_cfg_reg (LCD_TP_RESET);
	omap_cfg_reg (ACC_INT);

/*
	gpio_direction_output(VIDEO_PIM_ENABLE, 1);
	gpio_direction_output(VIDEO_PIM_SW_ENABLE, 0);
*/
	gpio_direction_output(90,1);
	gpio_direction_output(92,1);

	return 0;
}

static void omap3_bug_panel_disable_lcd(struct omap_dss_device *display)
{
	//gpio_direction_output(VIDEO_PIM_SW_ENABLE, 1);

	// Mux these pins to safe mode
  	omap_cfg_reg (DSS_D18);
	omap_cfg_reg (DSS_D19);
	omap_cfg_reg (DSS_D20);
	omap_cfg_reg (DSS_D21);
	omap_cfg_reg (DSS_D21);
	omap_cfg_reg (DSS_D22);
	omap_cfg_reg (DSS_D23);

	return;
}


static struct omap_dss_device omap3_bug_lcd_device = {
	.type = OMAP_DISPLAY_TYPE_DPI,
	.name = "lcd",
	.driver_name = "sharp_spi_panel",
	.phy.dpi.data_lines = 18,
	.reset_gpio = 90,
	.platform_enable = omap3_bug_panel_enable_lcd,
	.platform_disable = omap3_bug_panel_disable_lcd,
};

static int omap3_bug_panel_enable_dvi(struct omap_dss_device *display)
{
	omap_cfg_reg (DSS_DATA_18);
	omap_cfg_reg (DSS_DATA_19);
	omap_cfg_reg (DSS_DATA_20);
	omap_cfg_reg (DSS_DATA_21);
	omap_cfg_reg (DSS_DATA_22);
	omap_cfg_reg (DSS_DATA_23);
	omap_cfg_reg (GPIO_10);
/*
	gpio_direction_output(VIDEO_PIM_ENABLE, 1);
	gpio_direction_output(VIDEO_PIM_SW_ENABLE, 0);
*/
	return 0;
}

static void omap3_bug_panel_disable_dvi(struct omap_dss_device *display)
{
	//gpio_direction_output(VIDEO_PIM_SW_ENABLE, 1);

	// Mux these pins to safe mode
  	omap_cfg_reg (DSS_D18);
	omap_cfg_reg (DSS_D19);
	omap_cfg_reg (DSS_D20);
	omap_cfg_reg (DSS_D21);

	omap_cfg_reg (DSS_D21);
	omap_cfg_reg (DSS_D22);
	omap_cfg_reg (DSS_D23);

	return;
}

static struct omap_dss_device omap3_bug_vga_device = {
	.type                = OMAP_DISPLAY_TYPE_DPI,
	.name                = "vga",
	.driver_name         = "vga_panel",
	.phy.dpi.data_lines  = 24,
	.platform_enable     = omap3_bug_panel_enable_dvi,
	.platform_disable    = omap3_bug_panel_disable_dvi,
};

static struct omap_dss_device omap3_bug_dvi_device = {
	.type                = OMAP_DISPLAY_TYPE_DPI,
	.name                = "dvi",
	.driver_name         = "generic_panel",
	.phy.dpi.data_lines  = 24,
	.platform_enable     = omap3_bug_panel_enable_dvi,
	.platform_disable    = omap3_bug_panel_disable_dvi,
};

struct omap_dss_device *omap3_bug_display_devices[] = {
        &omap3_bug_lcd_device,
	&omap3_bug_dvi_device,
	&omap3_bug_vga_device,
};

static struct omap_dss_board_info omap3_bug_dss_data = {
	.num_devices	     = ARRAY_SIZE(omap3_bug_display_devices),
	.devices	     = omap3_bug_display_devices,
	.default_device	     = &omap3_bug_dvi_device,
};

static struct platform_device omap3_bug_dss_device = {
	.name	 	     = "omapdss",
	.id		     = -1,
	.dev                 = {
		.platform_data = &omap3_bug_dss_data,
	},
};

static struct resource bmi_slot1_resources[] = {
  [0] = {
    .start = 16,
    .flags = IORESOURCE_IRQ,
  },
  [1] = {
    .start = 21,
    .flags = IORESOURCE_IRQ,
  },
};

static struct resource bmi_slot2_resources[] = {
  [0] = {
    .start = 14,
    .flags = IORESOURCE_IRQ,
  },
  [1] = {
    .start = 15,
    .flags = IORESOURCE_IRQ,
  },
};

static struct resource bmi_slot3_resources[] = {
  [0] = {
    .start = 22,
    .flags = IORESOURCE_IRQ,
  },
  [1] = {
    .start = 23,
    .flags = IORESOURCE_IRQ,
  },
};

static struct resource bmi_slot4_resources[] = {
  [0] = {
    .start = 12,
    .flags = IORESOURCE_IRQ,
  },
  [1] = {
    .start = 13,
    .flags = IORESOURCE_IRQ,
  },
};

static struct omap_bmi_platform_data bmi_slot_pdata1 = {
  .gpios = {218, 219, 220, 221},
  .i2c_bus_no = 4,
  .spi_cs = 4,  
};

static struct omap_bmi_platform_data bmi_slot_pdata2 = {
  .gpios = {-1,},
  .i2c_bus_no = 5,
  .spi_cs = -1,  
};

static struct omap_bmi_platform_data bmi_slot_pdata3 = {
  .gpios = {214, 215, 222, 223},
  .i2c_bus_no = 6,
  .spi_cs = 5,  
};

static struct omap_bmi_platform_data bmi_slot_pdata4 = {
  .gpios = {210, 211, 212, 213},
  .i2c_bus_no = 7,

  .spi_cs = 6,  
};

static struct platform_device bmi_slot_devices[] = {
  {
    .name = "omap_bmi_slot",
    .id = 0,
    .num_resources = ARRAY_SIZE(bmi_slot1_resources),
    .resource = bmi_slot1_resources,
    .dev = {
      .platform_data = &bmi_slot_pdata1,
    },
  },
  {
    .name = "omap_bmi_slot",
    .id = 1,
    .num_resources = ARRAY_SIZE(bmi_slot2_resources),
    .resource = bmi_slot2_resources,
    .dev = {
      .platform_data = &bmi_slot_pdata2,
    },
  },
  {
    .name = "omap_bmi_slot",
    .id = 2,
    .num_resources = ARRAY_SIZE(bmi_slot3_resources),
    .resource = bmi_slot3_resources,
    .dev = {
      .platform_data = &bmi_slot_pdata3,
    },
  },
  {
    .name = "omap_bmi_slot",
    .id = 3,
    .num_resources = ARRAY_SIZE(bmi_slot4_resources),
    .resource = bmi_slot4_resources,
    .dev = {
      .platform_data = &bmi_slot_pdata4,
    },
  },    
};


static void omap_init_bmi_slots(void)

{
  int i;

  //  gpio_direction_output(156, false);
  //  gpio_direction_output(159, false);

  for (i = 0; i < ARRAY_SIZE(bmi_slot_devices); i++) {
    if (platform_device_register(&bmi_slot_devices[i]) < 0)
      dev_err(&bmi_slot_devices[i].dev,
	      "Unable to register BMI slot\n");
  }
}

static struct resource omap3_bug_pwr_switch_resources[] = {
  [0] = {
    .start = TWL4030_PWR_IRQ_BASE,
    .flags = IORESOURCE_IRQ,
  },
};

static struct platform_device omap3_bug_pwr_switch = {
  .name = "twl4030_pwrbutton",
  .id = -1,
  .num_resources = ARRAY_SIZE(omap3_bug_pwr_switch_resources),
  .resource = omap3_bug_pwr_switch_resources,
};

static struct platform_device omap3_bug_pwm_a = {
  .name = "twl4030_pwm",
  .id = 0,
};

static struct platform_device omap3_bug_pwm_b = {
  .name = "twl4030_pwm",
  .id = 1,
};

static void __init omap3_bug_init_irq(void)
{
  omap2_init_common_hw(mt46h32m32lf6_sdrc_params, NULL);
  omap_init_irq();
  omap_gpio_init();
}

/*
 * On/Off LEDs available on OMAP.
 */
static struct gpio_led gpio_leds[] = {
		{
			.name		    = "omap3bug:green:battery",
			.default_trigger    = "none",
			.gpio		    = 53,
			.active_low         = true,
			.default_state      = LEDS_GPIO_DEFSTATE_OFF,
		},
		{
			.name		    = "omap3bug:red:battery",
			.default_trigger    = "none",
			.gpio		    = 54,
			.active_low         = 
true,
			.default_state      = LEDS_GPIO_DEFSTATE_OFF,
		},
/*
		{
			.name		    = "omap3bug:red:wlan",
			.default_trigger    = "none",
			.gpio		    = 39,
			.active_low         = false,
			.default_state      = LEDS_GPIO_DEFSTATE_OFF,
		},
		{
			.name		    = "omap3bug:green:wlan",
			.default_trigger    = "none",
			.gpio		    = 40,
			.active_low         = false,
			.default_state      = LEDS_GPIO_DEFSTATE_OFF,
		},
		{
			.name		    = "omap3bug:blue:wlan",
			.default_trigger    = "none",
			.gpio		    = 56,
			.active_low         = false,
			.default_state      = LEDS_GPIO_DEFSTATE_OFF,
		},
*/
};

static struct gpio_led_platform_data gpio_led_info = {
       .leds           = gpio_leds,
       .num_leds       = ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
       .name   = "leds-gpio",
       .id     = -1,
       .dev    = {
               .platform_data  = &gpio_led_info,
       },
};

/*
 * PWM LEDs available on TWL.
 */

static struct led_pwm pwm_leds[] =
{
		{
			.name               = "omap3bug:red:wifi",
			.default_trigger    = "phy0radio",
			.pwm_id             = 0,
			.active_low         = true,
			.max_brightness     = LED_FULL,
			.pwm_period_ns      = 330,
		},
		{
			.name               = "omap3bug:green:wifi",
			.default_trigger    = "phy0assoc",
			.pwm_id             = 1,
			.active_low         = true,
			.max_brightness     = LED_FULL,
			.pwm_period_ns      = 330,
		},
};

static struct led_pwm_platform_data pwm_led_info =
{
		.leds = pwm_leds,
		.num_leds = ARRAY_SIZE(pwm_leds),
};

static struct platform_device leds_pwm =
{

                .name = "leds_pwm",
		.id = -1,
		.dev =
		{
                                   .platform_data = &pwm_led_info,
		},
};

/*
 * PWM LEDs available on OMAP.
 */

static struct omap_pwm_led_platform_data omap_pwm_led_gpt8 = {
       .name                = "omap3bug:blue:bt",
       .intensity_timer     = 8,
       .blink_timer         = 0,
       //.default_trigger     = "none",
       //.set_power           = set_power(&omap_pwm_led_gpt92, 0),
};

static struct platform_device omap3_bug_pwm_gpt8 = {
       .name   = "omap_pwm_led",
       .id     = 0,
       .dev    = {
               .platform_data  = &omap_pwm_led_gpt8,
       },
};


static struct omap_pwm_led_platform_data omap_pwm_led_gpt9 = {
       .name                = "omap3bug:blue:battery",
       .intensity_timer     = 9,
       .blink_timer         = 0,
       //.default_trigger     = "none",
       //.set_power           = set_power(&omap_pwm_led_gpt92, 0),
};

static struct platform_device omap3_bug_pwm_gpt9 = {
       .name   = "omap_pwm_led",
       .id     = 1,
       .dev    = {
               .platform_data  = &omap_pwm_led_gpt9,
       },
};

static struct omap_pwm_led_platform_data omap_pwm_led_gpt10 = {
       .name                = "omap3bug:blue:wifi",
       .intensity_timer     = 10,
       .blink_timer         = 0,
       //.default_trigger     = "none",
       //.set_power           = set_power(&omap_pwm_led_gpt92, 0),
};

static struct platform_device omap3_bug_pwm_gpt10 = {
       .name   = "omap_pwm_led",
       .id     = 2,
       .dev    = {
               .platform_data  = &omap_pwm_led_gpt10,
       },
};

static struct omap_pwm_led_platform_data omap_pwm_led_gpt11 = {
       .name                = "omap3bug:blue:power",
       .intensity_timer     = 11,
       .blink_timer         = 0,
       //.default_trigger     = "breathe",
       //.set_power           = set_power(&omap_pwm_led_gpt92, 0),
};

static struct platform_device omap3_bug_pwm_gpt11 = {
       .name   = "omap_pwm_led",
       .id     = 3,
       .dev    = {
               .platform_data  = &omap_pwm_led_gpt11,
       },
};

static struct platform_device *omap3_bug_devices[] __initdata = {

  	&bug_disp_pwr,
	&omap3_bug_dss_device,
	&omap3bug_vout_device,
	&omap3_bug_pwr_switch,
	&omap3_bug_pwm_a,
	&omap3_bug_pwm_b,
	&omap3_bug_pwm_gpt8,
	&omap3_bug_pwm_gpt9,
	&omap3_bug_pwm_gpt10,
	&omap3_bug_pwm_gpt11,
	&leds_pwm,
	&leds_gpio
};


static struct twl4030_hsmmc_info mmc[] __initdata = {
	{
		.mmc		= 1,
		.wires		= 4,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{
	  .mmc = 2,
	  .wires = 4,
	  .gpio_cd	= 170,
	  //.gpio_wp	= 63,
	  .ocr_mask = MMC_VDD_32_33,
	},
	{
	  .mmc = 3,
	  .wires = 1,
	  .gpio_cd	= -EINVAL,
	  .gpio_wp	= -EINVAL,
	  .ocr_mask = MMC_VDD_165_195 | MMC_VDD_32_33,
	},
	{}	/* Terminator */
};

static int __init omap3bug_twl_gpio_setup(struct device *dev,
               unsigned gpio, unsigned ngpio)
{
       /* gpio + 0 is "mmc0_cd" (input/IRQ) */
       mmc[0].gpio_cd = gpio + 0;
       twl4030_mmc_init(mmc);

       bug_vmmc_supplies[0].dev = mmc[0].dev;
       bug_vmmc_supplies[1].dev = mmc[1].dev;
       bug_vmmc_supplies[2].dev = mmc[2].dev;

       /* Most GPIOs are for USB OTG.  Some are mostly sent to
        * the P2 connector; notably LEDA for the LCD backlight.
        */
	gpio_request(gpio + 1, "usb_hub");
	gpio_direction_output(gpio + 1, 1);
	gpio_free(gpio + 1);
       return 0;
}

static int omap3bug_ioexp_gpio_setup(struct i2c_client *client,
				     unsigned gpio, unsigned ngpio, void *context)
{
  int r;
  r =   gpio_request(gpio + 14, "lt_en");
  if (r) {
    printk(KERN_ERR "ioexp_gpio: failed to get lt_en...\n");
    return -1;
  }
  gpio_direction_output(gpio+14, 0);
  gpio_free(gpio + 14);
  return 0;
}

static int omap3bug_ioexp_gpio_teardown(struct i2c_client *client,
				     unsigned gpio, unsigned ngpio, void *context)
{
  int r;
  r =  gpio_direction_output(gpio+14, 1);
  if (r) {
    printk(KERN_ERR "ioexp_gpio: failed to reset lt_en...\n");
    return -1;
  }
  gpio_free(gpio+14);
  return 0;
}

static int omap3bug_spi_uart_gpio_setup(struct spi_device *spi, unsigned gpio, unsigned ngpio, void *context)
{
	int r;
  
	printk(KERN_INFO "spi_uart_gpio: Setting up gpios...\n");
	omap3_bug_display_init();
	r =   gpio_request(gpio + 4, "wifi_en");  
	if (r) {
	  printk(KERN_ERR "spi_uart_gpio: failed to get wifi_en...\n");
	  return r;
	}
	gpio_direction_output(gpio+4, 1);

	mdelay(100);
	r =   gpio_request(157, "wifi_rst");
	if (r) {
	  printk(KERN_ERR "spi_uart_gpio: failed to get wifi_rst...\n");
	  return r;
	}
	gpio_direction_output(157, 1);

	r =   gpio_request(156, "bt_rst");
	if (r) {
	  printk(KERN_ERR "spi_uart_gpio: failed to get bt_rst...\n");
	  return r;
	}
	gpio_direction_output(156, 1);

	r =   gpio_request(163, "wifi_wakeup");
	if (r) {
	  printk(KERN_ERR "spi_uart_gpio: failed to get wifi_wakeup...\n");
	  return r;
	}
	gpio_direction_output(163, 0);
	  
	r =   gpio_request(233, "5V_en");
	gpio_direction_output(233,1);
	gpio_free(233);
	mdelay(100);
	gpio_set_value (163, 1);
	gpio_set_value (157, 0);
  
	mdelay(100);
	gpio_set_value (157, 1);
	gpio_set_value (156, 0);
	mdelay(100);
	gpio_set_value (156, 1);

	printk(KERN_INFO "spi_uart_gpio: Freeing gpios...");
	gpio_free(230);
	gpio_free(156);
	gpio_free(157);
	gpio_free(163);
	return 0;
}

#define TWL4030_VAUX2_1P8V 0x5
#define ENABLE_VAUX2_DEV_GRP 0x20

/* This is called from twl4030-core.c and is required by
 * MUSB and EHCI on new OMAP3BUG.
 */
void usb_gpio_settings(void)
{
	unsigned char val;

	/* enable VAUX2 for EHCI */
	/*
	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			TWL4030_VAUX2_1P8V, TWL4030_VAUX2_DEDICATED);
	twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
			ENABLE_VAUX2_DEV_GRP, TWL4030_VAUX2_DEV_GRP);
	*/

	/* Enable TWL GPIO Module */
	twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, 0x04, REG_GPIO_CTRL);

	/*
	 * Configure GPIO-6 as output
	 */
	twl4030_i2c_read_u8(TWL4030_MODULE_GPIO, &val, REG_GPIODATADIR1);
	val |= 0x4;
	twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, val, REG_GPIODATADIR1);

	/* Set GPIO6 = 1 */
	twl4030_i2c_read_u8(TWL4030_MODULE_GPIO, &val, REG_GPIODATAOUT1);
	val |= 0x40;
	twl4030_i2c_write_u8(TWL4030_MODULE_GPIO, val, REG_GPIODATAOUT1);


}
//EXPORT_SYMBOL(usb_gpio_settings);

void gen_gpio_settings(void)
{
  int r;
  r =   gpio_request(107, "dock_rst");
  if (r) {
    printk(KERN_ERR "gen_gpio: failed to get dock_rst...\n");
    return;
  }
  gpio_direction_output(107, 1);

  r =   gpio_request(42, "spi_uart_rst");
  if (r) {
    printk(KERN_ERR "gen_gpio: failed to get spi_uart_rst...\n");
    return;
  }
  gpio_direction_output(42, 1);


  r =   gpio_request(109, "twl_msecure");
  if (r) {
    printk(KERN_ERR "gen_gpio: failed to get twl_msecure...\n");
    return;
  }
  gpio_direction_output(109, 1);

  r =   gpio_request(35, "mmc1_enable");
  if (r) {
    printk(KERN_ERR "gen_gpio: failed to get mmc1_enable...\n");
    return;
  }
  gpio_direction_output(35, 1);

  r =   gpio_request(108, "audio_mute");
  if (r) {
    printk(KERN_ERR "gen_gpio: failed to get audio_mute...\n");
    return;
  }
  gpio_direction_output(108, 1);

  return;
  
}

static void __init omap3bug_flash_init(void)
{
	u8 cs = 0;
	u8 nandcs = GPMC_CS_NUM + 1;

	u32 gpmc_base_add = OMAP34XX_GPMC_VIRT;

	/* find out the chip-select on which NAND exists */
	while (cs < GPMC_CS_NUM) {
		u32 ret = 0;
		ret = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG1);

		if ((ret & 0xC00) == 0x800) {
			printk(KERN_INFO "Found NAND on CS%d\n", cs);
			if (nandcs > GPMC_CS_NUM)
				nandcs = cs;
		}
		cs++;
	}

	if (nandcs > GPMC_CS_NUM) {
		printk(KERN_INFO "NAND: Unable to find configuration "
				 "in GPMC\n ");
		return;
	}

	if (nandcs < GPMC_CS_NUM) {
		omap3bug_nand_data.cs = nandcs;
		omap3bug_nand_data.gpmc_cs_baseaddr = (void *)
			(gpmc_base_add + GPMC_CS0_BASE + nandcs * GPMC_CS_SIZE);
		omap3bug_nand_data.gpmc_baseaddr = (void *) (gpmc_base_add);

		printk(KERN_INFO "Registering NAND on CS%d\n", nandcs);
		if (platform_device_register(&omap3bug_nand_device) < 0)
			printk(KERN_ERR "Unable to register NAND device\n");
	}
}

static struct ehci_hcd_omap_platform_data ehci_pdata __initconst = {

	.port_mode[0] = EHCI_HCD_OMAP_MODE_UNKNOWN,
	.port_mode[1] = EHCI_HCD_OMAP_MODE_UNKNOWN,
	.port_mode[2] = EHCI_HCD_OMAP_MODE_UNKNOWN,

	//.chargepump = false,
	.phy_reset  = true,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = 126,
	.reset_gpio_port[2]  = -EINVAL
};


static void __init omap3_bug_init(void)
{

	/* Get BUG board version and save it */
        //omap3bug_board_rev();
        printk(KERN_INFO "BUGBASE: Init i2c..\n");
	omap3_bug_i2c_init();
        printk(KERN_INFO "BUGBASE: Init spi..\n");
	spi_register_board_info(omap3bug_spi_board_info,
				ARRAY_SIZE(omap3bug_spi_board_info));
	omap_serial_init();
	platform_add_devices(omap3_bug_devices, ARRAY_SIZE(omap3_bug_devices));
	//omap_init_twl4030();
	usb_gpio_settings();
	usb_musb_init();
	bugbase_gadget_init();
	usb_ehci_init(&ehci_pdata);
	gen_gpio_settings();

	omap3bug_flash_init();
	omap_init_bmi_slots();

	/* Pin Mux - Set T8 to GPT9_PWM_EVT */
	// For LED - should probably be moved into uboot
	omap_cfg_reg(T8_34XX_GPIO55_OUT);

}

static void __init omap3_bug_map_io(void)
{
	omap2_set_globals_343x();
	omap2_map_common_io();
}

//MACHINE_START(BUG, "OMAP3 BUG")
MACHINE_START(OMAP3EVM, "OMAP3 BUG")
	/* Maintainer: Matt Isaacs - BugLabs, inc */
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xd8000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap3_bug_map_io,
	.init_irq	= omap3_bug_init_irq,
	.init_machine	= omap3_bug_init,
	.timer		= &omap_timer,
MACHINE_END
