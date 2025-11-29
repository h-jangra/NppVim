// This file is rust translation of https://github.com/npp-plugins/plugintemplate/blob/master/src/Notepad_plus_msgs.h

// use std::ffi::{CStr, CString};
use std::os::raw::{c_int, c_void};
// pub use winapi::shared::minwindef::{BOOL, HINSTANCE, LPARAM, UINT, WPARAM};
pub use winapi::shared::minwindef::{BOOL, LPARAM, LRESULT, UINT, WPARAM};
pub use winapi::shared::windef::{HBITMAP, HICON, HWND};

// use winapi::um::winnt::LANGID;

pub const NPPMSG: u32 = 1024; // WM_USER + 1000

// Language types
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LangType {
    L_TEXT = 0,
    L_PHP,
    L_C,
    L_CPP,
    L_CS,
    L_OBJC,
    L_JAVA,
    L_RC,
    L_HTML,
    L_XML,
    L_MAKEFILE,
    L_PASCAL,
    L_BATCH,
    L_INI,
    L_ASCII,
    L_USER,
    L_ASP,
    L_SQL,
    L_VB,
    L_JS_EMBEDDED,
    L_CSS,
    L_PERL,
    L_PYTHON,
    L_LUA,
    L_TEX,
    L_FORTRAN,
    L_BASH,
    L_FLASH,
    L_NSIS,
    L_TCL,
    L_LISP,
    L_SCHEME,
    L_ASM,
    L_DIFF,
    L_PROPS,
    L_PS,
    L_RUBY,
    L_SMALLTALK,
    L_VHDL,
    L_KIX,
    L_AU3,
    L_CAML,
    L_ADA,
    L_VERILOG,
    L_MATLAB,
    L_HASKELL,
    L_INNO,
    L_SEARCHRESULT,
    L_CMAKE,
    L_YAML,
    L_COBOL,
    L_GUI4CLI,
    L_D,
    L_POWERSHELL,
    L_R,
    L_JSP,
    L_COFFEESCRIPT,
    L_JSON,
    L_JAVASCRIPT,
    L_FORTRAN_77,
    L_BAANC,
    L_SREC,
    L_IHEX,
    L_TEHEX,
    L_SWIFT,
    L_ASN1,
    L_AVS,
    L_BLITZBASIC,
    L_PUREBASIC,
    L_FREEBASIC,
    L_CSOUND,
    L_ERLANG,
    L_ESCRIPT,
    L_FORTH,
    L_LATEX,
    L_MMIXAL,
    L_NIM,
    L_NNCRONTAB,
    L_OSCRIPT,
    L_REBOL,
    L_REGISTRY,
    L_RUST,
    L_SPICE,
    L_TXT2TAGS,
    L_VISUALPROLOG,
    L_TYPESCRIPT,
    L_JSON5,
    L_MSSQL,
    L_GDSCRIPT,
    L_HOLLYWOOD,
    L_GOLANG,
    L_RAKU,
    L_TOML,
    L_SAS,
    L_ERRORLIST,
    L_EXTERNAL,
}

// External lexer auto indent modes
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExternalLexerAutoIndentMode {
    Standard = 0,
    C_Like = 1,
    Custom = 2,
}

// Macro status
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MacroStatus {
    Idle = 0,
    RecordInProgress = 1,
    RecordingStopped = 2,
    PlayingBack = 3,
}

// Windows versions
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WinVer {
    WV_UNKNOWN = 0,
    WV_WIN32S,
    WV_95,
    WV_98,
    WV_ME,
    WV_NT,
    WV_W2K,
    WV_XP,
    WV_S2003,
    WV_XPX64,
    WV_VISTA,
    WV_WIN7,
    WV_WIN8,
    WV_WIN81,
    WV_WIN10,
    WV_WIN11,
}

// Platforms
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Platform {
    PF_UNKNOWN = 0,
    PF_X86,
    PF_X64,
    PF_IA64,
    PF_ARM64,
}

// View constants
pub const MAIN_VIEW: i32 = 0;
pub const SUB_VIEW: i32 = 1;

// File count types
pub const ALL_OPEN_FILES: i32 = 0;
pub const PRIMARY_VIEW: i32 = 1;
pub const SECOND_VIEW: i32 = 2;

