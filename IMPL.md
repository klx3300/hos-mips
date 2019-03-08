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

The first thing initialization process do is invalidate all TLB entries. That's another significant difference between MIPS and x86: In x86, we prepare page tables, and the processor shall handle TLB and TLB Misses for us. In MIPS, The OS is the one who take care of that.

All TLB entries are starting to be at unknown state. In order to prevent unwanted exceptions (Protection Violation, etc..) when we try to access memories at mapped area, a good practice is to clear all stuff by invalidating it all over.

The corresponding TLB invalidating code is at *thumips_tlb.c*. Current implementation is assuming the TLB only have 16 entries, and by using `write_one_tlb` (implemented by CP0 instructions & `TLBWI`), this code writes TLB Index 0~15 with `VPN2` starting from `0x80000000`, which is totally useless because these memory region is `UNMAPPED`(*kseg0*). The reason doing this instead of just put all zeros is to prevent multiple TLB match when doing memory access, resulting in an `Machine Check` Exception, instead of expected `TLB Refill`.

The `PIC` initialization part seems just to temper the SMP(*Symmetric Multiprocessing*) part of original ucore kernel, as the code inside it is only some tricks.

Then we come to the `cons_init()`, which initializes the console output. This procedure in turn calls `serial_init()` to complete the initialization of `COM` output, which do some COM work which I don't really care, and continue to enable interruption controlling of the COM port by calling `pic_enable()`.(The name is confusing, as I didn't see any relationship between this function and previous `pic_init()` stuff. Am I missing something?) This function set the corresponding interrupt mask bit in `CP0[Status]`.

---

### Tips: Console Output Sequence

Let's directly jump to the workhorse part. The workhorse for all `klibc`(I'm used to call them that, as they are kernel-implemented `libc` functions to make everyone who is already familiar with C happy) I/O functions is `cons_putc()` & `cons_getc()` at *console.c*.

Starting from the output part, we can see the function calls `local_intr_save` to save the current value and clear the `CP0[Status][IE]` bit to prevent further interrupts. Then, the function calls the `serial_putc`, which do the `\b` clearing trick and call `serial_putc_sub` to do the COM communication.

For the input part, it seems that normally COM input is handled by interruptions, but to enable the processor to grab input explicitly even when the interruption is disabled due to debug requirements, this function calls `serial_intr()`, which is going to poll the COM port for new input.

---

After console output is up, the kernel initialization procedure continues to the next item: the clock interrupt, which is basically one of the fundamental hardware supports of modern OS concepts. The corresponding functions is located at *clock.c*, which includes the initialization code and the clock interruption handler code.

The MIPS clock interruption is also different to the x86. By referring to MIPS PRA, the timer interruption in MIPS is completed by the two CP0 registers, `Count` & `Compare`. The `Count` is a incremental counter set by the hardware clock signal. When the two registers hold the same value, a clock interruption is raised. The `reload_timer` function is designed to do this work, by loading the compare register with the current count value plus an interval, to create a periodical timer interrupt.

The `clock_int_handler` completes the jobs which should get done every time a clock interruption arrived, basically the system `timer` services. This mechanism is left for further discussions in following chapters.

## 0x02 Interlude: The HOS Interruption Handling

As we reached a point which closely related to the interruption handling procedure, let's study further about how interruptions are handled by HOS.

The related code is at *exception.S*, which every exception is dispatched through the `common_exception`. But before that, the `ramExcHandle_general` is going to setup sane working environment for it, basically the kernel stack. The code saves previous stack pointer (to **k1**, which will be additionally saved to exception kernel stack), checks whether it's coming from user space. If it does, the code setup the kernel stack using the provided memory pointer in the processor controlling structure - the details will be left for further discussion.

In `common_exception`, it setup function calling convention to satisfy GDB backtrace, complete the register saving corresponding to the `mips_trap` function (or `struct Trapframe`). The cause of the interruption, effectively `CP0[Cause]`, is already stored to register **k0** before this routine, so saving them saves the cause altogether. Then, in order to satisfy the nested interruption, clears the `CP0[Status][KSU], CP0[Status][EXL], CP0[Status][IE]`...

> Wait, what did it clear?
>
> *OKAY*...

Ignoring that, the routine continues to call the `mips_trap` to do the actual work.

