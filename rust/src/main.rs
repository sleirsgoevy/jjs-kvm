mod error;
mod pack;
mod launch;
mod bundle;
use std::env;
use std::io::Write;

fn main() {
    let mut x = env::args();
    x.next().unwrap();
    let mut b = pack::pack_dir(&x.next().unwrap()).unwrap();
    let (s, d) = launch::run(&mut b).unwrap();
    println!("Status: {}", s);
    let mut i = 0;
    for x in d {
        let (status, outputs) = x;
        println!("###### Test #{}: outcome {} ######", i+1, status);
        let mut j = 0;
        for y in outputs {
            println!("Output #{} (size {}):", j, y.len());
            std::io::stdout().write(&y).unwrap();
            std::io::stdout().flush().unwrap();
            println!("");
            j += 1;
        }
        i += 1;
    }
}