// Modeless dialog actions
pub const MODELESSDIALOGADD: i32 = 0;
pub const MODELESSDIALOGREMOVE: i32 = 1;

// Menu types
pub const NPPPLUGINMENU: i32 = 0;
pub const NPPMAINMENU: i32 = 1;

// Status bar parts
pub const STATUSBAR_DOC_TYPE: i32 = 0;
pub const STATUSBAR_DOC_SIZE: i32 = 1;
pub const STATUSBAR_CUR_POS: i32 = 2;
pub const STATUSBAR_EOF_FORMAT: i32 = 3;
pub const STATUSBAR_UNICODE_TYPE: i32 = 4;
pub const STATUSBAR_TYPING_MODE: i32 = 5;

// Line number width modes
pub const LINENUMWIDTH_DYNAMIC: i32 = 0;
pub const LINENUMWIDTH_CONSTANT: i32 = 1;

// Document status flags
pub const DOCSTATUS_READONLY: i32 = 1;
pub const DOCSTATUS_BUFFERDIRTY: i32 = 2;

// Variable recognition constants
pub const VAR_NOT_RECOGNIZED: u32 = 0;
pub const FULL_CURRENT_PATH: u32 = 1;
pub const CURRENT_DIRECTORY: u32 = 2;
pub const FILE_NAME: u32 = 3;
pub const NAME_PART: u32 = 4;
pub const EXT_PART: u32 = 5;
pub const CURRENT_WORD: u32 = 6;
pub const NPP_DIRECTORY: u32 = 7;
pub const CURRENT_LINE: u32 = 8;
pub const CURRENT_COLUMN: u32 = 9;
pub const NPP_FULL_FILE_PATH: u32 = 10;
pub const GETFILENAMEATCURSOR: u32 = 11;
pub const CURRENT_LINESTR: u32 = 12;

