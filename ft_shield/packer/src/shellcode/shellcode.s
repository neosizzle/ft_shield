section .text

; func(rdi, rsi, rdx, rcx)

; old entry
; 0x401000
; 0x555555555000 _init
; 0x555555555060 _start (in .text)
; 0x555555555149 _main (in .text)
; 0x555555555166 _main end (in .text)
; b __run_exit_handlers (rdx issue)

_init_woody_str:
        ; 0x401025
        ; 0x555555555175
        call _shell_main
        db `....WOODY....\n`; call pushes next instruction to stack

_shell_main:
        ; print WOODY message
        ; 0x401038
        ; 0x555555555188
        pop rsi             ; pointer to string

        ; push rdx to save reg, used by libc later on
        push rdx

        mov rdi, 1          ; stdout
        mov rdx, 14         ; length
        mov rax, 1          ; write
        syscall             ; write(1, str, 14);
        ; 0x40104b
        ; 0x55555555519b


        ; mprotect here to make text section writable
        mov rax, 0xa ; syscall number for mprotect
        mov rdi, rsi ; address to memprotect / arg1, calculated from _init_woody_str address
        sub rdi, 0x5 ; minus 5 for the call instruction. NOTE: not using new_entry here due to ASLR for pie binaries
        mov rsi, [rel new_entry] ; calculate size of mprotect buffer, using new entry. NOTE: using new_enrtry here because offset is ASLR insensitive
        sub rsi, [rel v_addr] ; difference between new entry and v_addr is original segment length
        sub rdi, rsi ; rdi is already at end of mprotect region, so just sub difference to get to the start
        mov rdx, 0x7 ; read, write and execute perms
        ; 0x40106d
        ; 0x5555555551bd
        syscall

        ; start decryption routine
        jmp _key ; push key to stack

key_ret:

        pop rsi ; rsi has key now
        ; 0x401075
        ; 0x5555555551c5
        mov rax, rdi ; load mprotect region start from jn?
        sub rax, [rel p_offset]
        add rax, [rel text_offset] ; since the segment may not be the start of the section, relocate to section start
        mov rcx, 0 ; text section iterator
        mov rdx, 0 ; key iterator
        
_decrypt:
        ; 0x401090
        ; 0x5555555551e0
        cmp rcx, [rel text_size]
        jz _end_decrypt ; end of all of text section is decrypted
        cmp rdx, [rel key_size]
        jnz _decrypt_routine
        mov rdx, 0 ; rotate key iterator if key idx overflow
        
_decrypt_routine:
        ; 0x4010a7
        ; 0x5555555551f7
        mov r8b, byte[rsi + rdx] ; current key byte in r8
        xor byte[rax, rcx], r8b ; xor cipher byte and key byte
        inc rcx
        inc rdx
        jmp _decrypt

_end_decrypt:
        ; 0x4010b7
        ; 0x555555555207
        mov rax, rdi ; load old entry
        add rax, [rel old_entry]
        sub rax, [rel v_addr] ; since the segment may not be the start of the section, relocate to section start
        ; restore rdx
        pop rdx

        jmp rax

        xor rdi, rdi        ; exit code 0
        mov rax, 60         ; exit
        syscall

_params:
        v_addr dq 0x0
        p_offset dq 0x0
        text_offset dq 0x0
        text_size dq 0x0
        new_entry dq 0x0
        old_entry dq 0x0
        key_size dq 0x0
        
_key:
        call key_ret
        db ``