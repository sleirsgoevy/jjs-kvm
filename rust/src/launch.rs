use std::fs;
use std::os::unix::io::FromRawFd;
use std::os::unix::io::IntoRawFd;
use std::ffi;
use std::vec::Vec;
use std::result::Result;
use std::io::Read;
use std::io::Write;
use std::io::Seek;
use std::io::Error;
use crate::bundle;

#[repr(C)]
struct PipeFds(i32, i32);

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
}

fn do_run(bundle: Vec<u8>) -> Result<(Vec<u8>, Vec<u8>), Error> {
    let mut stubs = Vec::<i32>::new();
    unsafe {
        for i in 0..5 {
            let x = dup(i);
            if x < 0 {
                return Result::Err(Error::last_os_error());
            }
            stubs.push(x);
        }
    }
    let mut memfd: i32;
    unsafe {
        let raws = ffi::CString::new("bundle.bin").unwrap().into_raw();
        memfd = memfd_create(raws as *const u8, 2);
        ffi::CString::from_raw(raws);
        let mut f = fs::File::from_raw_fd(memfd);
        f.write(&bundle).unwrap();
        f.flush().unwrap();
        f.seek(std::io::SeekFrom::Start(0)).unwrap();
        memfd = f.into_raw_fd();
    }
    let mut monitor_in = PipeFds(0, 0);
    let fd1: i32;
    unsafe {
        if pipe(&mut monitor_in) != 0 {
            return Result::Err(Error::last_os_error());
        }
        fd1 = fs::File::open("/dev/null").unwrap().into_raw_fd();
    }
    let PipeFds(fd0, monitor_in) = monitor_in;
    let mut done_fd = PipeFds(0, 0);
    unsafe {
        if pipe(&mut done_fd) != 0 {
            return Result::Err(Error::last_os_error());
        }
    }
    let PipeFds(done_fd, fd3) = done_fd;
    let fd4 = memfd;
    let mut bundle_out = PipeFds(0, 0);
    unsafe {
        if pipe(&mut bundle_out) != 0 {
            return Result::Err(Error::last_os_error());
        }
    }
    let PipeFds(bundle_out, fd5) = bundle_out;
    let pid: i32;
    unsafe {
        pid = fork();
        if pid < 0 {
            return Result::Err(Error::last_os_error());
        }
    }
    if pid == 0 {
        unsafe {
            if dup2(fd0, 0) != 0
            || dup2(fd1, 1) != 1
            || dup2(fd3, 3) != 3
            || dup2(fd4, 4) != 4
            || dup2(fd5, 5) != 5 {
                return Result::Err(Error::last_os_error());
            }
        }
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
        let mut args = vec!["qemu-system-i386", "-m", "4096", "-enable-kvm", "-drive", fmt1.as_str(), "-device", "loader,file=/dev/fd/4,addr=0x1000000,force-raw=on", "-device", fmt2.as_str(), "-debugcon", "file:/dev/fd/3", "-monitor", "stdio", "-nographic", "-serial", "null", "-s"];
        #[allow(unused_mut)]
        let mut fmt3: String;
        if std::path::Path::new(&format!("{}/bios.bin", dist_dir)).exists() {
            args.push("-bios");
            fmt3 = format!("{}/bios.bin", dist_dir);
            args.push(fmt3.as_str());
        }
        let mut ffi_args = Vec::<*const u8>::new();
        for i in args {
            ffi_args.push(ffi::CString::new(i).unwrap().into_raw() as *const u8);
        }
        unsafe {
            let argv = ffi_args.as_mut_ptr();
            std::mem::forget(argv);
            execvp(ffi::CString::new("qemu-system-i386").unwrap().into_raw() as *const u8, argv);
        }
        panic!("execv() failed");
    }
    for i in vec![fd0, fd1, fd3, fd4, fd5] {
        unsafe {
            close(i);
        }
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
                return Result::Err(Error::new(std::io::ErrorKind::UnexpectedEof, "done_fd eof"));
            }
            b.push(x[0]);
            if x[0] == ('\n' as u8) {
                break;
            }
        }
    }
    let start_addr = 0xc1000000 as u32;
    let cmd = format!("memsave 0x{:x} 0x{:x} ./dev/fd/5\nquit\n", start_addr, bundle.len());
    let cmd = cmd.as_bytes();
    {
        let mut monitor_f: fs::File;
        unsafe {
            monitor_f = fs::File::from_raw_fd(monitor_in);
        }
        if monitor_f.write(&cmd).unwrap() != cmd.len() {
            return Result::Err(Error::new(std::io::ErrorKind::UnexpectedEof, "monitor_in partial write"));
        }
    }
    let mut bundle_out_f: fs::File;
    unsafe {
        bundle_out_f = fs::File::from_raw_fd(bundle_out);
    }
    let mut ans = Vec::<u8>::new();
    bundle_out_f.read_to_end(&mut ans).unwrap();
    let mut status = 0 as i32;
    unsafe {
        if waitpid(pid, &mut status, 0) != pid {
            return Result::Err(Error::last_os_error());
        }
    }
    if status != 0 {
        return Result::Err(Error::new(std::io::ErrorKind::Other, "qemu failed"));
    }
    Result::Ok((b, ans))
}

pub fn run(b: &mut bundle::Bundle) -> Result<(String, Vec<(u32, Vec<Vec<u8>>)>), Error>{
    let data0 = b.dump();
    let (status0, data) = do_run(data0.clone()).unwrap();
    let mut ans = Vec::<(u32, Vec<Vec<u8>>)>::new();
    for i in &b.tests {
        let mut status = 0 as u32;
        for j in 0..4 {
            status = (status << 8) | (data[(i.outcome_addr+3-j) as usize] as u32);
        }
        let mut outputs = Vec::<Vec<u8>>::new();
        for j in &i.fs {
            match j.access {
                bundle::FileType::ReadWrite => {
                    let mut sz = 0 as u32;
                    for k in 0..4 {
                        sz = (sz << 8) | (data[(j.sz_addr+3-k) as usize] as u32);
                    }
                    let content = &data[j.data_addr as usize..(j.data_addr+sz) as usize];
                    let content = content.to_vec();
                    outputs.push(content);
                },
                bundle::FileType::ReadOnly => {}
            }
        }
        ans.push((status, outputs));
    }
    Result::Ok((std::str::from_utf8(&status0).unwrap_or("WTF\n").to_string(), ans))
}
