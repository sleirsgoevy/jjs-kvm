import bundle
x = bundle.File('test', bundle.File.READONLY, open('../kernel/userspace/test', 'rb').read(), len(open('../kernel/userspace/test', 'rb').read()))
input_txt = bundle.File('input.txt', bundle.File.READONLY, b'1 2\n', 4)
output_txt = bundle.File('output.txt', bundle.File.READWRITE, bytes(1000000), 0)
test = bundle.Test(1000000, 65536, 'input.txt', 'output.txt', [input_txt, output_txt])
the_bundle = bundle.Bundle([x], 'test', [test])
x = the_bundle.dump()
open('../bundle.bin', 'wb').write(x)
