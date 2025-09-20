#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me");
MODULE_DESCRIPTION("Fun with Descriptors...");

static int __init dscfun_init(void)
{
    pr_info("dscfun: module loaded\n");
    pr_info("dscfun: Great success!\n");

    return 0;
}

static void __exit dscfun_exit(void)
{
    pr_info("dscfun: module unloaded\n");
}

module_init(dscfun_init);
module_exit(dscfun_exit);
