#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xe0f5cc03, "module_layout" },
	{ 0xc7eca37, "blk_queue_merge_bvec" },
	{ 0xe5adeddd, "blk_init_queue" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0xa54ebc4f, "alloc_disk" },
	{ 0x77ecac9f, "zlib_inflateEnd" },
	{ 0xf81e05d8, "blk_cleanup_queue" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0xd0d8621b, "strlen" },
	{ 0xe008b3c0, "blk_queue_max_hw_sectors" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x70defbdf, "filp_close" },
	{ 0xd5f697d2, "mutex_unlock" },
	{ 0x999e8297, "vfree" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0x7f27c8ea, "kthread_create_on_node" },
	{ 0x84aad565, "invalidate_bdev" },
	{ 0xe174aa7, "__init_waitqueue_head" },
	{ 0xb9020722, "blk_queue_max_segments" },
	{ 0xacc1ebd1, "param_ops_charp" },
	{ 0x2c23b2f9, "vfs_read" },
	{ 0xb9df2afa, "set_device_ro" },
	{ 0x2bc95bd4, "memset" },
	{ 0x88941a06, "_raw_spin_unlock_irqrestore" },
	{ 0x12dc87a3, "current_task" },
	{ 0x50eedeb8, "printk" },
	{ 0xcc04875f, "kthread_stop" },
	{ 0xa43f7d71, "del_gendisk" },
	{ 0xeb1d08ca, "kunmap" },
	{ 0x201e299f, "blk_queue_segment_boundary" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0xce5ac24f, "zlib_inflate_workspacesize" },
	{ 0x6f5427, "_raw_spin_unlock_irq" },
	{ 0xb29ec112, "mutex_lock" },
	{ 0x71a50dbc, "register_blkdev" },
	{ 0x9d797bcd, "fput" },
	{ 0x881039d0, "zlib_inflate" },
	{ 0xb5a459dc, "unregister_blkdev" },
	{ 0xee89c059, "kmap" },
	{ 0x4292364c, "schedule" },
	{ 0x7f92d01f, "put_disk" },
	{ 0xf333a2fb, "_raw_spin_lock_irq" },
	{ 0xcc8fc764, "blk_fetch_request" },
	{ 0xae542200, "wake_up_process" },
	{ 0x59110216, "__blk_end_request_all" },
	{ 0x587c70d8, "_raw_spin_lock_irqsave" },
	{ 0x4211c3c1, "zlib_inflateInit2" },
	{ 0xf09c7f68, "__wake_up" },
	{ 0xd2965f6f, "kthread_should_stop" },
	{ 0x37a0cba, "kfree" },
	{ 0x2e60bace, "memcpy" },
	{ 0x3ed63055, "zlib_inflateReset" },
	{ 0xe75663a, "prepare_to_wait" },
	{ 0xbdbaca9c, "add_disk" },
	{ 0xb67cce8e, "set_user_nice" },
	{ 0xec47a5e2, "fget" },
	{ 0xb00ccc33, "finish_wait" },
	{ 0x36a54bdc, "blk_queue_max_segment_size" },
	{ 0xa2394545, "vfs_getattr" },
	{ 0x33d169c9, "_copy_from_user" },
	{ 0x13095525, "param_ops_uint" },
	{ 0xce766008, "filp_open" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

