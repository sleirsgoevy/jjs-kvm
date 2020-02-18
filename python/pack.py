#
# sys.argv[1] must be path to a directory with the following structure:
#
# /exe -> <executable path in virtual fs>
# /root/ <virtual fs root dir>
# /cwd/ <virtual fs current dir>
# /<testno>-<tl>-<ml>-<outszlim>/
#                                /root/ <virtual fs overlay root dir>
#                                /cwd/ <virtual fs overlay current dir>
#                                /input -> <input.txt path in virtual fs>
#                                /output -> <output.txt path in virtual fs>
#
# sys.argv[2] must point to a file where the bundle will be written
#
# To boot the bundle:
#
# cd kernel/
# #place the bundle in ../bundle.bin
# make qemu-kvm

import bundle, os, sys

def pack_fs(d, p, fszlim):
    if os.path.islink(d):
        assert os.readlink(d) == '/dev/stdout'
        return [bundle.File(p[:-1], bundle.File.READWRITE, bytes(fszlim), 0)]
    elif os.path.isdir(d):
        return sum((pack_fs(d+'/'+i, p+i+'/', fszlim) for i in os.listdir(d) if not i.startswith('.')), [])
    else:
        data = open(d, 'rb').read()
        return [bundle.File(p[:-1], bundle.File.READONLY, data, len(data))]

def pack_dir(base_dir):
    rootfs = pack_fs(base_dir+'/root', '/', -1)+pack_fs(base_dir+'/cwd', '', -1)
    exe = os.readlink(base_dir+'/exe')
    data = []
    for i in os.listdir(base_dir):
        if i.count('-') == 3:
            data.append(tuple(map(int, i.split('-')))+(i,))
    assert len(set(i[0] for i in data)) == len(data)
    data.sort()
    tests = []
    for _, tl, ml, fszlim, dir in data:
        fs = pack_fs(base_dir+'/'+dir+'/root', '/', fszlim)+pack_fs(base_dir+'/'+dir+'/cwd', '', fszlim)
        input_txt = os.readlink(base_dir+'/'+dir+'/input')
        output_txt = os.readlink(base_dir+'/'+dir+'/output')
        tests.append(bundle.Test(tl, ml, input_txt, output_txt, fs))
    return bundle.Bundle(rootfs, exe, tests)

if __name__ == '__main__':
    with open(sys.argv[2], 'wb') as file:
        file.write(pack_dir(sys.argv[1]).dump())