When returning from handler, the code proceed by restoring those saved values back to corresponding registers & special registers(including `CP0[Status]`), and finally issue `ERET` to return from interruption. Note to keep things consistent, the **sp** must be restored at the last, and `CP0[Status][EXL]` should be set to mimic a interrupted state.

In `mips_trap`, to do the nested interruption handling, it's saving the previous trapframe of current process, which will be discussed later, and then call the dispatch procedure `trap_dispatch`.

## 0x03 Memory Management: PMM & Page Directories

Note: the HOS is mimicking the x86 L2PT(2-level page table, or page directory, if you like) mechanism, which I will not explain in detail in this document. If you aren't familiar with it enough, read it at my [BLOG](https://imzhwk.com).

> If you are *really* going to read my blog, refer to MIT 6.828 Lab Solutions.

The kernel completes basic memory management initialization using `pmm_init`.

The `pmm_init` does the following job:

- Call `init_pmm_manager`, which points to the initialization procedure of current effective PMM. It seems this PMM uses a First-Fit allocation algorithm, or `FFAA`, or call it whatever you like.
  - For a FFAA, it just create an empty list.
- Call `page_init`, which initialize the `struct Page` array to hold information for each page, and call the corresponding PMM routine to finish initialization.
  - The total physical memory is hard-coded, `512M`.
  - The idea here is use the `end` linker trick to calculate how much memory the kernel already occupied, and set the remains to FREE.
- Create initial page directory for kernel.
  - Well I don't really understand why this is necessary, as this is MIPS, kernel code is always executing at unmapped region *kseg0*. Perhaps it's for convenient access of physical memory by creating a direct mapping below `0x80000000`, but we shouldn't do this because it's too wasting, always remember we are MIPS!! The more efficient way is to directly infer the corresponding address when a kernel space TLB miss occurred.
  - The `boot_alloc_page` is not we think for the more clean implementation of `JOS`, it's just a pointer to the allocation procedure of current PMM.
  - The `check_pgdir` is the uCore lab grading function. Feels funny.
  - The structure of page directory isn't using the `JOS`-style wrap-back trick.
  - And they set the unmapped space mapped for kernel in this `boot_pgdir`. Interesting. Don't get why. Wasting memory? My original suspect did not prove right: they did not map the *kuseg* part.
- Set the current page directory to `boot_pgdir` so the handler can get the correct physical address when facing kernel space TLB miss....
  - Well it's executing *kseg0*! It shouldn't trigger any TLB miss here because this mapping just won't go through TLB **AT ALL**!!!!
  - What are they writing? **Am I missing something ???** Great thanks for any hints provided. Contact me when you found the answer.
- Call `check_boot_pgdir`, which is uCore grading function, and clearing the whole page directory all over again.
  - Well I'm not surprised anymore.
  - Note there's a memory leak because the page directory isn't cleared using the correct routine, the page table pages will get leaked with their reference counters never gets 0.

## 0x04 Memory Management: TLB Misses

Shortly off-course, it's time to discuss another topic about memory management: how HOS take care of TLB misses happened when executing code on mapped area?

All TLB-related exceptions is redirected to `handle_tlbmiss`(*trap.c*).

