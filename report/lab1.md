## Lab1

> By：张子谦

### 问题一

> 思考题 1：阅读 `_start` 函数的开头，尝试说明 ChCore 是如何让其中一个核首先进入初始化流程，并让其他核暂停执行的。
>
> 提示：可以在 [Arm Architecture Reference Manual](https://documentation-service.arm.com/static/61fbe8f4fa8173727a1b734e) 找到 `mpidr_el1` 等系统寄存器的详细信息。

**思考题 1**：`mpidr_el1`存储的是当前处理器的ID号。

- 首先第一步读取值到`x8`寄存器，然后将`x8`寄存器的值与`0xFF`进行逻辑与操作，结果仍然存储在`x8`中。
- `cbz x8, primary`：如果`x8`为0，则跳转到标签`primary`。这是用来检测当前处理器是否是主处理器（通常ID为0的处理器被认为是主处理器）。
- `b .`：一个无限循环，用于挂起非主处理器。如果当前处理器不是主处理器，它将停留在这个循环中

```
BEGIN_FUNC(_start)
	mrs	x8, mpidr_el1
	and	x8, x8,	#0xFF
	cbz	x8, primary

	/* hang all secondary processors before we introduce smp */
	b 	.

primary:
	/* Turn to el1 from other exception levels. */
	bl 	arm64_elX_to_el1

	/* Prepare stack pointer and jump to C. */
	ldr 	x0, =boot_cpu_stack
	add 	x0, x0, #INIT_STACK_SIZE
	mov 	sp, x0

	bl 	init_c

	/* Should never be here */
	b	.
END_FUNC(_start)
```

### 问题二

> 练习题 2：在 `arm64_elX_to_el1` 函数的 `LAB 1 TODO 1` 处填写一行汇编代码，获取 CPU 当前异常级别。
>
> 提示：通过 `CurrentEL` 系统寄存器可获得当前异常级别。通过 GDB 在指令级别单步调试可验证实现是否正确。

**思考题 2**： `CurrentEL` 系统寄存器存储的是当前异常级别。

  ```
  mrs x9, CurrentEL
  ```

在gdb中打印后输出为 12，对应异常文件 EL3。

> 练习题 3：在 `arm64_elX_to_el1` 函数的 `LAB 1 TODO 2` 处填写大约 4 行汇编代码，设置从 EL3 跳转到 EL1 所需的 `elr_el3` 和 `spsr_el3` 寄存器值。具体地，我们需要在跳转到 EL1 时暂时屏蔽所有中断、并使用内核栈（`sp_el1` 寄存器指定的栈指针）。

### 问题三

**思考题 3**：添加的代码如下。

```
	/* LAB 1 TODO 2 BEGIN */
	adr x9, .Ltarget
	msr elr_el3, x9
	mov x9, SPSR_ELX_DAIF | SPSR_ELX_EL1H
	msr spsr_el3, x9
	/* LAB 1 TODO 2 END */
```

### 问题四

其中 `SPSR_ELX_DAIF` 用来设置屏蔽中断，而 `SPSR_ELX_EL1H` 用来设置内核栈。

> 思考题 4：结合此前 ICS 课的知识，并参考 `kernel.img` 的反汇编（通过 `aarch64-linux-gnu-objdump -S` 可获得），说明为什么要在进入 C 函数之前设置启动栈。如果不设置，会发生什么？

**思考题 4**：C语言中很多操作涉及到栈，例如函数返回的时候的返回地址是通过读取栈里面的值，此外函数的第一个参数、第二个参数等分别通过rdi、rsi寄存器传递。如果不设置栈，程序无法正常的运行。

### 问题五

> 思考题 5：在实验 1 中，其实不调用 `clear_bss` 也不影响内核的执行，请思考不清理 `.bss` 段在之后的何种情况下会导致内核无法工作。

答：未被初始化全局变量和静态变量被存储在bss段。若未执行`clear_bss`，虽然原则上也不影响内核的执行，但是如果有程序员在编写代码的时候，假定了全局变量和静态变量的初始值为0，那么就会出现一些致命错误。

### 问题六

> 练习题 6：在 `kernel/arch/aarch64/boot/raspi3/peripherals/uart.c` 中 `LAB 1 TODO 3` 处实现通过 UART 输出字符串的逻辑。

```c
void uart_send_string(char *str)
{
        /* LAB 1 TODO 3 BEGIN */
        early_uart_init();      // Initialize uart first
        int i = 0;
        while (str[i] != '\0'){
            early_uart_send(str[i]);
            i++;
	    }
        /* LAB 1 TODO 3 END */
}

```

### 问题七

> 练习题 7：在 `kernel/arch/aarch64/boot/raspi3/init/tools.S` 中 `LAB 1 TODO 4` 处填写一行汇编代码，以启用 MMU。

答：如下所示，这段代码首先从系统寄存器 `sctlr_el1` 里面读取值，然后到x8，经过一系列修改之后，然后再保存回去。

```
	mrs     x8, sctlr_el1
	/* Enable MMU */
	/* LAB 1 TODO 4 BEGIN */
	orr x8, x8, #SCTLR_EL1_M
	/* LAB 1 TODO 4 END */
	/* Disable alignment checking */
	bic     x8, x8, #SCTLR_EL1_A
	bic     x8, x8, #SCTLR_EL1_SA0
	bic     x8, x8, #SCTLR_EL1_SA
	orr     x8, x8, #SCTLR_EL1_nAA
	/* Data accesses Cacheable */
	orr     x8, x8, #SCTLR_EL1_C
	/* Instruction access Cacheable */
	orr     x8, x8, #SCTLR_EL1_I
	msr     sctlr_el1, x8
```

