// This file is rust translation of https://github.com/npp-plugins/plugintemplate/blob/master/src/Sci_Position.h

// use std::os::raw::{c_long, c_ulong};
// use winapi::shared::minwindef::UINT;

pub type Sci_Position = isize;
pub type Sci_PositionU = usize;
pub type Sci_PositionCR = isize;

#[cfg(target_os = "windows")]
pub const SCI_METHOD: &str = "__stdcall";
#[cfg(not(target_os = "windows"))]
pub const SCI_METHOD: &str = "";