// Message identifiers
pub const NPPM_GETCURRENTSCINTILLA: u32 = NPPMSG + 4;
pub const NPPM_GETCURRENTLANGTYPE: u32 = NPPMSG + 5;
pub const NPPM_SETCURRENTLANGTYPE: u32 = NPPMSG + 6;
pub const NPPM_GETNBOPENFILES: u32 = NPPMSG + 7;
pub const NPPM_GETOPENFILENAMES_DEPRECATED: u32 = NPPMSG + 8;
pub const NPPM_MODELESSDIALOG: u32 = NPPMSG + 12;
pub const NPPM_GETNBSESSIONFILES: u32 = NPPMSG + 13;
pub const NPPM_GETSESSIONFILES: u32 = NPPMSG + 14;
pub const NPPM_SAVESESSION: u32 = NPPMSG + 15;
pub const NPPM_SAVECURRENTSESSION: u32 = NPPMSG + 16;
pub const NPPM_GETOPENFILENAMESPRIMARY_DEPRECATED: u32 = NPPMSG + 17;
pub const NPPM_GETOPENFILENAMESSECOND_DEPRECATED: u32 = NPPMSG + 18;
pub const NPPM_CREATESCINTILLAHANDLE: u32 = NPPMSG + 20;
pub const NPPM_DESTROYSCINTILLAHANDLE_DEPRECATED: u32 = NPPMSG + 21;
pub const NPPM_GETNBUSERLANG: u32 = NPPMSG + 22;
pub const NPPM_GETCURRENTDOCINDEX: u32 = NPPMSG + 23;
pub const NPPM_SETSTATUSBAR: u32 = NPPMSG + 24;
pub const NPPM_GETMENUHANDLE: u32 = NPPMSG + 25;
pub const NPPM_ENCODESCI: u32 = NPPMSG + 26;
pub const NPPM_DECODESCI: u32 = NPPMSG + 27;
pub const NPPM_ACTIVATEDOC: u32 = NPPMSG + 28;
pub const NPPM_LAUNCHFINDINFILESDLG: u32 = NPPMSG + 29;
pub const NPPM_DMMSHOW: u32 = NPPMSG + 30;
pub const NPPM_DMMHIDE: u32 = NPPMSG + 31;
pub const NPPM_DMMUPDATEDISPINFO: u32 = NPPMSG + 32;
pub const NPPM_DMMREGASDCKDLG: u32 = NPPMSG + 33;
pub const NPPM_LOADSESSION: u32 = NPPMSG + 34;
pub const NPPM_DMMVIEWOTHERTAB: u32 = NPPMSG + 35;
pub const NPPM_RELOADFILE: u32 = NPPMSG + 36;
pub const NPPM_SWITCHTOFILE: u32 = NPPMSG + 37;
pub const NPPM_SAVECURRENTFILE: u32 = NPPMSG + 38;
pub const NPPM_SAVEALLFILES: u32 = NPPMSG + 39;
pub const NPPM_SETMENUITEMCHECK: u32 = NPPMSG + 40;
pub const NPPM_ADDTOOLBARICON_DEPRECATED: u32 = NPPMSG + 41;
pub const NPPM_GETWINDOWSVERSION: u32 = NPPMSG + 42;
pub const NPPM_DMMGETPLUGINHWNDBYNAME: u32 = NPPMSG + 43;
pub const NPPM_MAKECURRENTBUFFERDIRTY: u32 = NPPMSG + 44;
pub const NPPM_GETENABLETHEMETEXTUREFUNC_DEPRECATED: u32 = NPPMSG + 45;
pub const NPPM_GETPLUGINSCONFIGDIR: u32 = NPPMSG + 46;
pub const NPPM_MSGTOPLUGIN: u32 = NPPMSG + 47;
pub const NPPM_MENUCOMMAND: u32 = NPPMSG + 48;
pub const NPPM_TRIGGERTABBARCONTEXTMENU: u32 = NPPMSG + 49;
pub const NPPM_GETNPPVERSION: u32 = NPPMSG + 50;
pub const NPPM_HIDETABBAR: u32 = NPPMSG + 51;
pub const NPPM_ISTABBARHIDDEN: u32 = NPPMSG + 52;
pub const NPPM_GETPOSFROMBUFFERID: u32 = NPPMSG + 57;
pub const NPPM_GETFULLPATHFROMBUFFERID: u32 = NPPMSG + 58;
pub const NPPM_GETBUFFERIDFROMPOS: u32 = NPPMSG + 59;
pub const NPPM_GETCURRENTBUFFERID: u32 = NPPMSG + 60;
pub const NPPM_RELOADBUFFERID: u32 = NPPMSG + 61;
pub const NPPM_GETBUFFERLANGTYPE: u32 = NPPMSG + 64;
pub const NPPM_SETBUFFERLANGTYPE: u32 = NPPMSG + 65;
pub const NPPM_GETBUFFERENCODING: u32 = NPPMSG + 66;
pub const NPPM_SETBUFFERENCODING: u32 = NPPMSG + 67;
pub const NPPM_GETBUFFERFORMAT: u32 = NPPMSG + 68;
pub const NPPM_SETBUFFERFORMAT: u32 = NPPMSG + 69;
pub const NPPM_HIDETOOLBAR: u32 = NPPMSG + 70;
pub const NPPM_ISTOOLBARHIDDEN: u32 = NPPMSG + 71;
pub const NPPM_HIDEMENU: u32 = NPPMSG + 72;
pub const NPPM_ISMENUHIDDEN: u32 = NPPMSG + 73;
pub const NPPM_HIDESTATUSBAR: u32 = NPPMSG + 74;
pub const NPPM_ISSTATUSBARHIDDEN: u32 = NPPMSG + 75;
pub const NPPM_GETSHORTCUTBYCMDID: u32 = NPPMSG + 76;
pub const NPPM_DOOPEN: u32 = NPPMSG + 77;
pub const NPPM_SAVECURRENTFILEAS: u32 = NPPMSG + 78;
pub const NPPM_GETCURRENTNATIVELANGENCODING: u32 = NPPMSG + 79;
pub const NPPM_ALLOCATESUPPORTED_DEPRECATED: u32 = NPPMSG + 80;
pub const NPPM_ALLOCATECMDID: u32 = NPPMSG + 81;
pub const NPPM_ALLOCATEMARKER: u32 = NPPMSG + 82;
pub const NPPM_GETLANGUAGENAME: u32 = NPPMSG + 83;
pub const NPPM_GETLANGUAGEDESC: u32 = NPPMSG + 84;
pub const NPPM_SHOWDOCLIST: u32 = NPPMSG + 85;
pub const NPPM_ISDOCLISTSHOWN: u32 = NPPMSG + 86;
pub const NPPM_GETAPPDATAPLUGINSALLOWED: u32 = NPPMSG + 87;
pub const NPPM_GETCURRENTVIEW: u32 = NPPMSG + 88;
pub const NPPM_DOCLISTDISABLEEXTCOLUMN: u32 = NPPMSG + 89;
pub const NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR: u32 = NPPMSG + 90;
pub const NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR: u32 = NPPMSG + 91;
pub const NPPM_SETSMOOTHFONT: u32 = NPPMSG + 92;
pub const NPPM_SETEDITORBORDEREDGE: u32 = NPPMSG + 93;
pub const NPPM_SAVEFILE: u32 = NPPMSG + 94;
pub const NPPM_DISABLEAUTOUPDATE: u32 = NPPMSG + 95;
pub const NPPM_REMOVESHORTCUTBYCMDID: u32 = NPPMSG + 96;
pub const NPPM_GETPLUGINHOMEPATH: u32 = NPPMSG + 97;
pub const NPPM_GETSETTINGSONCLOUDPATH: u32 = NPPMSG + 98;
pub const NPPM_SETLINENUMBERWIDTHMODE: u32 = NPPMSG + 99;
pub const NPPM_GETLINENUMBERWIDTHMODE: u32 = NPPMSG + 100;
pub const NPPM_ADDTOOLBARICON_FORDARKMODE: u32 = NPPMSG + 101;
pub const NPPM_DOCLISTDISABLEPATHCOLUMN: u32 = NPPMSG + 102;
pub const NPPM_GETEXTERNALLEXERAUTOINDENTMODE: u32 = NPPMSG + 103;
pub const NPPM_SETEXTERNALLEXERAUTOINDENTMODE: u32 = NPPMSG + 104;
pub const NPPM_ISAUTOINDENTON: u32 = NPPMSG + 105;
pub const NPPM_GETCURRENTMACROSTATUS: u32 = NPPMSG + 106;
pub const NPPM_ISDARKMODEENABLED: u32 = NPPMSG + 107;
pub const NPPM_GETDARKMODECOLORS: u32 = NPPMSG + 108;
pub const NPPM_GETCURRENTCMDLINE: u32 = NPPMSG + 109;
pub const NPPM_CREATELEXER: u32 = NPPMSG + 110;
pub const NPPM_GETBOOKMARKID: u32 = NPPMSG + 111;
pub const NPPM_DARKMODESUBCLASSANDTHEME: u32 = NPPMSG + 112;
pub const NPPM_ALLOCATEINDICATOR: u32 = NPPMSG + 113;
pub const NPPM_GETTABCOLORID: u32 = NPPMSG + 114;
pub const NPPM_SETUNTITLEDNAME: u32 = NPPMSG + 115;
pub const NPPM_GETNATIVELANGFILENAME: u32 = NPPMSG + 116;
pub const NPPM_ADDSCNMODIFIEDFLAGS: u32 = NPPMSG + 117;
pub const NPPM_GETTOOLBARICONSETCHOICE: u32 = NPPMSG + 118;
pub const NPPM_GETNPPSETTINGSDIRPATH: u32 = NPPMSG + 119;

