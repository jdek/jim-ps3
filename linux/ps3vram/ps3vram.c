/**
 * ps3vram.c -- use extra PS3 video ram as MTD block device
 *
 * Copyright (c) 2007           Jim Paris <jim@jtan.com>
 * 
 * Based on phram.c:
 * Copyright (c) ????		Jochen Schäuble <psionic@psionic.de>
 * Copyright (c) 2003-2004	Jörn Engel <joern@wh.fh-wedel.de>
 *
 */
#include <asm/io.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/version.h>
#include <asm/lv1call.h>

#ifndef MTD_ERASEABLE
#define MTD_ERASEABLE 0
#endif

#ifndef MTD_VOLATILE
#define MTD_VOLATILE 0
#endif


struct mtd_info ps3vram_mtd;

struct ps3vram_priv {
	uint64_t memory_handle;
	uint8_t *base;
	uint8_t *real_base;
};

static int ps3vram_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct ps3vram_priv *priv = mtd->priv;

	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

	/* Set bytes to 0xFF */
	memset(priv->base + instr->addr, 0xFF, instr->len);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}


static int ps3vram_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	struct ps3vram_priv *priv = mtd->priv;

	if (from >= mtd->size)
		return -EINVAL;

	if (len > mtd->size - from)
		len = mtd->size - from;

	/* Copy from vram to buf */
	memcpy(buf, priv->base + from, len);

	*retlen = len;
	return 0;
}

static int ps3vram_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, const u_char *buf)
{
	struct ps3vram_priv *priv = mtd->priv;

	if (to >= mtd->size)
		return -EINVAL;

	if (len > mtd->size - to)
		len = mtd->size - to;

	/* Copy from buf to vram */
	memcpy(priv->base + to, buf, len);

	*retlen = len;
	return 0;
}

static void unregister_devices(void)
{
	struct ps3vram_priv *priv;

	priv = ps3vram_mtd.priv;
	del_mtd_device(&ps3vram_mtd);
	iounmap(priv->real_base);
	lv1_gpu_memory_free(priv->memory_handle);
	kfree(priv);

	printk(KERN_INFO "ps3vram mtd device unregistered\n");
}

static int register_device(void)
{
	struct ps3vram_priv *priv;
	uint64_t status, ddr_lpar, ddr_size;
	int ret = -ENOMEM;

	ret = -EIO;
	ps3vram_mtd.priv = kzalloc(sizeof(struct ps3vram_priv), GFP_KERNEL);
	if (!ps3vram_mtd.priv)
		goto out0;
	priv = ps3vram_mtd.priv;
	
	/* Request memory */
	ddr_size = 0x0fc00000;  /* XXX 252 MB */
	status = lv1_gpu_memory_allocate(ddr_size, 0, 0, 0, 0, 
					 &priv->memory_handle, &ddr_lpar);
	if (status != 0) {
		printk(KERN_ERR "ps3vram: lv1_gpu_memory_allocate failed\n");
		goto out1;		
	}

	priv->base = priv->real_base = ioremap(ddr_lpar, ddr_size);
	if (!priv->real_base) {
		printk(KERN_ERR "ps3vram: ioremap failed\n");
		goto out2;
	}
	
	/* XXX: Skip beginning GDDR ram that might belong to the framebuffer. */
#define SKIP_SIZE ((1920*1080*4)*2)
	priv->base += SKIP_SIZE;
	ddr_size -= SKIP_SIZE;

	ps3vram_mtd.name = "ps3vram";
	ps3vram_mtd.size = ddr_size;
	ps3vram_mtd.flags = MTD_CAP_RAM | MTD_ERASEABLE | MTD_VOLATILE;
        ps3vram_mtd.erase = ps3vram_erase;
	ps3vram_mtd.point = NULL;
	ps3vram_mtd.unpoint = NULL;
	ps3vram_mtd.read = ps3vram_read;
	ps3vram_mtd.write = ps3vram_write;
	ps3vram_mtd.owner = THIS_MODULE;
	ps3vram_mtd.type = MTD_RAM;
	ps3vram_mtd.erasesize = PAGE_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
	ps3vram_mtd.writesize = 1;
#endif

	ret = -EAGAIN;
	if (add_mtd_device(&ps3vram_mtd)) {
		printk(KERN_ERR "ps3vram: failed to register device\n");
		goto out3;
	}

	printk(KERN_INFO "ps3vram mtd device registered, %ld bytes\n", ddr_size);

	return 0;

out3:
	iounmap(priv->real_base);
out2:
	lv1_gpu_memory_free(priv->memory_handle);
out1:
	kfree(ps3vram_mtd.priv);
out0:
	return ret;
}

static int __init init_ps3vram(void)
{
	register_device();
	return 0;
}

static void __exit cleanup_ps3vram(void)
{
	unregister_devices();
}

module_init(init_ps3vram);
module_exit(cleanup_ps3vram);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jim Paris <jim@jtan.com>");
MODULE_DESCRIPTION("MTD driver for PS3 GDDR video RAM");
