use std::vec::Vec;
use std::string::String;
use std::fs;
use std::io::Read;
use std::result::Result;
use crate::error::JjsKvmError;
use crate::bundle;

fn pack_fs(d: &String, p: &String, fszlim: u32, out: &mut Vec<bundle::File>, allnones: bool) -> Result<(), Box<dyn std::error::Error> > {
    match fs::read_link(&d) {
        Ok(pp) => {
            if pp.to_str().unwrap_or("") != "/dev/stdout" {
                return Err(Box::new(JjsKvmError::new(format!("{} does not point to /dev/stdout", d).as_str())));
            }
            let mut p = p.clone();
            p.pop();
            out.push(bundle::File::new(&p, bundle::FileType::ReadWrite, &if allnones { None } else { Some(vec![0 as u8; fszlim as usize]) }, 0));
            return Ok(());
        },
        Err(_) => {}
    };
    match fs::read_dir(&d) {
        Ok(l) => {
            for i in l {
                match i {
                    Err(_) => continue,
                    _ => {}
                }
                let i = i.unwrap().path();
                let i = i.file_name();
                if i == None {
                    continue;
                }
                let i = i.unwrap().to_str();
                if i == None {
                    continue;
                }
                let i = i.unwrap();
                if i.chars().next() != Some('.') {
                    pack_fs(&(d.to_owned()+"/"+i), &(p.to_owned()+i+"/"), fszlim, out, allnones)?;
                }
            }
            return Ok(());
        },
        Err(_) => {}
    };
    if allnones {
        let mut p = p.clone();
        p.pop();
        out.push(bundle::File::new(&p, bundle::FileType::ReadOnly, &None, 0u32));
    } else {
        let mut file = fs::File::open(d)?;
        let mut file_data = Vec::<u8>::new();
        let l = file.read_to_end(&mut file_data).unwrap();
        let mut p = p.clone();
        p.pop();
        out.push(bundle::File::new(&p, bundle::FileType::ReadOnly, &Some(file_data), l as u32));
    }
    Ok(())
}

pub fn pack_dir(base_dir: &String) -> Result<bundle::Bundle, Box<dyn std::error::Error> > {
    let base_dir : String = base_dir.to_string();
    let mut rootfs = Vec::<bundle::File>::new();
    pack_fs(&(base_dir.clone()+"/root"), &"/".to_string(), 0, &mut rootfs, false)?;
    pack_fs(&(base_dir.clone()+"/cwd"), &"".to_string(), 0, &mut rootfs, false)?;
    let exe = std::fs::read_link(&(base_dir.clone()+"/exe"))?;
    let exe = match exe.to_str() {
        Some(x) => x,
        None => return Err(Box::new(JjsKvmError::new(format!("{} points to non-string", base_dir.clone()+"/exe").as_str())))
    };
    let mut data = Vec::<(u32, u64, u32, u32, String)>::new();
    for i in fs::read_dir(&base_dir)? {
        match i {
            Err(_) => continue,
            _ => {}
        }
        let i = i.unwrap().path();
        let i = i.file_name();
        if i == None {
            continue;
        }
        let i = i.unwrap().to_str();
        if i == None {
            continue;
        }
        let i = i.unwrap();
        if i.split("-").count() != 4 {
            continue;
        }
        let mut it = i.split("-");
        let idx = it.next().unwrap().parse::<u32>()?;
        let tl = it.next().unwrap().parse::<u64>()?;
        let ml = it.next().unwrap().parse::<u32>()?;
        let fszl = it.next().unwrap().parse::<u32>()?;
        data.push((idx, tl, ml, fszl, i.to_string()));
    }
    data.sort_unstable();
    let mut tests = Vec::<bundle::Test>::new();
    for (idx, (_, tl, ml, fszlim, dir)) in data.iter().enumerate() {
        let mut testfs = Vec::<bundle::File>::new();
        pack_fs(&(base_dir.clone()+"/"+&dir+"/root"), &"/".to_string(), *fszlim, &mut testfs, idx == 0)?;
        pack_fs(&(base_dir.clone()+"/"+&dir+"/cwd"), &"".to_string(), *fszlim, &mut testfs, idx == 0)?;
        let input_txt = std::fs::read_link(&(base_dir.clone()+"/"+&dir+"/input"))?;
        let input_txt = match input_txt.to_str() {
            Some(x) => x,
            None => return Err(Box::new(JjsKvmError::new(format!("{} points to non-string", base_dir.clone()+"/"+&dir+"/input").as_str())))
        };
        let output_txt = std::fs::read_link(&(base_dir.clone()+"/"+&dir+"/output"))?;
        let output_txt = match output_txt.to_str() {
            Some(x) => x,
            None => return Err(Box::new(JjsKvmError::new(format!("{} points to non-string", base_dir.clone()+"/"+&dir+"/output").as_str())))
        };
        tests.push(bundle::Test::new(*tl, *ml, input_txt.to_string(), output_txt.to_string(), testfs));
    }
    Ok(bundle::Bundle::new(rootfs, exe, tests))
}