// RUNCOMMAND_USER messages
pub const RUNCOMMAND_USER: u32 = 0x400 + 3000; // WM_USER + 3000

pub const NPPM_GETFULLCURRENTPATH: u32 = RUNCOMMAND_USER + FULL_CURRENT_PATH;
pub const NPPM_GETCURRENTDIRECTORY: u32 = RUNCOMMAND_USER + CURRENT_DIRECTORY;
pub const NPPM_GETFILENAME: u32 = RUNCOMMAND_USER + FILE_NAME;
pub const NPPM_GETNAMEPART: u32 = RUNCOMMAND_USER + NAME_PART;
pub const NPPM_GETEXTPART: u32 = RUNCOMMAND_USER + EXT_PART;
pub const NPPM_GETCURRENTWORD: u32 = RUNCOMMAND_USER + CURRENT_WORD;
pub const NPPM_GETNPPDIRECTORY: u32 = RUNCOMMAND_USER + NPP_DIRECTORY;
pub const NPPM_GETNPPFULLFILEPATH: u32 = RUNCOMMAND_USER + NPP_FULL_FILE_PATH;
pub const NPPM_GETFILENAMEATCURSOR: u32 = RUNCOMMAND_USER + GETFILENAMEATCURSOR;
pub const NPPM_GETCURRENTLINESTR: u32 = RUNCOMMAND_USER + CURRENT_LINESTR;
pub const NPPM_GETCURRENTLINE: u32 = RUNCOMMAND_USER + CURRENT_LINE;
pub const NPPM_GETCURRENTCOLUMN: u32 = RUNCOMMAND_USER + CURRENT_COLUMN;

