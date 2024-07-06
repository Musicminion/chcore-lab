## Lab2

> By：张子谦

### 思考题一

> 思考题 1：请思考多级页表相比单级页表带来的优势和劣势（如果有的话），并计算在 AArch64 页表中分别以 4KB 粒度和 2MB 粒度映射 0～4GB 地址范围所需的物理内存大小（或页表页数量）。

优势：

1. **内存使用效率提高**：多级页表允许按需分配页表，只有实际映射到物理内存的虚拟地址空间才需要分配页表项，这减少了未使用地址空间的页表占用，从而提高内存使用效率。
2. **支持更大的地址空间**：通过增加页表级数，多级页表可以支持更大的虚拟地址空间，而不是线性增加单个页表的大小，这对于现代操作系统支持大量内存和进程是非常重要的。
3. **灵活的内存管理**：多级页表结构提供了更灵活的内存管理方式，可以更容易地实现如按需分页（demand paging）、共享内存、内存保护等高级内存管理功能。

劣势：

1. **访问时间增加**：每次虚拟地址到物理地址的转换可能需要访问多个页表，这增加了内存访问的延迟。虽然现代处理器通过TLB（Translation Lookaside Buffer）缓存常用的地址转换来减少这种开销，但是TLB未命中时的性能损失仍然存在。
2. **实现复杂度增加**：多级页表的管理比单级页表复杂，需要操作系统在页表的创建、销毁、维护等方面做更多的工作，这增加了操作系统内核的复杂度。
3. **空间开销仍然存在**：虽然多级页表提高了内存使用效率，但是对于每一级的页表本身仍然需要占用一定的物理内存，特别是在深层次的多级页表结构中，这种开销不容忽视。

如果是4KB粒度，映射可以得到：4GB / (4KB * 512) = 2048个页表页；

如果是2MB粒度，映射可以得到：4GB / (2MB * 512) = 4个页表页

### 思考题二

练习题 2：请在 `init_boot_pt` 函数的 `LAB 2 TODO 1` 处配置内核高地址页表（`boot_ttbr1_l0`、`boot_ttbr1_l1` 和 `boot_ttbr1_l2`），以 2MB 粒度映射。

mmu.c文件函数的这部分的操作主要是，把虚拟地址的低地址、高地址映射到

- 高地址中，我把外设[`KERNEL_VADDR`, `KERNEL_VADDR` + `PERIPHERAL_BASE`]处的地址映射到物理内存中的物理内存（SDRAM）[`PHYSMEM_START`, `PERIPHERAL_BASE`]
- 高地址中，把的[`KERNEL_VADDR + PERIPHERAL_BASE`, `KERNEL_VADDR + PHYSMEM_END`]处的地址映射到物理内存中的共享外设内存[`PERIPHERAL_BASE`, `PHYSMEM_END`]

```c++

        /* TTBR1_EL1 0-1G */
        /* LAB 2 TODO 1 BEGIN */
        /* Step 1: set L0 and L1 page table entry */
        // `0xffff_ff00_0000_0000`～`0xffff_ffff_ffff_ffff` 为高地址。
        // 这里KERNEL_VADDR为高地址
        vaddr = KERNEL_VADDR;
        boot_ttbr1_l0[GET_L0_INDEX(vaddr)] = ((u64)boot_ttbr1_l1) | IS_TABLE
                                             | IS_VALID | NG;
        boot_ttbr1_l1[GET_L1_INDEX(vaddr)] = ((u64)boot_ttbr1_l2) | IS_TABLE
                                             | IS_VALID | NG;

        /* Step 2: map PHYSMEM_START ~ PERIPHERAL_BASE with 2MB granularity */
        for (; vaddr < KERNEL_VADDR + PERIPHERAL_BASE; vaddr += SIZE_2M) {
                boot_ttbr1_l2[GET_L2_INDEX(vaddr)] =
                        (vaddr - KERNEL_VADDR) /* high mem, va = pa - KERNEL_VADDR */
                        | UXN /* Unprivileged execute never */
                        | ACCESSED /* Set access flag */
                        | NG /* Mark as not global */
                        | INNER_SHARABLE /* Sharebility */
                        | NORMAL_MEMORY /* Normal memory */
                        | IS_VALID;
        }

        /* Step 2: map PERIPHERAL_BASE ~ PHYSMEM_END with 2MB granularity */
        vaddr = KERNEL_VADDR + PERIPHERAL_BASE;
        for (; vaddr < KERNEL_VADDR + PHYSMEM_END; vaddr += SIZE_2M) {
                boot_ttbr1_l2[GET_L2_INDEX(vaddr)] =
                        (vaddr - KERNEL_VADDR) /* high mem, va = pa - KERNEL_VADDR */
                        | UXN /* Unprivileged execute never */
                        | ACCESSED /* Set access flag */
                        | NG /* Mark as not global */
                        | DEVICE_MEMORY /* Device memory */
                        | IS_VALID;
        }

        /* LAB 2 TODO 1 END */
```

### 思考题三

 思考题 3：请思考在 `init_boot_pt` 函数中为什么还要为低地址配置页表，并尝试验证自己的解释。

在 `init_boot_pt` 函数中保证低地址和高地址都能映射到对应的物理内存。这样在启动MMU后，PC的值会变成MMU开启后的地址+4，配置了低地址页表之后，虚拟地址中的低地址映射到物理地址中数值相等的内存区域，能顺利执行接下来的代码。

### 思考题四

思考题 4：请解释 `ttbr0_el1` 与 `ttbr1_el1` 是具体如何被配置的，给出代码位置，并思考页表基地址配置后为何需要ISB指令。

查看代码发现在`kernel/arch/aarch64/boot/raspi3/init/tools.S`里面可以找到ttbr0_el1的代码，实际上boot_ttbr0_l0来源于`kernel/arch/aarch64/boot/raspi3/init/mmu.c`定义的全局变量。

	/* Write ttbr with phys addr of the translation table */
	/* tool.S:246-250 */
	adrp    x8, boot_ttbr0_l0
	msr     ttbr0_el1, x8
	adrp    x8, boot_ttbr1_l0
	msr     ttbr1_el1, x8
	isb

`isb`的作用是：确保当前指令执行完成之前，后面的指令不会得到执行。考虑到CPU的设计，这些汇编指令可能会出现串行的执行流。在配置好页表基地址之前，是万万不可以执行开启MMU的代码。

### 思考题五





