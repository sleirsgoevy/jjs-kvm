use std::vec::Vec;
use std::string::String;
use std::fs;
use std::io::Read;
use crate::bundle;

fn pack_fs(d: &String, p: &String, fszlim: u32, out: &mut Vec<bundle::File>) {
    match fs::read_link(&d) {
        Ok(pp) => {
            assert!(pp.to_str().unwrap() == "/dev/stdout");
            let mut p = p.clone();
            p.pop();
            out.push(bundle::File::new(&p, bundle::FileType::ReadWrite, &vec![0 as u8; fszlim as usize], 0));
            return;
        },
        Err(_) => {}
    };
    match fs::read_dir(&d) {
        Ok(l) => {
            for i in l {
                let i = i.unwrap().path();
                let i = i.file_name().unwrap().to_str().unwrap();
                if i.chars().next() != Some('.') {
                    pack_fs(&(d.to_owned()+"/"+i), &(p.to_owned()+i+"/"), fszlim, out);
                }
            }
            return;
        },
        Err(_) => {}
    };
    let mut file = fs::File::open(d).unwrap();
    let mut file_data = Vec::<u8>::new();
    let l = file.read_to_end(&mut file_data).unwrap();
    let mut p = p.clone();
    p.pop();
    out.push(bundle::File::new(&p, bundle::FileType::ReadOnly, &file_data, l as u32));
}

pub fn pack_dir(base_dir: &String) -> bundle::Bundle {
    let base_dir : String = base_dir.to_string();
    let mut rootfs = Vec::<bundle::File>::new();
    pack_fs(&(base_dir.clone()+"/root"), &"/".to_string(), 0, &mut rootfs);
    pack_fs(&(base_dir.clone()+"/cwd"), &"".to_string(), 0, &mut rootfs);
    let exe = std::fs::read_link(&(base_dir.clone()+"/exe")).unwrap();
    let exe = exe.to_str().unwrap();
    let mut data = Vec::<(u32, u64, u32, u32, String)>::new();
    for i in fs::read_dir(&base_dir).unwrap() {
        let i = i.unwrap().path();
        let i = i.file_name().unwrap().to_str().unwrap();
        if i.split("-").count() != 4 {
            continue;
        }
        let mut it = i.split("-");
        let idx = it.next().unwrap().parse::<u32>().unwrap();
        let tl = it.next().unwrap().parse::<u64>().unwrap();
        let ml = it.next().unwrap().parse::<u32>().unwrap();
        let fszl = it.next().unwrap().parse::<u32>().unwrap();
        data.push((idx, tl, ml, fszl, i.to_string()));
    }
    data.sort_unstable();
    let mut tests = Vec::<bundle::Test>::new();
    for (_, tl, ml, fszlim, dir) in data {
        let mut testfs = Vec::<bundle::File>::new();
        pack_fs(&(base_dir.clone()+"/"+&dir+"/root"), &"/".to_string(), fszlim, &mut testfs);
        pack_fs(&(base_dir.clone()+"/"+&dir+"/cwd"), &"".to_string(), fszlim, &mut testfs);
        let input_txt = std::fs::read_link(&(base_dir.clone()+"/"+&dir+"/input")).unwrap();
        let input_txt = input_txt.to_str().unwrap();
        let output_txt = std::fs::read_link(&(base_dir.clone()+"/"+&dir+"/output")).unwrap();
        let output_txt = output_txt.to_str().unwrap();
        tests.push(bundle::Test::new(tl, ml, input_txt.to_string(), output_txt.to_string(), testfs));
    }
    bundle::Bundle::new(rootfs, exe, tests)
}