// Notification codes
pub const NPPN_FIRST: u32 = 1000;
pub const NPPN_READY: u32 = NPPN_FIRST + 1;
pub const NPPN_TBMODIFICATION: u32 = NPPN_FIRST + 2;
pub const NPPN_FILEBEFORECLOSE: u32 = NPPN_FIRST + 3;
pub const NPPN_FILEOPENED: u32 = NPPN_FIRST + 4;
pub const NPPN_FILECLOSED: u32 = NPPN_FIRST + 5;
pub const NPPN_FILEBEFOREOPEN: u32 = NPPN_FIRST + 6;
pub const NPPN_FILEBEFORESAVE: u32 = NPPN_FIRST + 7;
pub const NPPN_FILESAVED: u32 = NPPN_FIRST + 8;
pub const NPPN_SHUTDOWN: u32 = NPPN_FIRST + 9;
pub const NPPN_BUFFERACTIVATED: u32 = NPPN_FIRST + 10;
pub const NPPN_LANGCHANGED: u32 = NPPN_FIRST + 11;
pub const NPPN_WORDSTYLESUPDATED: u32 = NPPN_FIRST + 12;
pub const NPPN_SHORTCUTREMAPPED: u32 = NPPN_FIRST + 13;
pub const NPPN_FILEBEFORELOAD: u32 = NPPN_FIRST + 14;
pub const NPPN_FILELOADFAILED: u32 = NPPN_FIRST + 15;
pub const NPPN_READONLYCHANGED: u32 = NPPN_FIRST + 16;
pub const NPPN_DOCORDERCHANGED: u32 = NPPN_FIRST + 17;
pub const NPPN_SNAPSHOTDIRTYFILELOADED: u32 = NPPN_FIRST + 18;
pub const NPPN_BEFORESHUTDOWN: u32 = NPPN_FIRST + 19;
pub const NPPN_CANCELSHUTDOWN: u32 = NPPN_FIRST + 20;
pub const NPPN_FILEBEFORERENAME: u32 = NPPN_FIRST + 21;
pub const NPPN_FILERENAMECANCEL: u32 = NPPN_FIRST + 22;
pub const NPPN_FILERENAMED: u32 = NPPN_FIRST + 23;
pub const NPPN_FILEBEFOREDELETE: u32 = NPPN_FIRST + 24;
pub const NPPN_FILEDELETEFAILED: u32 = NPPN_FIRST + 25;
pub const NPPN_FILEDELETED: u32 = NPPN_FIRST + 26;
pub const NPPN_DARKMODECHANGED: u32 = NPPN_FIRST + 27;
pub const NPPN_CMDLINEPLUGINMSG: u32 = NPPN_FIRST + 28;
pub const NPPN_EXTERNALLEXERBUFFER: u32 = NPPN_FIRST + 29;
pub const NPPN_GLOBALMODIFIED: u32 = NPPN_FIRST + 30;
pub const NPPN_NATIVELANGCHANGED: u32 = NPPN_FIRST + 31;
pub const NPPN_TOOLBARICONSETCHANGED: u32 = NPPN_FIRST + 32;

// Structures

