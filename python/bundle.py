import io

class _BundleBuilder:
    def __init__(self):
        self.labels = {}
        self.labelrefs = {}
        self.out = io.BytesIO()
        self.pc = 0xc1000004
        self.counter = 0
    def resolve(self):
        for k, v in self.labelrefs.items():
            p = self.labels[v]
            if isinstance(p, bytes):
                self.out.seek(self.pc-0xc1000004)
                self.labels[v] = self.pc
                self.write(p+b'\0')
                p = self.labels[v]
            if isinstance(p, tuple):
                self.out.seek(self.pc-0xc1000004)
                self.align(p[1])
                self.labels[v] = self.pc
                self.write(p[0])
                p = self.labels[v]
            self.out.seek(k-0xc1000004)
            self.out.write(p.to_bytes(4, 'little'))
    def write_label(self, l):
        if l != None:
            self.labelrefs[self.pc] = l
        self.write(b'\0\0\0\0')
    def set_label(self, l=None):
        if l is None: l = self.get_label()
        self.labels[l] = self.pc
        return l
    def get_label(self):
        ans = self.counter
        self.counter += 1
        return ans
    def set_label_string(self, s):
        if isinstance(s, str): s = s.encode('utf-8')
        self.labels[s] = s
        return s
    def set_label_bytes(self, l, bundle, align):
        self.labels[l] = (bundle, align)
        return l
    def write(self, data):
        self.out.write(data)
        self.pc += len(data)
    def align(self, q):
        self.write(bytes((-self.pc)%q))

class Test:
    def __init__(self, tl, ml, input_txt, output_txt, files):
        self.tl = tl
        self.ml = ml
        self.input_txt = input_txt
        self.output_txt = output_txt
        self.files = list(files)
        self.outcome_addr = None
    def _dump(self, dumper, tgt_l):
        #l = dumper.get_label()
        dumper.write(self.tl.to_bytes(8, 'little'))
        dumper.write(self.ml.to_bytes(4, 'little'))
        dumper.write_label(dumper.set_label_string(self.input_txt))
        dumper.write_label(dumper.set_label_string(self.output_txt))
        dumper.write_label(tgt_l)
        self.outcome_addr = dumper.set_label()
        dumper.write(bytes(4)) #outcome

class File:
    READONLY = 0
    READWRITE = 1
    def __init__(self, name, access, data, sz):
        self.name = name
        self.access = access
        self.data = data
        self.sz = sz
        self.sz_addr = None
        self.data_addr = None
    def _dump(self, dumper, tgt_l):
        dumper.write_label(dumper.set_label_string(self.name))
        dumper.write(self.access.to_bytes(4, 'little'))
        self.data_addr = dumper.set_label_bytes(dumper.get_label(), self.data, 4)
        dumper.write_label(self.data_addr)
        self.sz_addr = dumper.set_label()
        dumper.write(self.sz.to_bytes(4, 'little'))
        dumper.write(len(self.data).to_bytes(4, 'little'))
        dumper.write_label(tgt_l)

class Bundle:
    def __init__(self, files, exe, tests):
        self.files = list(files)
        self.exe = exe
        self.tests = tests
    def _dump(self, dumper):
        dumper.write(len(self.tests).to_bytes(4, 'little'))
        rootfs_l = dumper.get_label() if self.files else None
        dumper.write_label(rootfs_l)
        dumper.write_label(dumper.set_label_string(self.exe))
        all_files = sum((i.files for i in self.tests), self.files)
        fs_ls = []
        for i in self.tests:
            if i.files:
                fs_l = dumper.get_label()
                fs_ls.append(fs_l)
                i._dump(dumper, fs_l)
            else:
                fs_ls.append(None)
                i._dump(dumper, rootfs_l)
        for i, fs_l in zip(self.tests, fs_ls):
            if fs_l is None: continue
            for f in i.files[:-1]:
                dumper.set_label(fs_l)
                fs_l = dumper.get_label()
                f._dump(dumper, fs_l)
            dumper.set_label(fs_l)
            i.files[-1]._dump(dumper, rootfs_l)
        fs_l = rootfs_l
        for f in self.files[:-1]:
            dumper.set_label(fs_l)
            fs_l = dumper.get_label()
            f._dump(dumper, fs_l)
        if self.files:
            dumper.set_label(fs_l)
            self.files[-1]._dump(dumper, None)
        dumper.resolve()
        for i in all_files:
            i.sz_addr = dumper.labels[i.sz_addr] - 0xc1000000
            i.data_addr = dumper.labels[i.data_addr] - 0xc1000000
        for i in self.tests:
            i.outcome_addr = dumper.labels[i.outcome_addr] - 0xc1000000
    def dump(self):
        dumper = _BundleBuilder()
        self._dump(dumper)
        ans = dumper.out.getvalue()
        return len(ans).to_bytes(4, 'little')+ans
