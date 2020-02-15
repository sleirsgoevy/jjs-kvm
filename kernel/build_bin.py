import os

start_addr = None

for a, b, c in map(str.split, os.popen('nm kernel.elf')):
    if c == '_start':
        start_addr = int(a, 16)

assert start_addr != None

data = open('kernel.bin', 'rb').read()
setup = open('setup.bin', 'rb').read()
assert len(setup) == 4096

jump_addr = setup.find(b'\xb3\xb3\xb3\xb3')

rel_off = start_addr-(jump_addr+0x1004)

setup = setup[:jump_addr]+rel_off.to_bytes(4, 'little', signed=True)+setup[jump_addr+4:]

with open('../kernel.bin', 'wb') as file:
    file.write(setup+data)
