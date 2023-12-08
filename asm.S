.text
.extern _dosigret

// Note this swtch does not support switching to oneself
.global _swtch
_swtch:
    // rdi: where to save this stack
    // rsi: new satck to switch to

    push %r12
    push %r13
    push %r14
    push %r15
    push %rbx
    push %rbp

    mov %rsp, (%rdi)
    mov %rsi, %rsp

    pop %rbp
    pop %rbx
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    ret

// rdi: addr
// rsi: new
// rdx: old
.global _cmpxchg
_cmpxchg:
    mov %rdx, %rax
    lock cmpxchg %rsi, (%rdi)
    ret

// rdi: addr
// rsi: new
.global _xchg
_xchg:
    mov %rsi, %rax
    lock xchg %rax, (%rdi)
    ret
