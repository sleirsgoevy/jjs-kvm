use std::string::String;
use std::vec::Vec;
use std::collections::HashMap;

#[derive(Debug)]
enum Label {
    Addr(u32),
    Bytes(Vec<u8>, u32),
    Inode(),
}

struct BundleBuilder {
    labels: HashMap<u32, Label>,
    labelrefs: HashMap<u32, u32>,
    out: Vec<u8>,
    counter: u32,
    inode: u32
}

impl BundleBuilder {
    pub fn new() -> BundleBuilder {
        BundleBuilder {
            labels: HashMap::new(),
            labelrefs: HashMap::new(),
            out: Vec::new(),
            counter: 0,
            inode: 179
        }
    }
    fn pc(&self) -> u32{
        (self.out.len() + 0xc1000004) as u32
    }
    pub fn resolve(&mut self) {
        for (k, v) in &self.labelrefs {
            let mut addr = match self.labels.get(v).unwrap() {
                Label::Bytes(data, q) => {
                    BundleBuilder::_align(&mut self.out, *q);
                    let cur = self.pc();
                    BundleBuilder::_write(&mut self.out, data);
                    self.labels.insert(*v, Label::Addr(cur));
                    cur
                },
                Label::Addr(x) => *x,
                Label::Inode() => {
                    let ans = self.inode;
                    self.inode += 1;
                    ans
                },
            };
            for i in k-0xc1000004..k-0xc1000000 {
                self.out[i as usize] = (addr & 0xff) as u8;
                addr >>= 8;
            }
        }
    }
    fn _align(out: &mut Vec<u8>, q: u32) {
        while (out.len() as u32) % q != 0 {
            out.push(0 as u8);
        }
    }
    #[allow(dead_code)]
    pub fn align(&mut self, q: u32) {
        BundleBuilder::_align(&mut self.out, q);
    }
    fn _write(out: &mut Vec<u8>, data: &[u8]) {
        out.extend(data);
    }
    pub fn write(&mut self, data: &[u8]) {
        BundleBuilder::_write(&mut self.out, data);
    }
    pub fn write_label(&mut self, lbl: u32) {
        self.labelrefs.insert(self.pc(), lbl);
        for _ in 0..4 {
            self.out.push(0 as u8);
        }
    }
    pub fn maybe_write_label(&mut self, lbl: Option<u32>) {
        match lbl {
            Some(lbl) => self.write_label(lbl),
            None => self.write(&vec![0 as u8; 4])
        };
    }
    pub fn set_label(&mut self, lbl: u32) {
        self.labels.insert(lbl, Label::Addr(self.pc()));
    }
    pub fn get_label(&mut self) -> u32 {
        let ans = self.counter;
        self.counter += 1;
        ans
    }
    pub fn set_label_string(&mut self, lbl: &str) -> u32 {
        let mut vec = lbl.as_bytes().to_vec();
        let lbl = self.get_label();
        vec.push(0);
        self.labels.insert(lbl, Label::Bytes(vec, 1));
        lbl
    }
    pub fn set_label_bytes(&mut self, data: Vec<u8>, q: u32) -> u32 {
        let lbl = self.get_label();
        self.labels.insert(lbl, Label::Bytes(data, q));
        lbl
    }
    pub fn set_label_inode(&mut self) -> u32 {
        let lbl = self.get_label();
        self.labels.insert(lbl, Label::Inode());
        lbl
    }
}

pub struct Test {
    tl: u64, //in microseconds
    ml: u32, //in pages (4096B each)
    input_txt: String,
    output_txt: String,
    pub fs: Vec<File>,
    pub outcome_addr: u32
}

impl Test {
    pub fn new(tl: u64, ml: u32, input_txt: String, output_txt: String, fs: Vec<File>) -> Test {
        Test {
            tl,
            ml,
            input_txt,
            output_txt,
            fs,
            outcome_addr: 0
        }
    }
    fn _dump(&mut self, dumper: &mut BundleBuilder, tgt_l: u32) {
        dumper.write(&(self.tl.to_ne_bytes()));
        dumper.write(&(self.ml.to_ne_bytes()));
        let l = dumper.set_label_string(&self.input_txt[..]);
        dumper.write_label(l);
        let l = dumper.set_label_string(&self.output_txt[..]);
        dumper.write_label(l);
        dumper.write_label(tgt_l);
        let l = dumper.get_label();
        dumper.set_label(l);
        self.outcome_addr = l;
        dumper.write(&[0 as u8; 4].to_vec());
    }
}

pub enum FileType {
    ReadOnly,
    ReadWrite
}

pub struct File {
    name: String,
    pub access: FileType,
    data: Option<Vec<u8>>,
    sz: u32,
    pub sz_addr: u32,
    pub data_addr: Option<u32>
}