#[repr(C)]
#[derive(Debug, Clone)]
pub struct SessionInfo {
    pub session_file_path_name: *mut u16, // wchar_t* - Full session file path name
    pub nb_file: c_int,                   // Number of files
    pub files: *mut *mut u16,             // wchar_t** - Array of file names
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct ToolbarIcons {
    pub h_toolbar_bmp: HBITMAP,
    pub h_toolbar_icon: HICON,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct ToolbarIconsWithDarkMode {
    pub h_toolbar_bmp: HBITMAP,
    pub h_toolbar_icon: HICON,
    pub h_toolbar_icon_dark_mode: HICON,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct CommunicationInfo {
    pub internal_msg: c_int,         // Message identifier
    pub src_module_name: *const u16, // Source module name
    pub info: *mut c_void,           // Information pointer
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct ShortcutKey {
    pub is_ctrl: bool,
    pub is_alt: bool,
    pub is_shift: bool,
    pub key: u8, // UCHAR
}

// Dark mode constants and structures
pub mod npp_dark_mode {
    use winapi::shared::windef::COLORREF;

    // Standard flags for main parent after its children are initialized.
    pub const DMF_INIT: u32 = 0x0000000B;

    // Standard flags for main parent usually used in NPPN_DARKMODECHANGED.
    pub const DMF_HANDLE_CHANGE: u32 = 0x0000000C;

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct Colors {
        pub background: COLORREF,
        pub softer_background: COLORREF,
        pub hot_background: COLORREF,
        pub pure_background: COLORREF,
        pub error_background: COLORREF,
        pub text: COLORREF,
        pub darker_text: COLORREF,
        pub disabled_text: COLORREF,
        pub link_text: COLORREF,
        pub edge: COLORREF,
        pub hot_edge: COLORREF,
        pub disabled_edge: COLORREF,
    }

    impl Default for Colors {
        fn default() -> Self {
            Self {
                background: 0,
                softer_background: 0,
                hot_background: 0,
                pure_background: 0,
                error_background: 0,
                text: 0,
                darker_text: 0,
                disabled_text: 0,
                link_text: 0,
                edge: 0,
                hot_edge: 0,
                disabled_edge: 0,
            }
        }
    }
}

// Helper functions for string conversion
pub fn to_wide_string(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

pub fn from_wide_string(ptr: *const u16) -> String {
    if ptr.is_null() {
        return String::new();
    }

    let mut len = 0;
    unsafe {
        while *ptr.add(len) != 0 {
            len += 1;
        }

        let slice = std::slice::from_raw_parts(ptr, len);
        String::from_utf16_lossy(slice)
    }
}

// Type aliases for better readability
pub type BufferID = usize; // UINT_PTR in C++
pub type UniMode = i32; // Encoding mode
pub type EolType = i32; // End of line type

// Encoding modes (UniMode)
pub const UNI_ANSI: i32 = 0;
pub const UNI_UTF8_BOM: i32 = 1;
pub const UNI_UTF16BE_BOM: i32 = 2;
pub const UNI_UTF16LE_BOM: i32 = 3;
pub const UNI_UTF8: i32 = 4;
pub const UNI_7BIT: i32 = 5;
pub const UNI_UTF16BE: i32 = 6;
pub const UNI_UTF16LE: i32 = 7;

// EOL types
pub const EOL_TYPE_WIN: i32 = 0; // CRLF
pub const EOL_TYPE_MAC: i32 = 1; // CR
pub const EOL_TYPE_UNIX: i32 = 2; // LF
pub const EOL_TYPE_UNKNOWN: i32 = 3;

// Tab color IDs
pub const TAB_COLOR_YELLOW: i32 = 0;
pub const TAB_COLOR_GREEN: i32 = 1;
pub const TAB_COLOR_BLUE: i32 = 2;
pub const TAB_COLOR_ORANGE: i32 = 3;
pub const TAB_COLOR_PINK: i32 = 4;
pub const TAB_COLOR_NONE: i32 = -1;

// Toolbar icon set choices
pub const TOOLBAR_ICONSET_FLUENT_SMALL: i32 = 0;
pub const TOOLBAR_ICONSET_FLUENT_LARGE: i32 = 1;
pub const TOOLBAR_ICONSET_FLUENT_FILLED_SMALL: i32 = 2;
pub const TOOLBAR_ICONSET_FLUENT_FILLED_LARGE: i32 = 3;
pub const TOOLBAR_ICONSET_STANDARD_SMALL: i32 = 4;
