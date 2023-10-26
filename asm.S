.text
.extern _dosigret

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

// "ret" (instead of "call") here to pop one 8 byte value from the stack
// to move sp up by 8 bytes
.global _doret1
_doret1:
    ret

.global _doret2
_doret2:
    mov 24(%rsp), %rdi // Prep arugment for func()
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
