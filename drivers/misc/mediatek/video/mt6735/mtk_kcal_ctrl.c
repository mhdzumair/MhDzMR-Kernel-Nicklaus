/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "ddp_drv.h"
#include "primary_display.h"

#define MAX_LUT_SCALE 256
#define PROGRESSION_SCALE 128
static u32 mtk_disp_ld_r = MAX_LUT_SCALE;
static u32 mtk_disp_ld_g = MAX_LUT_SCALE;
static u32 mtk_disp_ld_b = MAX_LUT_SCALE;
static struct mtk_rgb_work_queue {
        struct work_struct work;
	struct mutex lock;
} mtk_rgb_work_queue;

static ssize_t kcal_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", mtk_disp_ld_r, mtk_disp_ld_g, mtk_disp_ld_b);
}

static ssize_t kcal_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int r = MAX_LUT_SCALE, g = MAX_LUT_SCALE, b = MAX_LUT_SCALE;

	if (count > 19)
		return -EINVAL;

	sscanf(buf, "%d %d %d", &r, &g, &b);

	if (r < 0 || r > MAX_LUT_SCALE) return -EINVAL;
	if (g < 0 || g > MAX_LUT_SCALE) return -EINVAL;
	if (b < 0 || b > MAX_LUT_SCALE) return -EINVAL;

	cancel_work_sync(&mtk_rgb_work_queue.work);
	mtk_disp_ld_r = r;
	mtk_disp_ld_g = g;
	mtk_disp_ld_b = b;
	schedule_work(&mtk_rgb_work_queue.work);

	return count;
}

static void mtk_disp_rgb_work(struct work_struct *work) {
        struct mtk_rgb_work_queue *rgb_wq = container_of(work, struct mtk_rgb_work_queue, work);
	int r = mtk_disp_ld_r, g = mtk_disp_ld_g, b = mtk_disp_ld_b;
	int i, gammutR, gammutG, gammutB, ret;
	DISP_GAMMA_LUT_T *gamma;

	mutex_lock(&rgb_wq->lock);

	gamma = kzalloc(sizeof(DISP_GAMMA_LUT_T), GFP_KERNEL);
	gamma->hw_id = 0;
	for (i = 0; i < 512; i++) {
		gammutR = i * r / PROGRESSION_SCALE;
		gammutG = i * g / PROGRESSION_SCALE;
		gammutB = i * b / PROGRESSION_SCALE;

		gamma->lut[i] = GAMMA_ENTRY(gammutR, gammutG, gammutB);
	}

	ret = primary_display_user_cmd(DISP_IOCTL_SET_GAMMALUT, (unsigned long)gamma);

	kfree(gamma);
	mutex_unlock(&rgb_wq->lock);
}

static DEVICE_ATTR(kcal, 0644, kcal_show, kcal_store);

static int kcal_ctrl_probe(struct platform_device *pdev)
{
	int ret;

	mutex_init(&mtk_rgb_work_queue.lock);
	INIT_WORK(&mtk_rgb_work_queue.work, mtk_disp_rgb_work);
	ret = device_create_file(&pdev->dev, &dev_attr_kcal);
	if (ret)
		pr_err("%s: unable to create sysfs entries\n", __func__);

	return ret;
}

static int kcal_ctrl_remove(struct platform_device *pdev)
{
	mutex_destroy(&mtk_rgb_work_queue.lock);
	device_remove_file(&pdev->dev, &dev_attr_kcal);

	return 0;
}

static struct platform_driver kcal_ctrl_driver = {
	.probe = kcal_ctrl_probe,
	.remove = kcal_ctrl_remove,
	.driver = {
		.name = "kcal_ctrl",
	},
};

static struct platform_device kcal_ctrl_device = {
	.name = "kcal_ctrl",
};

static int __init kcal_ctrl_init(void)
{
	if (platform_driver_register(&kcal_ctrl_driver))
		return -ENODEV;

	if (platform_device_register(&kcal_ctrl_device))
		return -ENODEV;

	pr_info("%s: registered\n", __func__);

	return 0;
}

static void __exit kcal_ctrl_exit(void)
{
	platform_device_unregister(&kcal_ctrl_device);
	platform_driver_unregister(&kcal_ctrl_driver);
}

late_initcall(kcal_ctrl_init);
module_exit(kcal_ctrl_exit);

MODULE_DESCRIPTION("MTK KCAL PCC Interface Driver");

