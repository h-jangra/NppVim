#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(clippy::upper_case_acronyms)]

use std::cell::UnsafeCell;

use std::os::raw::c_int;
pub mod plugin;
use plugin::plugin_interface::FuncItem;
use plugin::scintilla::SCNotification;

struct FuncItemsWrapper(UnsafeCell<[FuncItem; 1]>);
unsafe impl Sync for FuncItemsWrapper {}

static mut NPP_DATA: plugin::plugin_interface::NppData = plugin::plugin_interface::NppData {
    _nppHandle: std::ptr::null_mut(),
    _scintillaMainHandle: std::ptr::null_mut(),
    _scintillaSecondHandle: std::ptr::null_mut(),
};

static FUNC_ITEMS: FuncItemsWrapper = FuncItemsWrapper(UnsafeCell::new([FuncItem {
    _itemName: [0; 64],
    _pFunc: Some(about_dialog),
    _cmdID: 0,
    _init2Check: false,
    _pShKey: std::ptr::null_mut(),
}]));

unsafe extern "C" fn about_dialog() {
    use winapi::um::winuser::{MessageBoxW, MB_OK};

    let msg: Vec<u16> = "Rust Plugin — OK!\0".encode_utf16().collect();
    let title: Vec<u16> = "About\0".encode_utf16().collect();

    MessageBoxW(std::ptr::null_mut(), msg.as_ptr(), title.as_ptr(), MB_OK);
}

fn set_item_name(dst: &mut [u16], text: &str) {
    let mut utf16: Vec<u16> = text.encode_utf16().collect();
    utf16.push(0);
    for (i, c) in utf16.into_iter().enumerate() {
        if i < dst.len() {
            dst[i] = c;
        }
    }
}

#[no_mangle]
pub extern "system" fn setInfo(npp_data: plugin::plugin_interface::NppData) {
    unsafe {
        NPP_DATA = npp_data;

        let ptr = (*FUNC_ITEMS.0.get()).as_mut_ptr();
        let first = &mut *ptr;
        set_item_name(&mut first._itemName, "About");
    }
}

#[no_mangle]
pub extern "system" fn getName() -> *const u16 {
    let name = "Rust Notepad++ Plugin\0";
    let wide_name: Vec<u16> = name.encode_utf16().collect();
    Box::into_raw(wide_name.into_boxed_slice()) as *const u16
}

#[no_mangle]
pub extern "system" fn getFuncsArray(nb_func: *mut c_int) -> *mut FuncItem {
    unsafe {
        if !nb_func.is_null() {
            *nb_func = 1;
        }
        (*FUNC_ITEMS.0.get()).as_mut_ptr()
    }
}

#[no_mangle]
pub extern "system" fn beNotified(notify_code: *mut SCNotification) {
    if notify_code.is_null() {
        return;
    }

    unsafe {
        let notification = &*notify_code;
        let code = notification.nmhdr.code;

        if code == plugin::notepad_plus_msgs::NPPN_READY as u32 {
        } else if code == plugin::notepad_plus_msgs::NPPN_FILEOPENED as u32 {
        } else if code == plugin::notepad_plus_msgs::NPPN_FILECLOSED as u32 {
        } else if code == plugin::notepad_plus_msgs::NPPN_BUFFERACTIVATED as u32 {
        }
    }
}

#[no_mangle]
pub extern "system" fn messageProc(
    _message: plugin::notepad_plus_msgs::UINT,
    _w_param: plugin::notepad_plus_msgs::WPARAM,
    _l_param: plugin::notepad_plus_msgs::LPARAM,
) -> plugin::notepad_plus_msgs::LRESULT {
    0
}

#[no_mangle]
pub extern "system" fn isUnicode() -> plugin::notepad_plus_msgs::BOOL {
    1
}

pub fn get_npp_handle() -> plugin::notepad_plus_msgs::HWND {
    unsafe { NPP_DATA._nppHandle }
}

pub fn get_scintilla_main_handle() -> plugin::notepad_plus_msgs::HWND {
    unsafe { NPP_DATA._scintillaMainHandle }
}

pub fn get_scintilla_second_handle() -> plugin::notepad_plus_msgs::HWND {
    unsafe { NPP_DATA._scintillaSecondHandle }
}
