import bundle, os.path, signal, pack, sys, ctypes

def memfd_create(name, flags):
    if isinstance(name, str): name = name.encode('utf-8')
    libc = ctypes.CDLL("libc.so.6")
    ans = libc.memfd_create(name, flags)
    if ans < 0:
        ctypes.pythonapi.PyErr_SetFromErrno(ctypes.py_object(OSError))
    return ans

def do_run(bundle):
    stubs = [os.dup(0) for i in range(5)] # alloc fds 1-5
    memfd = memfd_create('bundle.in', os.O_RDWR)
    with open(memfd, 'wb', closefd=False) as file: file.write(bundle)
    os.lseek(memfd, 0, 0)
    fd0, monitor_in = os.pipe()
    fd1 = os.open('/dev/null', os.O_RDONLY)
    done_fd, fd3 = os.pipe()
    fd4 = memfd
    bundle_out, fd5 = os.pipe()
    pid = os.fork()
    if not pid:
        os.dup2(fd0, 0)
        os.dup2(fd1, 1)
        os.dup2(fd3, 3)
        os.dup2(fd4, 4)
        os.dup2(fd5, 5)
        dist_dir = os.environ.get("DIST_DIR", os.path.split(os.path.abspath(__file__))[0])
        os.chdir('/')
        bios = ()
        if os.path.exists(dist_dir+'/bios.bin'):
            bios = ('-bios', dist_dir+'/bios.bin')
        os.execlp("qemu-system-i386", "qemu-system-i386", "-m", "4096", "-enable-kvm", "-drive", "file=%s/rmboot.img,format=raw"%dist_dir, "-device", "loader,file=/dev/fd/4,addr=0x1000000,force-raw=on", "-device", "loader,file=%s/kernel.elf"%dist_dir, "-debugcon", "file:/dev/fd/3", "-monitor", "stdio", "-nographic", "-serial", "null", "-s", *bios)
    for i in (fd0, fd1, fd3, fd4, fd5): os.close(i)
    b = b''
    while not b.endswith(b'\n'): b += os.read(done_fd, 1)
    os.close(done_fd)
    assert b in (b'DONE\n', b'ERROR\n'), b
    start_addr = 0xc1000000
    cmd = ('memsave 0x%x 0x%x ./dev/fd/5\nquit\n'%(start_addr, len(bundle))).encode('ascii')
    assert os.write(monitor_in, cmd) == len(cmd)
    os.close(monitor_in)
    with open(bundle_out, 'rb') as file:
        ans = file.read(len(bundle))
    assert not os.waitpid(pid, 0)[1]
    return (b, ans)

def run(b):
    status0, data = do_run(b.dump())
    ans = []
    for i in b.tests:
        status = int.from_bytes(data[i.outcome_addr:i.outcome_addr+4], 'little')
        outputs = []
        for j in i.files:
            if j.access == bundle.File.READWRITE:
                sz = int.from_bytes(data[j.sz_addr:j.sz_addr+4], 'little')
                content = data[j.data_addr:j.data_addr+sz]
                outputs.append(content)
        ans.append((status, outputs))
    return (status0.decode('ascii').strip(), ans)

if __name__ == '__main__':
    b = pack.pack_dir(sys.argv[1])
    s, d = run(b)
    print('Status:', s)
    for i, x in enumerate(d):
        print('###### Test #%d: outcome %d ######'%(i+1, x[0]))
        for j, y in enumerate(x[1]):
            print('Output #%d (size %d):'%(j, len(y)))
            sys.stdout.flush()
            sys.stdout.buffer.write(y)
            print()