The code firstly check for whether this is a VM violation or just a TLB miss. On VM violation, we can simply reuse the page fault handler already written in uCore to handle CoW(*Copy-on-Write*, if you aren't familiar with OS tricks), shadow allocated memory, and any other possible issues.

On a TLB miss, the code first read out the corresponding page table entry from the recorded `current_pgdir`(detailed will be explained at user process chapter), and then decide whether there's additional VM violation. If there is, kill the process. Otherwise, call `tlb_refill` to refill the missing TLB entries.

The `tlb_refill` uses the `TLBWR` instruction to write a random TLB entry, hence effectively solved the problem.

## 0x05 Process Management: Process Table

> Many operating system textbooks prefer to call the kernel-maintained data structure "Process Control Block", or PCB. In my case, the naming is differ from each specific kernel implementation. In this case(HOS), I prefer to call them "Process Structure".

Although the process scheduler is initialized before the process table itself, but we need a deeper insight of process structure to understand process scheduler better.

Unlike JOS, which use a large array to allow very fast random process structure access, the HOS or perhaps uCore (I didn't read the uCore document) is using a linked list to support not-predefined number of possible process structures, and use a hash table to enable relatively fast random access.

- In fact, the hash table trick itself is also very slow. There's another way to persist the two advantages, which will require reusing the freed process id.
  - I'm not going to describe the method here.

Processes also have relationships between them, most of the relationships is implemented as UNIX-like process relationships, including parent processes, sibling processes, and thread groups.

The process management subsystem is using some functions to maintain the correctness of these linked lists.

The master process table link, `proc_list`, is maintained using the `set_links` function(*proc.c*), which also setup proper sibling pointer using the parent pointer of the provided process structure. The searching process table link, `hash_list[]`, is maintained using another separate function `hash_proc`.

- I don't quite get why they DIDN'T combine these functions altogether to form a one-for-all function.

Thread groups is handled explicitly by the process creation routine, `do_fork`, which will be explained in detail later this document.

## 0x06 Process Life Cycle: Process Creation

Process is the fundamental execution unit of almost every modern operating systems. In these systems, traditional *threads* is implemented as processes which share some specific resources with the main thread (or whatever thread as long as applicable). The HOS didn't fail this assumption.

Every single process is forked from some previously existent process, which ... isn't true in the HOS case. The kernel startup routine isn't a process itself, but it's possible for it to create kernel threads by directly creating a fake trapframe and pass it to the `do_fork` to create independent processes. The related function is  `ucore_kernel_thread`. This mechanism is used to create the very first **valid** process in the whole system, the `kinit` process. The job of this thread will be discussed later on other chapters.

To make things clear, there are **invalid** processes used for kernel idle processes, which named `idle/*`. The reason I call them **invalid** is they just don't have valid trap frame. The only work of them is to trigger the re-scheduling routine, by keeping the `need_resched` field always set. These idle processes is directly created using `alloc_proc`, with PID directly set to the corresponding Processor ID.

- Wake up! We don't have real SMP support.

For the last part of this chapter, let's go through the `do_fork` routine, to have a thoroughly understanding of this procedure.

- First, do the check of current running process count with the maximum supported processes.
  - the `nr_process` trick is maintained by `set_links`. Don't understand why.
- Then, allocating a process structure using `alloc_proc`
- Setup the fields of the newly created process structure, including:
  - Parent Processor, Thread Group Link
  - Blocking Reason(they call it `wait_state`), time slice size(to 1/2 of the parent)
  - Setup Kernel Stack, Clone semaphore, filesystem state, page directory, signals, signal handlers if necessary, according to the clone flags provided.
  - Call the `copy_thread` to setup trapframe and do the fork return value trick.
- There's a `tls_pointer` which just don't know what's the use, searching through the source code tree result in nothing. Am I missing something again??
- Then a series of atomic operations which completes the link tables maintaining work.
- Finally call `wakeup_proc` to .... and return the created process id.

## 0x07 Process Life Cycle: Runnable

A process is at this state when it's just finished creation or be preempted because of exhausting its time slice or giving up its time slice by explicitly `yield` it.

Note: the exact words we use is different from the actual HOS code: when a HOS process is in `PROC_RUNNABLE` state, the process is either runnable but aren't running currently, or it's the current running process. In this chapter, when we refer to *runnable state*, we are referring to the not-running state of `PROC_RUNNABLE` processes.

The last function `do_fork` calls, `wakeup_proc`, is responsible for changing the state of given process to `PROC_RUNNABLE`. The detailed operations of this function will be left for further discussions in following chapters.

One of the runnable processes is chosen by the scheduler when one of the processes exhausted its assigned time slice or start blocking by I/O or waiting for semaphore or whatever reason. The chosen process will start running by `proc_run`.

The most interesting and funny designed part of HOS is perhaps this function, `proc_run`. Unlike `JOS`, which executing `env_run` directly resulted in switching back to user space, the `proc_run` in HOS is to switch between the kernel space of two processes, using the `context` member of process structure. Note the `load_rsp0` function does nothing, the real work of switching kernel stack between processes is done by `set_pagetable`. The `switch_to` routine is written in assembly, which saves the current kernel space context into previous context structure, and restore the registers using the next context structure, effectively switching to another process. Then, this function will return to `schedule()`, and eventually back to the trap handler, which do the work of switching back to user space process.

- Don't understand the benefit of implementing `proc_run` in this way. It's restricting the flexibility of process switching, as things must eventually route back to the interruption handler, or the switching will fail somehow.

## 0x08 Process Life Cycle: Running

The property of a running process is much the same of a runnable process, so we focus on the procedure of exhausting its assigned time slice.

Discussed above, the timer interrupt handler executes `run_timer_list` to tick the system timer service. In this routine, beside the work of ticking every timer set up in system and **pulling up** processes, the last thing it does is to call `sched_class_proc_tick` to tick the scheduler about the current executing process. If the scheduler decided that the running process already exhausted its time slice, it will set the `need_resched` member of its process structure to `1`.

Upon returning to the handler `mips_trap`,  it will discover that, and explicitly run `schedule` to do reschedule. The `schedule` will clear the `need_resched` bit, invoke the scheduler to get next running process, and switch to it.

There are several synchronization system services such as semaphores will put process out of running/runnable state, but these are left for discussions in IPC related chapters.

## 0x09 Process Life Cycle: Dying

Extinction is part of the essence of anything, a HOS process never have the luck to evade from that.

The work of exiting the whole process is done by `do_exit`. But let's first look at the `do_kill` routine, which is used for system kill process, but instead of sending signals like other unix-like OSes, the `do_kill` of HOS is only used for killing any valid process, regardless of its state.

The `do_kill` routine first search for the corresponding process structure, and passing them to the `__do_kill` workhorse along with the error code. The `__do_kill` set the `flags[PF_EXITING]` bit of the process structure, and forcefully wake it up by invoking `wakeup_proc`. When rescheduling hits this process, or it's encountering any interrupts, the handler `mips_trap` will found that this process is in `PF_EXITING` state, and call `do_exit` to finish the work of killing it. The reason `do_kill` must wake the process up is that the `do_exit` can only terminate the current running process, and requiring the process is in runnable state.

Now let's look at the `do_exit` routine. First, this process iterate through all processes in the same thread group, killing them except the exiting process using `__do_kill`. Then, call `do_exit_thread` to set the exiting code of the current process, and finally `__do_exit` to complete the cleanup work. The `__do_exit` do the following things in turn:

- Use `put_pgdir` to free the space occupied by the process... or did it?
  - By reading the code, it's very easy to discover that this function only releases the very page that occupied by page directory. The process text segment, data segment, stack, and all page tables are leaked.
  - This is a very serious kernel memory leak, and will significantly reduce the usability of the operating system. Am I missing the code that resolves this problem? Let me know once you found it.
- Clearing other system service structures, including singals, filesystems, semaphores.
- Setting current process state to `PROC_ZOMBIE`, and notify its parent if it's waiting for children signals.
- Removing this process from its thread group.
- Do the reparenting work for its child: either another process in the same thread group of current process, or the init process.
- Wake up every process in the event box wait queue of this process using `WT_INTERRUPTED`, the detailed behavior will left for the IPC chapter.
- Call `schedule` to select another runnable process to run.

## 0x?? Scheduler

Although scheduler can affect modern OS performance significantly, the HOS scheduler is a simple Round-Robin scheduler located in *sched_RR.c*, and doesn't have much interesting details to talk about. Let's skip this part.

## 0x0A Synchronization: Semaphores

HOS implements semaphores as one of the inter-process synchronization methods.To understand how this is working with the scheduler to provide a blocking behavior, we can begin at looking the corresponding system calls for semaphores. Currently, the HOS provide the following system calls for semaphore operations: `sem_init`, `sem_post`, `sem_wait`, `sem_free`, `sem_get_value`. the `post` operation increases the value of given semaphore, while `wait` operation decreases it, and block the calling process if the semaphore value is lower than zero.

One implementation decision which confuse me much is the use of `sem_undo`. According to its description, the reason uCore using this mechanism is to prevent user semaphore deadlock when one of the processes holding it crashes unexpectedly.

But undo the modification to restore the semaphore to last consistent state isn't a good solution. I don't want to talk about it much as it's off topic, but readers should consider semaphores with this mechanism is using with producer-consumer model while passing content using shared memory.

But it seems this confusing mechanism isn't implemented currently, which saves me a lot time required to understand the philosophy behind it.

### Overview, Creation, Destruction

In *sem.h*, there's three structure about semaphores, `semaphore_t`, `sem_undo_t`, and `sem_queue_t`. The relationship of them is as follows:

```
PROC -> sem_queue_t -> semaphore_t
                  | -> sem_undo_t (list) -> semaphore_t
```

The semaphore of `sem_queue_t` is used at kernel to guarantee the atomic operation of itself, while newly created semaphores goes to the `sem_undo_t` list. The returned semaphore ID is created by the allocated memory address for the semaphore minus the kernel base address.

The `count` member in `semaphore_t` is used for reference counter, increases when the semaphore queue is duplicated, and all semaphores inside it is referenced another time.(When? Hint: `fork()`).

When a semaphore is going to be destroyed, the `valid` member of `semaphore_t` will be cleared first, and every process that waiting for this semaphore is woken up using `WT_INTERRUPTED`. The invalid semaphore isn't going to be destroyed immediately, but whenever semaphore searching(will be invoked on almost every semaphore operation) reached an invalid semaphore, which will decrease its reference counter and remove the `sem_undo_t` from the list of current process, and eventually free it when reference counter reaches `0`.

###  Consume(`sem_wait`) and Produce (`sem_post`)

When a process is trying to decrease a semaphore, it uses the `sem_wait` system call. The actual workhorse of this system call is `usem_down`, which operates on `sem_undo_t`.

To provide a timeout mechanism, the `usem_down` creates a timer using system timer services, and pass it to the `__down` workhorse to complete the final steps of setting up process status.

---

### Interlude: Timer Service

HOS provide a system timer service in a relatively simple manner: when you setup a timer using `add_timer`, the timer will be added into a linked list, which the expire time of every timer is a relative value of previous timer, in a ordered manner. Every time the clock interrupt happen, the kernel would iterate through this list, trigger every timer that expired.

There're two kinds of timer provided by kernel. One is the simple timer, which will wakeup the process which set up it. Another type is a `linux-liked` timer, which accepts a timeout callback function, and will be executed by the kernel when it's expired. Semaphores utilize the first type.

---

The `__down` will finally complete the decrease of specified semaphore, and set current process to wait this semaphore by inserting the current process into the wait queue of the semaphore using `wait_current_set`. After that, it calls `ipc_add_timer` to setup the timer, then select a new process by calling `schedule`.

- `wait_current_set` should be better interpreted as `set_current_wait`, which not only insert current process into specified wait queue, but set the process state to waiting. Interesting naming style, I have to admit.
- If the semaphore value is equal to 0, the `__down` will not actually decrease it.

When this process is woken up, then `__down` will first remove the waiting state and the timer, then check the reason of waking by `wakeup_flags` of the `wait_t` structure passed into the `wait_current_set`.

The `__up` increases the semaphore value, and use the `wakeup_wait` function to wake up the first process that is waiting for this semaphore to be available. The `wakeup_wait` function will finish the job of removing the woken process from the wait queue.

-  Much alike the `__down`, if there's process waiting for this semaphore, the `__up` will not increase the semaphore value.

## 0x0B Communication: Event

HOS provides two inter-process communication(**IPC**) mechanism, but one of them isn't used. We introduce the one that's currently exposed to user space programs first: the `event` mechanism.

The related functions is located in *event.c*.

When a process wants to wait for an event to be sent to itself, it calls the `sys_event_recv` system call, which dispatches to workhorse `ipc_event_recv`. The `ipc_event_recv` performs parameter sanity check, and creates timer according to the `timeout` provided, then call `recv_event` to finalize the works and process states.

The `recv_event` will firstly check whether there's events already waiting on the wait queue of `current->event_box`, which is a member of process structure. If there is, copy this back to the receiving process, and wake the sender process up to notify it that the sending is succeeded.

Else, the receiving process will be put to blocking state, the created timer is added to the system timer list to ensure the timeout mechanism, and call `schedule` to run another process. The woken up checks is much alike the semaphore part.

- Note: receiving process is not added to any wait queue!

Then let's focus on senders. The corresponding dispatched function is `ipc_event_send`, which also performs parameter sanity check at first. Then, this function will wake up the specified receiver in the sending parameter, and then set the `current->event_box.event` to the event this process is sending, create timer, call the `send_event` to finalize its works.

`send_event` put the sender process into the wait queue of the receiver's event box. When the receiver is woken up from receiving event, it will pull out the event from its event box, and wake the sender back from the waiting queue, thus the two side is acknowledged the success of event passing.

By the way, the only data this event mechanism can send is one 32-bit integer.

## 0x0C: Communication (Not Used): MessageBox

This is the IPC mechanism that's currently not used in user space. Corresponding system calls aren't found on system call dispatching interface.