impl File {
    pub fn new(name: &str, access: FileType, data: &Option<Vec<u8>>, sz: u32) -> File {
        File {
            name: name.to_string(),
            access: access,
            data: match data {
                Some(x) => Some(x.to_vec()),
                None => None
            },
            sz: sz,
            sz_addr: 0,
            data_addr: None
        }
    }
    fn _dump(&mut self, dumper: &mut BundleBuilder, tgt_l: Option<u32>) {
        let l = dumper.set_label_string(&self.name[..]);
        dumper.write_label(l);
        let l = dumper.set_label_inode();
        dumper.write_label(l);
        dumper.write(&(match &self.access {
            FileType::ReadOnly => 0u32,
            FileType::ReadWrite => 1u32
        }).to_ne_bytes());
        if let Some(x) = &self.data {
            self.data_addr = Some(dumper.set_label_bytes(x.to_vec(), 4));
            dumper.write_label(self.data_addr.unwrap());
        } else {
            self.data_addr = None;
            dumper.write(&[0 as u8; 4]);
        }
        self.sz_addr = dumper.get_label();
        dumper.set_label(self.sz_addr);
        dumper.write(&self.sz.to_ne_bytes());
        if let Some(x) = &self.data {
            dumper.write(&(x.len() as u32).to_ne_bytes());
        } else {
            dumper.write(&[0 as u8; 4]);
        }
        dumper.maybe_write_label(tgt_l);
    }
}

pub struct Bundle {
    files: Vec<File>,
    exe: String,
    pub tests: Vec<Test>
}

impl Bundle {
    pub fn new(files: Vec<File>, exe: &str, tests: Vec<Test>) -> Bundle {
        Bundle {
            files: files,
            exe: exe.to_string(),
            tests: tests
        }
    }
    fn _dump(&mut self, dumper: &mut BundleBuilder) {
        dumper.write(&((self.tests.len()-1) as u32).to_ne_bytes());
        let rootfs_l = dumper.get_label();
        //dumper.write_label(rootfs_l);
        let l = dumper.set_label_string(&self.exe[..]);
        dumper.write_label(l);
        let mut fs_ls = Vec::new();
        for i in &mut self.tests {
            if i.fs.len() != 0 {
                let fs_l = dumper.get_label();
                fs_ls.push(Some(fs_l));
                i._dump(dumper, fs_l);
            } else {
                fs_ls.push(None);
                i._dump(dumper, rootfs_l);
            }
        }
        for (i, fs_l) in self.tests.iter_mut().zip(fs_ls.iter_mut()) {
            match fs_l {
                None => {},
                Some(fs_l) => {
                    let mut fs_l = *fs_l;
                    if i.fs.len() >= 2 {
                        for fi in 0..(i.fs.len()-1) {
                            let f = &mut i.fs[fi];
                            dumper.set_label(fs_l);
                            fs_l = dumper.get_label();
                            f._dump(dumper, Some(fs_l));
                        }
                    }
                    dumper.set_label(fs_l);
                    let fi = i.fs.len()-1;
                    i.fs[fi]._dump(dumper, Some(rootfs_l));
                }
            };
        }
        let mut fs_l = rootfs_l;
        if self.files.len() >= 2 {
            for fi in 0..(self.files.len()-1) {
                let f = &mut self.files[fi];
                dumper.set_label(fs_l);
                fs_l = dumper.get_label();
                f._dump(dumper, Some(fs_l));
            }
        }
        if self.files.len() != 0 {
            dumper.set_label(fs_l);
            let fi = self.files.len()-1;
            self.files[fi]._dump(dumper, None);
        }
        dumper.resolve();
        for ii in &mut self.tests {
            for i in &mut ii.fs {
                i.sz_addr = match dumper.labels.get(&i.sz_addr).unwrap() {
                    Label::Addr(x) => x,
                    _ => panic!("tried to get addr of non-addr label!"),
                } - 0xc1000000;
                if let Some(da) = i.data_addr {
                    i.data_addr = Some(match dumper.labels.get(&da).unwrap() {
                        Label::Addr(x) => x,
                        _ => panic!("tried to get addr of non-addr label!")
                    } - 0xc1000000);
                }
            }
        }
        for i in &mut self.files {
            i.sz_addr = match dumper.labels.get(&i.sz_addr).unwrap() {
                Label::Addr(x) => x,
                _ => panic!("tried to get addr of non-addr label!")
            } - 0xc1000000;
            if let Some(da) = i.data_addr {
                i.data_addr = Some(match dumper.labels.get(&da).unwrap() {
                    Label::Addr(x) => x,
                    _ => panic!("tried to get addr of non-addr label!")
                } - 0xc1000000);
            }
        }
        for i in &mut self.tests {
            i.outcome_addr = match dumper.labels.get(&i.outcome_addr).unwrap() {
                Label::Addr(x) => x,
                _ => panic!("tried to get addr of non-addr label!")
            } - 0xc1000000;
        }
    }
    pub fn dump(&mut self) -> Vec<u8> {
        let mut dumper = BundleBuilder::new();
        self._dump(&mut dumper);
        let mut ans = Vec::<u8>::new();
        ans.extend(&(dumper.out.len() as u32).to_ne_bytes());
        ans.extend(&dumper.out);
        return ans;
    }
}
