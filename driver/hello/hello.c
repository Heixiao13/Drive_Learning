#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

int __init hello_init(void) {
    printk("hello_init!\n");
    return 0;
}

void __exit hello_exit(void) {
    printk("hello exit!\n");
}

module_init(hello_init);
module_exit(hello_exit);
