section .text
org 0xc1000000
use32

dd end-start
start:

; header
dd 2 ;ntests
dd exec_file
dd exec_fn

; test #0 (trap)
dq 1000000 ;tl
dd 65536 ;ml (=256M)
dd input_txt
dd output_txt
dd test0_input_txt
dd 0 ;outcome

; test #1
dq 1000000 ;tl
dd 65536 ;ml (=256M)
dd input_txt
dd output_txt
dd test1_input_txt ;fs
dd 0 ;outcome

; test #2
dq 1000000 ;tl
dd 65536 ;ml (=256M)
dd input_txt
dd output_txt
dd test2_input_txt ;fs
dd 0 ;outcome

test0_input_txt:
dd input_txt
dd 0 ;FILE_READONLY
dd 0 ;data
dd 0 ;sz
dd 0 ;capacity
dd test0_output_txt ;next

test0_output_txt:
dd output_txt
dd 1 ;FILE_READWRITE
dd 0 ;data
dd 0 ;sz
dd 0 ;capacity
dd exec_file ;next

test1_input_txt:
dd input_txt ;filename
dd 0 ;FILE_READONLY
dd test1_input ;data
dd test1_input_end-test1_input ;sz
dd test1_input_end-test1_input ;capacity
dd test1_output_txt ;next

test1_output_txt:
dd output_txt ;filename
dd 1 ;FILE_READWRITE
dd test1_output ;data
dd 0 ;sz
dd test1_output_end-test1_output ;capacity
dd exec_file ;next

test2_input_txt:
dd input_txt ;filename
dd 0 ;FILE_READONLY
dd test2_input ;data
dd test2_input_end-test2_input ;sz
dd test2_input_end-test2_input ;capacity
dd test2_output_txt ;next

test2_output_txt:
dd output_txt ;filename
dd 1 ;FILE_READWRITE
dd test2_output ;data
dd 0 ;sz
dd test2_output_end-test2_output ;capacity
dd exec_file ;next

exec_file:
dd exec_fn ;filename
dd 0 ;FILE_READONLY
dd executable ;data
dd executable_end-executable ;sz
dd executable_end-executable ;capacity
dd 0 ;next

input_txt:
db "input.txt", 0
output_txt:
db "output.txt", 0
exec_fn:
db "test", 0

align 4
executable:
incbin "userspace/test"
executable_end:

align 4
test1_input:
db "1 2", 10
test1_input_end:

align 4
test1_output:
times 1048576 db 0
test1_output_end:

align 4
test2_input:
db "1000000000000 1000000000000", 10
test2_input_end:

align 4
test2_output:
times 1048576 db 0
test2_output_end:

end:
