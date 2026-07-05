# Buffer Design

> This document explains what a text buffer is, compares four major approaches,
> and explains which one tEdit uses for Version 1 and why.

---

## Table of Contents

1. [What Is a Text Buffer?](#what-is-a-text-buffer)
2. [The Problem We Are Solving](#the-problem-we-are-solving)
3. [Option 1: Array of Lines](#option-1-array-of-lines)
4. [Option 2: Gap Buffer](#option-2-gap-buffer)
5. [Option 3: Piece Table](#option-3-piece-table)
6. [Option 4: Rope](#option-4-rope)
7. [Comparison Table](#comparison-table)
8. [Decision: What tEdit Uses and Why](#decision-what-tEdit-uses-and-why)
9. [Data Structure Definition](#data-structure-definition)
10. [Complexity Analysis](#complexity-analysis)
11. [Memory Behaviour](#memory-behaviour)
12. [Upgrading in the Future](#upgrading-in-the-future)

---

## What Is a Text Buffer?

A text buffer is the data structure that holds the contents of a file in
memory while you are editing it.

When you open `main.c`, the bytes on disk get read into memory and stored in
a buffer. Every time you type a character, delete a word, or paste a line,
the buffer is mutated. When you save, the buffer is written back to disk.

The buffer is the most performance-critical data structure in an editor.
Every keypress causes at least one mutation. Every screen redraw reads every
visible line. Get the buffer wrong and the editor is either slow, buggy, or
both.

---

## The Problem We Are Solving

Text editing has specific access patterns that make it unusual as a data
structure problem:

1. **Insertions and deletions are local.** Most edits happen near the cursor.
   You rarely jump randomly throughout the file on every keypress.

2. **Insertions can be in the middle of large content.** If you are on line 500
   of a 10,000-line file and press a key, the data structure must handle
   "insert in the middle" efficiently.

3. **Reads are sequential.** Rendering reads lines top-to-bottom, one by one.
   Random access by line number is also common (Go to Line).

4. **Undo requires history.** Previous states of the buffer must somehow be
   reconstructable.

The question is: which data structure handles these access patterns best?

---

## Option 1: Array of Lines

### What it is

The file is split into individual lines. Each line is a `char *` string stored
in a dynamic array (a `char **`).

```
char **lines = [
  "int main(void) {\n",   ← lines[0]
  "    return 0;\n",       ← lines[1]
  "}\n",                   ← lines[2]
]
```

Inserting a character in the middle of a line means `realloc`-ing that line's
string and shifting bytes rightward from the insertion point.

Splitting a line (pressing Enter) means:
1. Allocating a new `char *` for the new line
2. Copying the text after the cursor to the new line
3. Inserting the new `char *` into the `lines` array (shifting all pointers
   after the cursor row rightward)

Merging lines (Backspace at start of line) is the reverse.

### Advantages

- **Conceptually simple.** Every operation is easy to reason about.
- **Line-by-line access is O(1).** `lines[row]` directly gives you the line.
- **Rendering is trivial.** Walk the array, print each string.
- **Debugging is easy.** You can print the entire buffer state in 5 lines.
- **Memory efficiency for sparse files.** Each line only uses as much memory
  as it needs (plus a small overhead).

### Disadvantages

- **Character-level insert/delete is O(n) per line.** Inserting in the middle
  of a 10,000-character line requires shifting ~10,000 bytes. For typical lines
  (< 200 chars) this is negligible.
- **Line insert/delete is O(n) for the lines array.** Inserting a line in the
  middle of a 100,000-line file requires shifting 50,000 pointers. This is
  noticeable but still very fast in practice (shifting 400KB of pointers
  takes microseconds).
- **Each line is a separate allocation.** A 50,000-line file has 50,000
  heap allocations. `malloc` is fast but this adds overhead.

### Complexity

| Operation               | Time      | Notes                                      |
|-------------------------|-----------|--------------------------------------------|
| Access line by index    | O(1)      | Direct pointer dereference                 |
| Insert char mid-line    | O(L)      | L = length of line                         |
| Delete char mid-line    | O(L)      | Shift remaining chars left                 |
| Insert new line         | O(N)      | N = total number of lines                  |
| Delete line             | O(N)      | Shift line pointers left                   |
| Render N visible lines  | O(N × L)  | Read and write each visible character      |

---

## Option 2: Gap Buffer

### What it is

The entire file is stored as one large contiguous `char` array with a "gap"
of unused bytes in the middle. The gap sits at the current cursor position.

```
Before: [ H e l l o _ _ _ _ W o r l d ]
         ← text →  ← gap →  ← text →
```

When you type a character, it fills one byte of the gap:

```
After 'X': [ H e l l o X _ _ _ W o r l d ]
```

When you move the cursor left, the gap moves left by copying characters:

```
Move left: [ H e l l _ _ _ _ o X W o r l d ]  ← gap shifted left
```

### Advantages

- **Insert at cursor is O(1).** Just write into the gap.
- **Delete at cursor is O(1).** Just expand the gap.
- **Used by real editors.** Emacs uses a gap buffer.

### Disadvantages

- **Moving the cursor far from the gap is O(n).** If the gap is at line 1 and
  you jump to line 500, you must move the entire gap. This can be slow on
  very large files.
- **Line access is O(n).** There are no line indices. To find line 500, you must
  scan forward from the start counting newlines.
- **More complex to implement.** The gap must be maintained correctly across
  every operation. Off-by-one errors are subtle.
- **Rendering is more complex.** You must handle the case where the gap falls
  in the middle of a visible line.
- **Not suited for undo of multi-line operations.** Saving the gap position
  and content for every undo record is fiddly.

### Complexity

| Operation               | Time      | Notes                                      |
|-------------------------|-----------|--------------------------------------------|
| Insert at cursor        | O(1)      | Only if gap is at cursor already           |
| Insert away from cursor | O(n)      | Must move gap first                        |
| Access line by number   | O(n)      | Must scan for newlines                     |
| Render visible lines    | O(n)      | Plus special case for the gap              |

---

## Option 3: Piece Table

### What it is

A piece table stores the file differently. Instead of storing the text itself,
it stores a sequence of *descriptors* pointing into two buffers:

- The **original buffer**: the file as it was when first opened (read-only)
- The **add buffer**: all new text ever inserted (append-only)

```
Original buffer: "Hello World"
Add buffer:      ""   (empty at first)

Piece table: [ { original, start=0, len=11 } ]
Means: "the document is: chars 0..10 of the original buffer"
```

When you insert "Beautiful " at position 6:

```
Add buffer: "Beautiful "
Piece table: [
  { original, start=0,  len=6  },   ← "Hello "
  { add,      start=0,  len=10 },   ← "Beautiful "
  { original, start=6,  len=5  },   ← "World"
]
```

Deletions shrink or split pieces. The original buffer is never modified.

### Advantages

- **Undo is cheap.** Undo is just restoring the previous piece table state.
  No text is ever duplicated.
- **Used by real editors.** VS Code originally used a piece table.
- **Insert/delete is O(log n)** with a balanced tree of pieces.
- **The original file is preserved.** This enables "show diff from last save"
  for free.

### Disadvantages

- **High complexity.** The piece table (especially with a tree) is one of the
  most complex data structures in common use in editors.
- **Character-level access is O(log n).** You must walk the piece chain to find
  a specific character position.
- **Debugging is hard.** The document's text is spread across two buffers
  and a piece chain. Visualising the actual content requires reconstruction.
- **Not beginner-friendly.** Explaining this to someone learning C is difficult
  until they are comfortable with linked lists and trees.

### Complexity

| Operation               | Time      | Notes                                      |
|-------------------------|-----------|--------------------------------------------|
| Insert                  | O(log n)  | With balanced tree of pieces               |
| Delete                  | O(log n)  | Split/merge pieces                         |
| Access char by offset   | O(log n)  | Walk piece chain                           |
| Access line by number   | O(log n)  | With line index                            |
| Undo                    | O(1)      | Restore previous piece table snapshot      |

---

## Option 4: Rope

### What it is

A rope is a binary tree where each leaf holds a small string fragment and each
internal node stores the total length of its left subtree. The "document" is
the in-order concatenation of all leaves.

```
            [len=11]
           /        \
       [len=5]    [len=6]
       "Hello"    " World"
```

Inserting at position 6 splits a leaf and inserts a new node. The tree
rebalances (like a B-tree or red-black tree) to keep operations O(log n).

### Advantages

- **Excellent for very large files.** Operations are O(log n) for any file size.
- **Concatenation is O(1).** Merging two ropes is just creating a new root node.
- **Used by real editors.** Used in some versions of xi-editor.

### Disadvantages

- **Very high implementation complexity.** A correct, efficient rope requires
  understanding self-balancing binary trees, careful memory management, and
  subtle edge cases.
- **High overhead for small files.** Each leaf is a tiny allocation. For a
  5-line file, a rope has more overhead than an array.
- **Cache-unfriendly.** Tree nodes are scattered in memory. Sequential reads
  (rendering) involve many pointer chases, which are slow on modern CPUs due
  to cache misses.
- **Absolute overkill for v1.** Files that stress a simpler data structure
  (millions of lines, megabytes of content) are not a v1 concern.

### Complexity

| Operation               | Time      | Notes                                      |
|-------------------------|-----------|--------------------------------------------|
| Insert                  | O(log n)  | Split leaf + rebalance                     |
| Delete                  | O(log n)  | Remove/shrink leaf + rebalance             |
| Concatenate two ropes   | O(1)      | New root node                              |
| Access char by offset   | O(log n)  | Walk tree                                  |
| Sequential read         | O(n)      | In-order traversal but cache-unfriendly    |

---

## Comparison Table

| Feature                    | Array of Lines | Gap Buffer | Piece Table | Rope    |
|----------------------------|----------------|------------|-------------|---------|
| Insert char at cursor      | O(L)           | O(1)       | O(log n)    | O(log n)|
| Insert new line            | O(N)           | O(n)       | O(log n)    | O(log n)|
| Access line by number      | O(1)           | O(n)       | O(log n)    | O(log n)|
| Sequential render          | O(N×L)         | O(n)       | O(n)        | O(n)    |
| Undo implementation        | Medium         | Hard       | Easy        | Medium  |
| Implementation complexity  | **Low**        | Medium     | High        | Very high|
| Memory overhead            | Low            | Low        | Low         | High    |
| Cache friendliness         | High           | Very high  | Medium      | Low     |
| Debuggability              | **Excellent**  | Medium     | Hard        | Hard    |
| Good for very large files  | No             | No         | Yes         | Yes     |

L = average line length (typically 30-100 chars)
N = total number of lines in the file

---

## Decision: What tEdit Uses and Why

**tEdit Version 1 uses the Array of Lines.**

Here is the reasoning:

### 1. The performance numbers are acceptable

The worst-case scenario for an array of lines is inserting a character in the
middle of a very long line in a very long file.

Let's measure:
- A 100,000-line file is a very large file.
- Inserting a line in the middle of it requires shifting 50,000 pointers.
- Each pointer is 8 bytes on a 64-bit system.
- Shifting 400,000 bytes with `memmove` takes roughly 0.1 milliseconds.
- That is imperceptibly fast to a human user.

For line lengths: typical source code lines are under 120 characters.
Shifting 60 bytes is measured in nanoseconds.

The cases where array-of-lines *is* slow (inserting into a 100,000-character
single line) are pathological cases that never occur in practice.

### 2. The learning value is maximised

An array of lines is the simplest possible representation of a text file. A
complete beginner in C can understand it immediately.

By starting here, we learn:
- Dynamic arrays (`realloc`, capacity management)
- String manipulation (shifting bytes, copying)
- Two-level indexing (line index, then character index within line)
- The relationship between the in-memory representation and the on-disk format

If we started with a piece table or rope, the complexity of the data structure
would overwhelm the learner before they had written a single editor feature.

### 3. It is upgradeable

The `buffer.h` public API is designed to hide the implementation. All callers
use functions like `buffer_insert_char(buf, row, col, c)`. They never touch
the internal arrays directly.

When we upgrade to a gap buffer or piece table in v2.0, we rewrite `buffer.c`
and the rest of the codebase needs no changes.

### 4. Real editors used this for years

The original Unix `ed` and `vi` editors used line arrays. They were fast enough
to be the dominant editors for decades. For a learning project, this is more
than sufficient.

---

## Data Structure Definition

```c
/*
 * Buffer — the central text storage structure.
 *
 * Stores the document as an array of heap-allocated strings.
 * Each string represents one line of text WITHOUT a terminating newline.
 * (The newline is implicit between adjacent lines.)
 *
 * The array is dynamically resized as lines are added or removed.
 *
 * Invariants that must always hold:
 *   1. lines[i] is always a valid null-terminated string for 0 <= i < line_count
 *   2. line_lengths[i] == strlen(lines[i]) for all valid i
 *   3. line_count <= line_capacity
 *   4. If line_count == 0, lines[0] == "" (the buffer is never truly empty)
 */
typedef struct Buffer
{
    char **lines;           /* Array of pointers, each pointing to one line's text  */
    int   *line_lengths;    /* Cached length of each line (avoids repeated strlen)  */
    int    line_count;      /* Number of lines currently in the buffer              */
    int    line_capacity;   /* Number of slots allocated in lines[] and line_lengths[] */
    int    modified;        /* 1 if the buffer has unsaved changes, 0 otherwise     */
} Buffer;
```

---

## Complexity Analysis

For a file with **N lines** and average line length **L**:

### Insert a character at (row, col)

1. `realloc(lines[row], line_lengths[row] + 2)` — O(L) worst case (memory copy)
2. `memmove` to shift chars right from col — O(L)
3. Write the character — O(1)
4. Update `line_lengths[row]` — O(1)

**Total: O(L)** — proportional to line length, not file size.

### Delete a character at (row, col)

1. `memmove` to shift chars left from col — O(L)
2. Update `line_lengths[row]` — O(1)
3. (Optional realloc to shrink — skipped for performance)

**Total: O(L)**

### Insert a new line after row r (user presses Enter)

1. `realloc(lines, (line_count + 1) * sizeof(char *))` — O(N) memory copy
2. `memmove` to shift line pointers from r+1 onward — O(N)
3. Allocate the new line string — O(L)
4. Copy the text after the cursor to the new line — O(L)
5. Truncate the current line at the cursor — O(1) (write '\0')

**Total: O(N + L)** — proportional to file length for the pointer shift.

For N = 100,000 lines, this is ~0.1ms. Imperceptibly fast.

### Delete a line (Backspace at start of line)

1. Append the deleted line's text to the previous line — O(L)
2. Free the deleted line string — O(1)
3. `memmove` to shift line pointers left — O(N)
4. Update counts — O(1)

**Total: O(N + L)**

### Access a line by index

```c
lines[row]   /* Direct array access */
```

**Total: O(1)** — this is the key advantage of line arrays.

---

## Memory Behaviour

### Initial allocation

When `buffer_new()` is called:
- `lines` is allocated for `BUFFER_INITIAL_CAPACITY = 64` line pointers
- `line_lengths` is allocated for 64 integers
- `lines[0]` is set to an empty string `""`

Memory used: `64 * (sizeof(char *) + sizeof(int)) = 64 * 12 = 768 bytes`

### Growth strategy

When the buffer grows beyond capacity, it doubles:

```
Capacity:  64 → 128 → 256 → 512 → 1024 → ...
```

Doubling gives amortised O(1) for append operations. The same strategy
as `std::vector` in C++.

### A 10,000-line file

- 10,000 `char *` pointers: 80,000 bytes
- 10,000 `int` lengths: 40,000 bytes
- 10,000 line strings at average 40 chars: 400,000 bytes
- Total: ~520,000 bytes ≈ **0.5 MB**

This is trivially small. tEdit's entire in-memory representation of a large
source file is smaller than a single JPEG image.

### Memory leaks

Every string allocated by `buffer_insert_line()` must be freed by
`buffer_delete_line()` or `buffer_free()`. The buffer subsystem owns
all its memory. Callers must not `free()` line strings directly.

---

## Upgrading in the Future

The buffer API in `buffer.h` is designed so that callers never see the
internal structure:

```c
/* Callers use these functions: */
int  buffer_insert_char(Buffer *buf, int row, int col, char c);
int  buffer_delete_char(Buffer *buf, int row, int col);
int  buffer_insert_line(Buffer *buf, int row, const char *text);
int  buffer_delete_line(Buffer *buf, int row);
const char *buffer_get_line(Buffer *buf, int row);
int  buffer_get_line_length(Buffer *buf, int row);

/* Callers never access buf->lines directly */
```

To upgrade to a gap buffer, piece table, or rope:

1. Change the `Buffer` struct fields in `buffer.h`
2. Rewrite all functions in `buffer.c`
3. Ensure the public API signatures remain unchanged
4. Run `bash scripts/run_tests.sh` — no other file should require modification

This is the power of encapsulation. The internal representation is an
implementation detail, not a public contract.
