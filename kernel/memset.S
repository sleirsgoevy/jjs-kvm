.intel_syntax noprefix

.global memset
memset:
push edi
mov edi, [esp+8]
mov al, [esp+12]
mov ecx, [esp+16]
test ecx, ecx
jz .skip_memset
rep stosb
.skip_memset:
pop edi
mov eax, [esp+4]
ret

.global memcpy
memcpy:
push edi
push esi
mov edi, [esp+12]
mov esi, [esp+16]
mov ecx, [esp+20]
test ecx, ecx
jz .skip_memcpy
rep movsb
.skip_memcpy:
pop esi
pop edi
mov eax, [esp+4]
ret
