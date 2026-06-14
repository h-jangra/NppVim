# NppVim Help

Welcome to NppVim! This plugin brings Vim-like modal editing to Notepad++.

## Getting Started

Press `ESC` to enter Normal Mode. From there, you can use standard Vim motions like `h`, `j`, `k`, `l` to move the cursor.

Use `:` to enter Command Mode for file operations and configuration.

## Configuration

NppVim uses two separate files for configuration to keep editing preferences and UI behavior organized.

### nppvim.rc (Vim Environment)

This is your primary startup script, similar to a `.vimrc` file. It is used for **Vim-specific settings**, custom mappings, and executing commands on startup. 

**Note:** Any command-mode command (e.g., `:w`, `:split`, `:command`) can be placed in this file.

#### Supported Options in .rc

- `set nu` / `set number` - Show absolute line numbers
- `set rnu` / `set relativenumber` - Show relative line numbers
- `set nu rnu` - Enable **Hybrid Mode** (Absolute current line, relative others)
- `set nonu` / `set nornu` - Disable line numbering
- `set tabstop=4` - Set tab width
- `set expandtab` - Use spaces instead of tabs
- `set wrap` - Enable word wrap
- `set cursorline` - Highlight the current line
- `set list` - Show whitespace characters
- `set scrolloff=5` - Keep lines above/below cursor
- `set hlsearch` / `set nohlsearch` - Search highlighting
- `set ignorecase` / `set smartcase`
- `set clipboard=unnamed`
- `set langmap=...` - Character translation for non-English layouts

#### Mappings in .rc

```vim
" Syntax: [n|v|i][nore]map <keys> <action>
nmap ; :
nnoremap <C-s> :w<CR>
command W w
```

### config.ini (UI & Shortcut Overrides)

This file stores settings managed via the **Configuration Dialog** (`Plugins -> NppVim -> Configuration Dialog`). These settings control how NppVim interacts with Notepad++ itself.

**Note:** Changes made in the Dialog box are saved to `config.ini` only. They are not added to your `.rc` file.

#### Settings in .ini / Dialog

- **Escape Key**: Choose between `Esc`, `jj`, `jk`, `kj`, or a custom sequence.
- **Keyboard Layouts**: Set English (Normal) and System/Other (Insert) layouts.
- **Clipboard Integration**: Toggle whether `d`, `c`, or `x` keys should update the Windows clipboard.
- **Shortcut Overrides**: Resolve conflicts with native Notepad++ shortcuts. When enabled, NppVim takes control of these keys in Normal/Visual mode:
    - **Ctrl+D**: Half-page down (Native: Duplicate Line)
    - **Ctrl+U**: Half-page up (Native: Convert to Lowercase)
    - **Ctrl+R**: Redo (Native: Redo)
    - **Ctrl+F**: Page forward (Native: Find)
    - **Ctrl+V**: Visual Block mode (Native: Paste)
    - **Ctrl+A**: Increment number (Native: Select All)
    - **Ctrl+X**: Decrement number (Native: Cut)
    - **Ctrl+O**: Jump backward (Native: Open File)
    - **Ctrl+I**: Jump forward

### Mappings

You can define custom mappings in your `nppvim.rc` file.

#### Syntax

```vim
nmap <keys> <action>
vmap <keys> <action>
imap <keys> <action>
nnoremap <keys> <action>
vnoremap <keys> <action>
```

#### Examples

```vim
" Change colon to semicolon for easier commands
nmap ; :

" Map Ctrl+S to save in Normal mode
nnoremap <C-s> :w<CR>

" Create a custom command alias
command W w
```

## Relative Line Numbers

NppVim provides a high-performance, themed relative numbering system.

## Keyboard Layout Switching

NppVim can automatically switch your Windows keyboard layout when entering and exiting Insert mode.

### Configuration Example

```vim
set keylayout " Enable switching
set normallayout=en-US
set insertlayout=ru-RU
```

Common Layout Codes:
- `en-US` (English - US)
- `en-GB` (English - UK)
- `ru-RU` (Russian)
- `hi-IN` (Hindi)
- `ko-KR` (Korean)
- `ja-JP` (Japanese)

---

## Registered Mappings and Commands
