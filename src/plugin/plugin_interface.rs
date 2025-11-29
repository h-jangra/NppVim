// This file is rust translation of https://github.com/npp-plugins/plugintemplate/blob/master/src/PluginInterface.h

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(dead_code)]

use std::os::raw::c_int;
pub use winapi::shared::minwindef::{BOOL, LPARAM, LRESULT, UINT, WPARAM};
pub use winapi::shared::windef::{HBITMAP, HICON, HWND};

use crate::plugin::scintilla::SCNotification;

pub type PFUNCGETNAME = Option<unsafe extern "C" fn() -> *const u16>;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct NppData {
    pub _nppHandle: HWND,
    pub _scintillaMainHandle: HWND,
    pub _scintillaSecondHandle: HWND,
}

pub type PFUNCSETINFO = Option<unsafe extern "C" fn(NppData)>;
pub type PFUNCPLUGINCMD = Option<unsafe extern "C" fn()>;
pub type PBENOTIFIED = Option<unsafe extern "C" fn(*mut SCNotification)>;
pub type PMESSAGEPROC = Option<unsafe extern "C" fn(UINT, WPARAM, LPARAM) -> LRESULT>;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ShortcutKey {
    pub _isCtrl: bool,
    pub _isAlt: bool,
    pub _isShift: bool,
    pub _key: u8,
}

pub const MENU_ITEM_SIZE: usize = 64;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct FuncItem {
    pub _itemName: [u16; MENU_ITEM_SIZE],
    pub _pFunc: PFUNCPLUGINCMD,
    pub _cmdID: c_int,
    pub _init2Check: bool,
    pub _pShKey: *mut ShortcutKey,
}

pub type PFUNCGETFUNCSARRAY = Option<unsafe extern "C" fn(*mut c_int) -> *mut FuncItem>;

extern "C" {
    pub fn setInfo(npp_data: NppData);
    pub fn getName() -> *const u16;
    pub fn getFuncsArray(nb_func: *mut c_int) -> *mut FuncItem;
    pub fn beNotified(notification: *mut SCNotification);
    pub fn messageProc(msg: UINT, w_param: WPARAM, l_param: LPARAM) -> LRESULT;
    pub fn isUnicode() -> BOOL;
}
