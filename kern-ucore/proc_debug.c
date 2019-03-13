#include <proc_debug.h>
#include <stdio.h>

const char* PROC_STATE_STRS[] = {"Uninitalized",
"Blocking", "Runnable", "Zombie", "Unknown"};

const char* PROC_FLAGS_STRS[] = {"Exiting"};

const char* PROC_WAITS_STRS[] = {"Child",
"Timer", "Keyboard Input", "Kern Semaphore", 
"User Semaphore", "Sending Event", "Receiving Event",
"Sending Message", "Receiving Message", "Pipe I/O",
"Signals", "Kernel Signals", "Interrupted"};

const char* list_sep = ",";
const char* list_first = "";

const char* pstatestr(enum proc_state pstate){
    if(pstate > PROC_ZOMBIE) pstate = PROC_ZOMBIE + 1;
    if(pstate < 0) pstate = PROC_ZOMBIE + 1;
    return PROC_STATE_STRS[pstate];
}

void dump_pflags(uint32_t pflags){
    if(pflags & PF_EXITING) kprintf(PROC_FLAGS_STRS[0]);
}


#define PRLIST_MAGIC(x) do{\
kprintf("%s%s", isfirst ? list_first : list_sep, (x));\
if(isfirst) isfirst = 0;}while(0)

void dump_pwaitstate(uint32_t ws){
    int isfirst = 1;
    if(ws & WT_CHILD) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_CHLD]);
    if(ws & WT_TIMER) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_TIMER]);
    if(ws & WT_KBD) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_KBD]);
    if(ws & WT_KSEM) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_KSEM]);
    if(ws & WT_USEM) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_USEM]);
    if(ws & WT_EVENT_SEND) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_EVSEND]);
    if(ws & WT_EVENT_RECV) PRLIST_MAGIC(PROC_WAITS_STRS[PWSTR_EVRECV]);

}