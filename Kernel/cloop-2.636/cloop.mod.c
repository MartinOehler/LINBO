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
	{ 0x4e5560a5, "module_layout" },
	{ 0xe54efd5d, "blk_queue_merge_bvec" },
	{ 0x8e77a03, "blk_init_queue" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x7ab81152, "alloc_disk" },
	{ 0x77ecac9f, "zlib_inflateEnd" },
	{ 0x8d856fa1, "blk_cleanup_queue" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0xd0d8621b, "strlen" },
	{ 0x340eb29d, "blk_queue_max_hw_sectors" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0xb20aef06, "filp_close" },
	{ 0x999e8297, "vfree" },
	{ 0xd238eda3, "_lock_kernel" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0xee3b5d24, "invalidate_bdev" },
	{ 0xe174aa7, "__init_waitqueue_head" },
	{ 0x16a1c0b7, "blk_queue_max_segments" },
	{ 0xacc1ebd1, "param_ops_charp" },
	{ 0xfea6f45e, "vfs_read" },
	{ 0xe40b5068, "set_device_ro" },
	{ 0x88941a06, "_raw_spin_unlock_irqrestore" },
	{ 0xb3b36165, "current_task" },
	{ 0xb72397d5, "printk" },
	{ 0x65957039, "kthread_stop" },
	{ 0xd21e7a2e, "del_gendisk" },
	{ 0x54b14c1c, "kunmap" },
	{ 0xe4d1b49c, "blk_queue_segment_boundary" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0xce5ac24f, "zlib_inflate_workspacesize" },
	{ 0x71a50dbc, "register_blkdev" },
	{ 0x8904ddc2, "fput" },
	{ 0x881039d0, "zlib_inflate" },
	{ 0xb5a459dc, "unregister_blkdev" },
	{ 0x8ff4079b, "pv_irq_ops" },
	{ 0x67bd574d, "kmap" },
	{ 0x4292364c, "schedule" },
	{ 0xb0aa8375, "put_disk" },
	{ 0xf333a2fb, "_raw_spin_lock_irq" },
	{ 0xbf64c045, "blk_fetch_request" },
	{ 0xa463f216, "wake_up_process" },
	{ 0xea0d91e4, "__blk_end_request_all" },
	{ 0x587c70d8, "_raw_spin_lock_irqsave" },
	{ 0x4211c3c1, "zlib_inflateInit2" },
	{ 0xf09c7f68, "__wake_up" },
	{ 0xd2965f6f, "kthread_should_stop" },
	{ 0x37a0cba, "kfree" },
	{ 0xa91c031b, "kthread_create" },
	{ 0x3ed63055, "zlib_inflateReset" },
	{ 0xe75663a, "prepare_to_wait" },
	{ 0x9040db90, "add_disk" },
	{ 0xa89a15b5, "set_user_nice" },
	{ 0x45638c14, "fget" },
	{ 0xb00ccc33, "finish_wait" },
	{ 0xe2a92606, "blk_queue_max_segment_size" },
	{ 0x948c382b, "vfs_getattr" },
	{ 0x33d169c9, "_copy_from_user" },
	{ 0x13095525, "param_ops_uint" },
	{ 0xcabbb30c, "_unlock_kernel" },
	{ 0x5a2d45f7, "filp_open" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

