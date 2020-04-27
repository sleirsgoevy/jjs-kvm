use crate::bundle;
use nix::sys::wait::waitpid;
use nix::unistd::close;
use nix::unistd::dup2;
use nix::unistd::execvp;
use nix::unistd::pipe;
use nix::Error;
use std::ffi;
use std::fs;
use std::io::Read;
use std::io::Seek;
use std::io::Write;
use std::os::unix::io::FromRawFd;
use std::os::unix::io::IntoRawFd;
use std::result::Result;
use std::vec::Vec;

/*
extern {
    fn memfd_create(name: *const u8, flags: i32) -> i32;
    fn pipe(fds: &mut PipeFds) -> i32;
    fn dup(fd: i32) -> i32;
    fn dup2(fd1: i32, fd2: i32) -> i32;
    fn close(fd: i32);
    #[allow(deprecated)]
    fn fork() -> std::os::unix::raw::pid_t;
    fn execvp(name: *const u8, args: *const *const u8);
    #[allow(deprecated)]
    fn waitpid(pid: std::os::unix::raw::pid_t, status: &mut i32, flags: i32) -> std::os::unix::raw::pid_t;
}*/

const ERR_OTHER: i32 = 0xbeef;

fn do_run(bundle: Vec<u8>) -> Result<(Vec<u8>, Vec<u8>), Error> {
    let mut stubs = Vec::<i32>::new();
    for i in 0..5 {
        let x = nix::unistd::dup(i)?;
        stubs.push(x);
    }

    let memfd = unsafe {
        let raws = ffi::CString::new("bundle.bin").unwrap();
        let mfd = nix::sys::memfd::memfd_create(
            &raws,
            nix::sys::memfd::MemFdCreateFlag::MFD_ALLOW_SEALING,
        )?;
        let mut f = fs::File::from_raw_fd(mfd);
        f.write(&bundle).unwrap();
        f.flush().unwrap();
        f.seek(std::io::SeekFrom::Start(0)).unwrap();
        f.into_raw_fd()
    };
    let (fd0, monitor_in) = pipe()?;
    let fd1 = fs::File::open("/dev/null").unwrap().into_raw_fd();
    let (done_fd, fd3) = pipe()?;
    let fd4 = memfd;
    let (bundle_out, fd5) = pipe()?;
    let fork_result = nix::unistd::fork()?;
    let pid = match fork_result {
        nix::unistd::ForkResult::Child => {
            dup2(fd0, 0)?;
            dup2(fd1, 1)?;
            dup2(fd3, 3)?;
            dup2(fd4, 4)?;
            dup2(fd5, 5)?;

            let dist_dir = match std::env::var("DIST_DIR") {
                Ok(s) => s,
                Err(_) => {
                    let mut p = std::env::current_exe().unwrap();
                    p.pop();
                    p.to_str().unwrap().to_string()
                }
            };
            std::env::set_current_dir("/").unwrap();
            let fmt1 = format!("file={}/rmboot.img,format=raw", dist_dir);
            let fmt2 = format!("loader,file={}/kernel.elf", dist_dir);
            let mut args = vec![
                "qemu-system-i386",
                "-m",
                "4096",
                "-enable-kvm",
                "-drive",
                fmt1.as_str(),
                "-device",
                "loader,file=/dev/fd/4,addr=0x1000000,force-raw=on",
                "-device",
                fmt2.as_str(),
                "-debugcon",
                "file:/dev/fd/3",
                "-monitor",
                "stdio",
                "-nographic",
                "-serial",
                "null",
                "-s",
            ];
            #[allow(unused_mut)]
            let mut fmt3: String;
            if std::path::Path::new(&format!("{}/bios.bin", dist_dir)).exists() {
                args.push("-bios");
                fmt3 = format!("{}/bios.bin", dist_dir);
                args.push(fmt3.as_str());
            }
            let mut ffi_args = Vec::new();
            for i in args {
                ffi_args.push(Box::leak(ffi::CString::new(i).unwrap().into_boxed_c_str())
                    as &'static std::ffi::CStr);
            }
            match execvp(&ffi::CString::new("qemu-system-i386").unwrap(), &ffi_args)
                .expect("execvp() failed") {}
        }
        nix::unistd::ForkResult::Parent { child: pid } => pid,
    };
    for i in vec![fd0, fd1, fd3, fd4, fd5] {
        close(i)?;
    }
    let mut b = Vec::<u8>::new();
    {
        let mut done_f: fs::File;
        unsafe {
            done_f = fs::File::from_raw_fd(done_fd);
        }
        loop {
            let mut x = [0 as u8; 1];
            if done_f.read(&mut x).unwrap() != 1 {
                return Result::Err(Error::Sys(nix::errno::Errno::EINVAL));
            }
            b.push(x[0]);
            if x[0] == ('\n' as u8) {
                break;
            }
        }
    }
    let start_addr = 0xc1000000 as u32;
    let cmd = format!(
        "memsave 0x{:x} 0x{:x} ./dev/fd/5\nquit\n",
        start_addr,
        bundle.len()
    );
    let cmd = cmd.as_bytes();
    {
        let mut monitor_f: fs::File;
        unsafe {
            monitor_f = fs::File::from_raw_fd(monitor_in);
        }
        if monitor_f.write(&cmd).unwrap() != cmd.len() {
            return Result::Err(Error::Sys(nix::errno::Errno::from_i32(ERR_OTHER)));
        }
    }
    let mut bundle_out_f: fs::File;
    unsafe {
        bundle_out_f = fs::File::from_raw_fd(bundle_out);
    }
    let mut ans = Vec::<u8>::new();
    bundle_out_f.read_to_end(&mut ans).unwrap();
    let status = waitpid(pid, None)?;
    match status {
        nix::sys::wait::WaitStatus::Exited(_, code) => {
            if code != 0 {
                return Err(Error::Sys(nix::errno::Errno::from_i32(ERR_OTHER)));
            }
        }
        _ => return Err(Error::Sys(nix::errno::Errno::from_i32(ERR_OTHER))),
    }
    Ok((b, ans))
}

pub fn run(b: &mut bundle::Bundle) -> Result<(String, Vec<(u32, Vec<Vec<u8>>)>), Error> {
    let data0 = b.dump();
    let (status0, data) = do_run(data0.clone()).unwrap();
    let mut ans = Vec::<(u32, Vec<Vec<u8>>)>::new();
    let it = &mut b.tests.iter();
    it.next();
    for i in it {
        let mut status = 0 as u32;
        for j in 0..4 {
            status = (status << 8) | (data[(i.outcome_addr + 3 - j) as usize] as u32);
        }
        let mut outputs = Vec::<Vec<u8>>::new();
        for j in &i.fs {
            match j.access {
                bundle::FileType::ReadWrite => {
                    let mut sz = 0 as u32;
                    for k in 0..4 {
                        sz = (sz << 8) | (data[(j.sz_addr + 3 - k) as usize] as u32);
                    }
                    let content = &data[j.data_addr.unwrap() as usize..(j.data_addr.unwrap() + sz) as usize];
                    let content = content.to_vec();
                    outputs.push(content);
                }
                bundle::FileType::ReadOnly => {}
            }
        }
        ans.push((status, outputs));
    }
    Result::Ok((
        std::str::from_utf8(&status0).unwrap_or("WTF\n").to_string(),
        ans,
    ))
}
