import sys
import unicorn, unicorn.x86_const as x86
from unicorn.unicorn import Uc

x = Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)

data = open('../kernel.bin', 'rb').read()

def hook_fault(x, accs, addr, sz, val, _):
    print('fault at addr', hex(addr))
    return False

def hook_out(x, port, sz, val, _):
    if port == 0xe9 and sz == 1: #qemu debugcon compatible
        sys.stdout.buffer.raw.write(bytes((val,)))

def hook_trace(x, addr, sz, _):
    print(hex(addr))

def hook_mem_read(x, accs, addr, sz, val, _):
    print('read', addr, sz)

def hook_mem_write(x, accs, addr, sz, val, _):
    print('write', addr, sz)

x.mem_map(0, 2**32)
x.mem_write(0x1000, data)
x.hook_add(unicorn.UC_HOOK_INSN, hook_out, None, 1, 0, x86.UC_X86_INS_OUT)
x.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED|unicorn.UC_HOOK_MEM_FETCH_UNMAPPED, hook_fault)
if '--trace' in sys.argv:
    x.hook_add(unicorn.UC_HOOK_CODE, hook_trace)
    x.hook_add(unicorn.UC_HOOK_MEM_READ, hook_mem_read)
    x.hook_add(unicorn.UC_HOOK_MEM_WRITE, hook_mem_write)

try: x.emu_start(0x1000, 0)
finally:
    print()
    print(hex(x.reg_read(x86.UC_X86_REG_CR2)))
