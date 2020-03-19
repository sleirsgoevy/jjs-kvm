use std::error::Error;
use std::fmt;

#[derive(Debug)]
pub struct JjsKvmError(String);

impl fmt::Display for JjsKvmError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl Error for JjsKvmError {}

impl JjsKvmError {
    pub fn new(s: &str) -> JjsKvmError {
        JjsKvmError(s.to_string())
    }
}
