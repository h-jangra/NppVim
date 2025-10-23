# NppVim Plugin

### Core Files
- **NppVim.h** - Central state management for the plugin
  - Contains all mode enums, operation types, and state variables

- **NppVim.cpp** - Main plugin entry point
  - DLL exports and Notepad++ interface
  - Scintilla window hooking
  - Routes key events to appropriate mode handlers

### Mode Handlers
- **NormalMode.h/cpp** - Normal mode implementation
  - All normal mode commands (hjkl, dw, yy, etc.)
  - Operator-motion combinations
  - Uses a key handler map for easy extension

- **VisualMode.h/cpp** - Visual mode implementation
  - Character and line visual modes
  - Visual selection operations
  - Motion handling in visual mode

- **CommandMode.h/cpp** - Command-line mode implementation
  - Handles `:` and `/` commands
  - Search functionality
  - File operations (:w, :q, :wq)

### Supporting Classes
- **Motion.h/cpp** - All cursor motion functions
  - Character, word, line movements
  - Search motions (f, F, t, T)
  - Paragraph and document navigation

- **TextObject.h/cpp** - Text object operations
  - Implements iw, aw, i", a(, etc.
  - Handles all bracket and quote matching
  - Applies operations to text objects

- **Utils.h/cpp** - Utility functions
  - Scintilla handle management
  - Status bar updates
  - Search highlighting
  - Clipboard operations
  - Word and bracket finding

## Adding New Keymaps

### To add a new Normal mode command:

1. Open **NormalMode.h** and add a handler declaration:
```cpp
void handleNewCommand(HWND hwndEdit, int count);
```

2. In **NormalMode.cpp**, add the implementation:
```cpp
void NormalMode::handleNewCommand(HWND hwndEdit, int count) {
    // Your implementation here
}
```

3. Register it in `setupKeyHandlers()`:
```cpp
keyHandlers['n'] = [this](HWND hwnd, int c) { handleNewCommand(hwnd, c); };
```

### To add a new Visual mode command:

Follow the same pattern in **VisualMode.h/cpp**:
```cpp
// In VisualMode.h
void handleNewVisualCommand(HWND hwndEdit, int count);

// In VisualMode.cpp
void VisualMode::handleNewVisualCommand(HWND hwndEdit, int count) {
    // Implementation
}

// In setupKeyHandlers()
keyHandlers['n'] = [this](HWND hwnd, int c) { handleNewVisualCommand(hwnd, c); };
```

### To add a new motion:

1. Add declaration in **Motion.h**:
```cpp
static void newMotion(HWND hwndEdit, int count);
```

2. Implement in **Motion.cpp**:
```cpp
void Motion::newMotion(HWND hwndEdit, int count) {
    // Motion logic
}
```

3. Call from mode handlers as needed

### To add a new text object:

1. Add to `TextObjectType` enum in **NppVim.h**
2. Implement in **TextObject.cpp** `getTextObjectBounds()` method
3. Add character mapping in `TextObject::apply()`


## Example: Adding a Custom Command

Let's add a command to duplicate the current line (Ctrl+D style):

```cpp
// In NormalMode.h
void handleDuplicateLine(HWND hwndEdit, int count);

// In NormalMode.cpp
void NormalMode::handleDuplicateLine(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
    for (int i = 0; i < count; ++i) {
        ::SendMessage(hwndEdit, SCI_LINEDUPLICATE, 0, 0);
    }
    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.recordLastOp(OP_MOTION, count, 'Y'); // Custom motion code
}

// In setupKeyHandlers()
keyHandlers['Y'] = [this](HWND hwnd, int c) { handleDuplicateLine(hwnd, c); };
```

Now pressing `Y` in normal mode will duplicate the current line, and `3Y` will duplicate it 3 times!
