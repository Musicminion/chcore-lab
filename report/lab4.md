## Lab4

> By：张子谦

### 思考题1

> 思考题 1：阅读汇编代码`kernel/arch/aarch64/boot/raspi3/init/start.S`。说明ChCore是如何选定主CPU，并阻塞其他其他CPU的执行的。

如下代码所示：

```
	mrs	x8, mpidr_el1
	and	x8, x8,	#0xFF
	cbz	x8, primary
```

- 首先ChCore先从`mpidr_el1`，获得CPU的ID
- cpu id为0的CPU是主CPU，然后跳转到primary中执行，但是其他的CPU核心不会跳转，进入到wait_until_smp_enabled循环
- 当secondary_boot_flag被设置为0后，然后就结束循环

### 思考题2

> 思考题 2：阅读汇编代码`kernel/arch/aarch64/boot/raspi3/init/start.S, init_c.c`以及`kernel/arch/aarch64/main.c`，解释用于阻塞其他CPU核心的`secondary_boot_flag`是物理地址还是虚拟地址？是如何传入函数`enable_smp_cores`中，又该如何赋值的（考虑虚拟地址/物理地址）？

- secondary_boot_flag在主cpu（id为0）中是虚拟地址，在其他cpu中是物理地址，因为其他cpu的mmu还没有开启，而主CPU打开了MMU。
- `init_c`函数中调用了`start_kernel(secondary_boot_flag);`
- 主CPU运行`start_kernel`函数时会调用`main`函数，会把`secondary_boot_flag`传入到`main`函数中

```
    /* Restore x0 */
    ldr     x0, [sp], #8
    bl      main
END_FUNC(start_kernel)
```

- 最后`main`函数将`secondary_boot_flag`传入到`enable_smp_cores`中

```
        /* Other cores are busy looping on the addr, wake up those cores */
        enable_smp_cores(boot_flag);
        kinfo("[ChCore] boot multicore finished\n");
```

`secondary_boot_flag`是一个数组，下标就是cpu的id，然后根据CPU的核数来依次对其进行赋值。刚传入这个enable_smp_cores函数的时候是物理地址，然后通过phys_to_virt转化为虚拟地址（主cpu开启mmu）

```
void enable_smp_cores(paddr_t boot_flag)
{
        int i = 0;
        long *secondary_boot_flag;

        /* Set current cpu status */
        cpu_status[smp_get_cpu_id()] = cpu_run;
        secondary_boot_flag = (long *)phys_to_virt(boot_flag);
        for (i = 0; i < PLAT_CPU_NUM; i++) {
                /* Lab4
                 * You should set one flag to enable the APs to continue in
                 * _start. Then, what's the flag?
                 */
                /* LAB 4 TODO BEGIN */

                /* LAB 4 TODO END */

                flush_dcache_area((u64)secondary_boot_flag,
                                  (u64)sizeof(u64) * PLAT_CPU_NUM);
                asm volatile("dsb sy");

                /* Lab4
                 * The BSP waits for the currently initializing AP finishing
                 * before activating the next one
                 */
                /* LAB 4 TODO BEGIN */

                /* LAB 4 TODO END */
                if (cpu_status[i] == cpu_run)
                        kinfo("CPU %d is active\n", i);
                else
                        BUG("CPU %d not running!\n", i);
        }
        /* wait all cpu to boot */
        kinfo("All %d CPUs are active\n", PLAT_CPU_NUM);
        init_ipi_data();
}
```

### 思考题3

> 练习题 3：完善主CPU激活各个其他CPU的函数：`enable_smp_cores`和`kernel/arch/aarch64/main.c`中的`secondary_start`。

- 参考代码

### 思考题4

> 练习题 4：本练习分为以下几个步骤：
> 1. 请熟悉排号锁的基本算法，并在`kernel/arch/aarch64/sync/ticket.c`中完成`unlock`和`is_locked`的代码。
> 2. 在`kernel/arch/aarch64/sync/ticket.c`中实现`kernel_lock_init`、`lock_kernel`和`unlock_kernel`。
> 3. 在适当的位置调用`lock_kernel`。
> 4. 判断什么时候需要放锁，添加`unlock_kernel`。（注意：由于这里需要自行判断，没有在需要添加的代码周围插入TODO注释）

