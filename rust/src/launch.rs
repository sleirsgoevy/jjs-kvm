use crate::bundle;
use crate::error;
use nix::Error;
use std::result::Result;
use std::vec::Vec;

extern crate kvm_bindings;
extern crate kvm_ioctls;
extern crate memmap;

fn do_run(bundle: &mut [u8]) -> std::result::Result<std::vec::Vec<u8>, anyhow::Error> {
    let kernel = std::include_bytes!("../../kernel.bin");
    let kvm = kvm_ioctls::Kvm::new()?;
    assert_eq!(kvm.get_api_version(), 12);
    let vm = kvm.create_vm()?;
    vm.create_irq_chip().unwrap();
    vm.create_pit2(kvm_bindings::kvm_pit_config { flags: 0, pad: [0u32; 15] }).unwrap();
    let mut memory = memmap::MmapMut::map_anon(0x40000000)?;
    unsafe {
        vm.set_user_memory_region(kvm_bindings::kvm_userspace_memory_region { slot: 0, guest_phys_addr: 0x1000, memory_size: 0x40000000, userspace_addr: memory.as_ptr() as u64, flags: 0 })?;
    };
    memory[0..kernel.len()].copy_from_slice(kernel);
    memory[0xfff000..0xfff000+bundle.len()].copy_from_slice(bundle);
    let vcpu = vm.create_vcpu(0)?;
    vcpu.set_cpuid2(&kvm.get_supported_cpuid(kvm_bindings::KVM_MAX_CPUID_ENTRIES)?)?;
    let mut sregs = vcpu.get_sregs()?;
    sregs.cr0 |= 1;
    sregs.cs.base = 0;
    sregs.cs.limit = 0xfffffu32;
    sregs.cs.g = 1;
    sregs.cs.db = 1;
    sregs.ds.base = 0;
    sregs.ds.limit = 0xfffffu32;
    sregs.cs.g = 1;
    sregs.cs.db = 1;
    sregs.es = sregs.ds;
    sregs.ss = sregs.ds;
    vcpu.set_sregs(&sregs)?;
    let mut regs = vcpu.get_regs()?;
    regs.rip = 0x1000;
    regs.rflags = 2;
    vcpu.set_regs(&regs)?;
    let mut output = std::vec::Vec::<u8>::new();
    loop {
        let q = vcpu.run()?;
        match q {
            kvm_ioctls::VcpuExit::IoOut(port, data) => {
                if port != 0xe9 {
                    (Err::<(), error::JjsKvmError>(error::JjsKvmError::new(&std::format!("output to port other than debug serial"))))?;
                }
                //std::io::stdout().write_all(&data)?;
                output.extend(data);
                if output.len() != 0 && output[output.len() - 1] == ('\n' as u8) {
                    bundle.copy_from_slice(&memory[0xfff000..0xfff000+bundle.len()]);
                    return Ok(output);
                }
            },
            _ => (Err::<(), error::JjsKvmError>(error::JjsKvmError::new(&std::format!("unknown exit reason: {:?}", q))))?,
        };
    };
}

pub fn run(b: &mut bundle::Bundle) -> Result<(String, Vec<(u32, Vec<Vec<u8>>)>), Error> {
    let mut data = b.dump().clone();
    let status0 = do_run(&mut data).unwrap();
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
