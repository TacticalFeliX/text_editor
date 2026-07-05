# CEdit - Terminal Text Editor in C

A lightweight terminal-based text editor written in C, supporting file editing, search, undo/redo, syntax highlighting, and efficient buffer management.

---

## Features

- Terminal-based text editing
- File open/save support
- Undo/Redo
- Search functionality
- Efficient text buffer
- Cross-platform C99 implementation

---

## Project Structure

```
text_editor/
├── docs/
│   ├── architecture.md
│   ├── buffer-design.md
│   ├── editor-design.md
│   ├── terminal-programming.md
│   └── undo-redo.md
|
├── src/
│   ├── main.c
│   ├── editor.c
│   ├── editor.h
│   ├── buffer.c
│   ├── buffer.h
│   ├── fileio.c
│   ├── fileio.h
│   ├── search.c
│   ├── search.h
│   ├── terminal.c
│   ├── terminal.h
│   ├── undo.c
│   └── undo.h
│
├── examples/
│   └── hello.c
└── README.md
```

---

# Requirements

## Windows

Recommended:

- MSYS2 (UCRT64)
- GCC 14+
- Windows 10/11

Download MSYS2:

https://www.msys2.org/

After installation, install GCC:

```bash
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc
```

---

## Verify Installation

Open Command Prompt or PowerShell and run:

```bash
gcc --version
```

Expected output:

```
gcc (Rev2, Built by MSYS2 project) 14.x.x
```

---

# Important Windows PATH Configuration

Make sure the following directory is added to your **PATH** environment variable:

```
C:\msys64\ucrt64\bin
```

Verify:

```cmd
where gcc
```

Expected:

```
C:\msys64\ucrt64\bin\gcc.exe
```

---

# Building the Project

## Windows (Command Prompt)

Compile using:

```cmd
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG -o cedit.exe ^
src\main.c ^
src\buffer.c ^
src\editor.c ^
src\fileio.c ^
src\search.c ^
src\terminal.c ^
src\undo.c
```

Alternatively, on a single line:

```cmd
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG -o cedit.exe src\main.c src\buffer.c src\editor.c src\fileio.c src\search.c src\terminal.c src\undo.c
```

---

## Windows (PowerShell)

PowerShell does **not** expand wildcards (`*.c`) in the same way as Bash.

Either compile by listing all files explicitly:

```powershell
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG -o cedit.exe `
src\main.c `
src\buffer.c `
src\editor.c `
src\fileio.c `
src\search.c `
src\terminal.c `
src\undo.c
```

or automatically expand all source files:

```powershell
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG `
-o cedit.exe `
(Get-ChildItem src -Filter *.c | ForEach-Object { $_.FullName })
```

---

## Linux / macOS

```bash
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -DNDEBUG -o cedit src/*.c
```

---

# Running

## Windows

```cmd
cedit.exe
```

## Linux

```bash
./cedit
```

---

# Cleaning

Windows:

```cmd
del cedit.exe
```

Linux:

```bash
rm cedit
```

---

# Compiler Flags

| Flag | Purpose |
|-------|---------|
| `-std=c99` | Compile as C99 |
| `-Wall` | Enable common warnings |
| `-Wextra` | Enable additional warnings |
| `-Wpedantic` | Strict ISO C compliance |
| `-Werror` | Treat warnings as errors |
| `-O2` | Optimize generated code |
| `-DNDEBUG` | Disable assertions |

---

# Common Issues

## gcc is not recognized

```
'gcc' is not recognized as an internal or external command
```

### Solution

Ensure

```
C:\msys64\ucrt64\bin
```

is included in your PATH.

Verify:

```cmd
where gcc
```

---

## Wildcard Error

```
cc1.exe: fatal error: src\*.c: Invalid argument
```

This occurs because some Windows shells do not expand `*.c` automatically.

Compile by listing each source file explicitly or use the PowerShell command shown above.

---

## Undefined Reference Errors

Example:

```
undefined reference to 'editor_init'
```

This happens when compiling only `main.c`.

Compile **all** source files together.

---

## Unsupported 16-bit Application

This usually indicates an invalid executable was produced due to an incorrect build process or incomplete compilation.

Delete the old executable and rebuild using the commands provided above.

---

# Development

Whenever new source files are added under `src/`, include them in the compilation command.

For larger projects, using a Makefile or CMake is recommended.

---

# Future Improvements

- Makefile support
- CMake support
- Windows package
- Syntax highlighting
- Multiple buffers
- Plugin support
- Clipboard integration
