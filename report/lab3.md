## Lab3

> By：张子谦

### 思考题一

思考题 1: 内核从完成必要的初始化到用户态程序的过程是怎么样的？尝试描述一下调用关系。

执行的顺序大概如下：Chcore 启动后会依次初始化 `uart` 模块、内存管理模块、中断模块，然后调用 `process_create_root` 从而创建一个根进程。根进程的创建首先从磁盘中载入ELF文件，创建然后初始化进程，具体会先从磁盘中载入 ELF 文件，然后创建进程的 `process` 结构体并初始化，包括 `slot_table` 的初始化和分配一块虚拟地址空间 `vmspace`，最后为进程创建一个主线程。

`eret_to_thread` 则使用 `eret` 指令完成从内核模式到用户模式的切换，并在用户模式下开始运行用户代码。

```c
/*
 * @boot_flag is the physical address of boot flag;
 */
void main(paddr_t boot_flag)
{
        u32 ret = 0;

        /* Init uart: no need to init the uart again */
        uart_init();
        kinfo("[ChCore] uart init finished\n");

        /* Init mm */
        mm_init();
        kinfo("[ChCore] mm init finished\n");

        /* Init exception vector */
        arch_interrupt_init();
        kinfo("[ChCore] interrupt init finished\n");

        create_root_thread();
        kinfo("[ChCore] create initial thread done on %d\n", smp_get_cpu_id());

        /* Context switch to the picked thread */
        eret_to_thread(switch_context());

        /* Should provide panic and use here */
        BUG("[FATAL] Should never be here!\n");
}
```

