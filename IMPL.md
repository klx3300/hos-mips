# HOS-Implementation

## 0x00 Linking and Startup Code

The kernel startup code is linked at memory address `0x80001000` (*ucore.ld*).

Refering to MIPS PRA, this address is at space `kseg0`, which isn't controlled by TLB by directly mapping to lowest physical memory space, only executable by kernel.

The startup code is located at *entry.S*, which do the basic initialization work and jump to C environment.

First, the code is trying to clear misc. registers set by resetting the whole CPU by clearing `CP0[Cause]` and `CP0[Status]`.

Then, by using `JAL` instruction, the startup code is trying to setup C-style function calling conventions by setup proper values in register **ra**, **gp** & **sp**. After successfully setup the function calling convention, it's possible to initialize the exception handling function, by load exception vector address `__exception_vector` into `CP0[EBase]` and explicitly set `CP0[Status][BEV]` to enable vectored interruptions.



---

### Tips: Current Interruption Handling State

What's loaded into the interruption base address `CP0[EBase]`?

The corresponding code is in *exception.S*.

The macro `RVECENT(f, n)` & `XVECENT(f, n)` is used to create the handling code. By referring to MIPS PRA, the MIPS Interruption Vectoring logic is very different from the x86. In general, the MIPS Processor will jump to some address calculated by the `CP0[EBase]`, and directly executing code from that. Due to this restriction, the handling code for each exception must be very compact.

In fact, the two macros above does the same thing: directly jump to dispatching entrance provided by parameter `f`. The `n` and the macro name different is just helping you to remember what this entry is responsible for.

Currently, this code is only able to hand two types of exception: TLBMiss & General Exceptions. Detailed handling procedure will be profiled later this document. Now let's focus on startup procedure.

---



By completing the bootloader work of clearing BSS segment of the kernel executable, now we are prepared for executing C codes. The startup code simply use a `JAL` to jump to C-coded kernel initialization routine.



## 0x01 Kernel Initialization: TLB, PIC, Console & Clock Interrupts

