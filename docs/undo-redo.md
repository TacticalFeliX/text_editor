# Undo / Redo Design

> This document explains the undo/redo system: the data structures it uses,
> how records are created and replayed, and the tradeoffs involved.

---

## Table of Contents

1. [Why Undo Is Hard](#why-undo-is-hard)
2. [Approach Comparison](#approach-comparison)
3. [tEdit's Approach: Command Stack](#tEdits-approach-command-stack)
4. [UndoRecord Types](#undorecord-types)
5. [Stack Data Structure](#stack-data-structure)
6. [The Redo Stack](#the-redo-stack)
7. [Compound Operations](#compound-operations)
8. [Complexity Analysis](#complexity-analysis)
9. [Memory Usage](#memory-usage)
10. [Implementation Preview](#implementation-preview)

---

## Why Undo Is Hard

Undo seems simple on the surface: "reverse what just happened." But consider:

1. **Delete key:** The user pressed Delete. To undo, we must re-insert the
   deleted character. But what character was deleted? We must have saved it.

2. **Backspace:** The user pressed Backspace. It might have:
   - Deleted a character (easy to reverse)
   - Merged two lines (harder — we must re-split them)

3. **Enter:** Pressed Enter, which split a line. Undo must merge the two lines
   back, inserting the cursor at exactly the right position.

4. **Redo:** After undoing 10 operations and then making a new edit, the redo
   history is lost. But the undo history of the new edit must be maintained.

5. **Grouping:** Typing the word "hello" produces 5 separate operations. Should
   Ctrl+Z undo one character at a time, or the entire word? (We will do one
   character at a time in v1; v2 can add word-level grouping.)

---

## Approach Comparison

### Approach A: Full Snapshot

On every keypress, save a complete copy of the buffer.

```
Operations:  [open file] [type 'h'] [type 'e'] [type 'l'] ...
Snapshots:   [ buf_0 ]   [ buf_1 ]   [ buf_2 ]   [ buf_3 ]
```

Undo: restore the previous snapshot.

**Advantages:**
- Trivially simple to implement
- Undo is O(1) — just swap the pointer

**Disadvantages:**
- Memory catastrophic: a 1MB file × 1000 keypresses = 1GB of snapshots
- Copy-on-write can help but adds complexity

**Verdict:** Not suitable. Memory use is prohibitive.

### Approach B: Delta / Command History (tEdit's choice)

Instead of saving the entire buffer, save a *description of the operation*
(the inverse command):

```
User typed 'h' → save record: "delete 'h' at (row=0, col=0)"
User typed 'e' → save record: "delete 'e' at (row=0, col=1)"
```

Undo replays the inverse command against the current buffer.

**Advantages:**
- Tiny memory footprint: each record is ~20 bytes
- 1000 keypresses → 20KB of undo history
- Well-understood design pattern (Command pattern)

**Disadvantages:**
- Each operation type needs a corresponding inverse
- Multi-line operations (Enter, Backspace) are more complex records
- Must record the operation *before* it happens, not after

**Verdict:** The right choice for v1. Clean, memory-efficient, teachable.

### Approach C: Piece Table Revision History

With a piece table buffer (see buffer-design.md), each edit produces a new
piece table state. The old state is preserved at zero cost.

Undo just reverts to the previous piece table state.

**Advantages:**
- Undo is O(1) — just restore the old piece table pointer
- No separate undo data structure needed

**Disadvantages:**
- Requires a piece table buffer (complex data structure)
- The piece table must be the buffer type to get this benefit
- Increases coupling between buffer and undo

**Verdict:** A future consideration. Not for v1 where we use array-of-lines.

---

## tEdit's Approach: Command Stack

Every operation that modifies the buffer produces an `UndoRecord` that
describes how to **reverse** it. Records are pushed onto a stack.

To undo: pop the top record and execute its inverse.
To redo: pop from the redo stack (see below).

```
After typing "hi":

  Undo stack (top is most recent):
  ┌─────────────────────────────┐ ← top
  │ DELETE 'i' at (0, 1)       │
  ├─────────────────────────────┤
  │ DELETE 'h' at (0, 0)       │
  └─────────────────────────────┘

Ctrl+Z (undo once):
  Pop: "DELETE 'i' at (0, 1)"
  Execute: buffer_delete_char(buf, 0, 1)
  Buffer now contains: "h"
  Push this operation onto the REDO stack.

Ctrl+Z again:
  Pop: "DELETE 'h' at (0, 0)"
  Execute: buffer_delete_char(buf, 0, 0)
  Buffer now contains: ""

Ctrl+Y (redo once):
  Pop from redo stack: the record for re-inserting 'h'
  Execute: buffer_insert_char(buf, 0, 0, 'h')
  Buffer: "h"
```

---

## UndoRecord Types

```c
typedef enum UndoType
{
    UNDO_INSERT_CHAR,   /* A single character was inserted.
                           Inverse: delete the character.              */

    UNDO_DELETE_CHAR,   /* A single character was deleted.
                           Inverse: re-insert the saved character.     */

    UNDO_INSERT_LINE,   /* A new line was inserted (Enter was pressed).
                           Inverse: merge the line back (delete the split). */

    UNDO_DELETE_LINE,   /* A line was merged into the previous (Backspace
                           at col 0). Inverse: re-split the line.     */

    UNDO_INSERT_PAIR,   /* Two chars inserted (auto-bracket).
                           Inverse: delete both characters.            */

} UndoType;

typedef struct UndoRecord
{
    UndoType type;      /* What kind of operation this reverses        */
    int      row;       /* Buffer row where the operation occurred     */
    int      col;       /* Buffer column where the operation occurred  */
    char     ch;        /* The character involved (for CHAR operations) */
    char     ch2;       /* The second character (for PAIR operations)  */

    /* For line operations: the content of the deleted/inserted line  */
    char    *line_text; /* NULL for char-level operations              */
    int      line_len;  /* Length of line_text                         */

} UndoRecord;
```

### Record sizes

| Type              | Memory per record                     |
|-------------------|---------------------------------------|
| INSERT_CHAR       | ~28 bytes (struct, line_text = NULL)  |
| DELETE_CHAR       | ~28 bytes                             |
| INSERT_LINE       | 28 bytes + line_text string length    |
| DELETE_LINE       | 28 bytes + line_text string length    |
| INSERT_PAIR       | ~28 bytes                             |

For a 1000-keypress session with typical line lengths of 40 chars:
~28 KB total. Negligible.

---

## Stack Data Structure

The undo stack is implemented as a singly-linked list of `UndoNode`s.

A linked list is chosen over a dynamic array because:
1. We never need random access — undo always operates on the top
2. Each node can hold a variable-size record (line_text length varies)
3. Nodes are added and removed from the top only — perfect for a linked list

```c
typedef struct UndoNode
{
    UndoRecord    record;   /* The undo/redo record                  */
    struct UndoNode *prev;  /* The node below this one on the stack  */
} UndoNode;

typedef struct UndoStack
{
    UndoNode *top;          /* Top of the stack (most recent action) */
    int       depth;        /* Number of records currently on stack  */
    int       max_depth;    /* Maximum depth (0 = unlimited)         */
} UndoStack;
```

### Push

```
Before push:
  top → [ record_N ] → [ record_N-1 ] → ... → NULL

After push(record_X):
  top → [ record_X ] → [ record_N ] → [ record_N-1 ] → ...
```

### Pop

```
Before pop:
  top → [ record_X ] → [ record_N ] → ...

After pop:
  Return record_X.
  top → [ record_N ] → ...
  Free the UndoNode for record_X.
```

---

## The Redo Stack

When the user presses Ctrl+Z (undo), we:
1. Pop the undo record
2. Execute its inverse
3. Push the *re-doable* record onto the **redo stack**

The redo stack is the mirror of the undo stack. It holds what can be
re-applied.

**Critical rule:** Whenever the user makes a new edit (any non-undo/redo
action), the redo stack is cleared. Once you make a new change, you cannot
redo the old undone operations — they are gone.

```
EditorState (conceptually):
  undo_stack: top → [A] → [B] → [C] → NULL
  redo_stack: top → [D] → [E] → NULL

Ctrl+Z:
  pop [A] from undo_stack
  execute inverse of A
  push re-doable version of A onto redo_stack

  undo_stack: top → [B] → [C] → NULL
  redo_stack: top → [A'] → [D] → [E] → NULL

Ctrl+Y:
  pop [A'] from redo_stack
  execute A' (re-apply the operation)
  push A back onto undo_stack

  undo_stack: top → [A] → [B] → [C] → NULL
  redo_stack: top → [D] → [E] → NULL

User types 'x':
  push new record [X] onto undo_stack
  CLEAR the redo stack (free all nodes)

  undo_stack: top → [X] → [A] → [B] → [C] → NULL
  redo_stack: top → NULL
```

In `UndoStack`, we store both stacks:

```c
typedef struct UndoStack
{
    UndoNode *undo_top;   /* Undo stack top  */
    UndoNode *redo_top;   /* Redo stack top  */
    int       undo_depth;
    int       redo_depth;
    int       max_depth;  /* 0 = unlimited   */
} UndoStack;
```

---

## Compound Operations

Some operations produce multiple changes. Enter (newline) does:
1. Split the current line at the cursor
2. Insert a new line in the buffer's line array
3. Add indentation to the new line

Undoing Enter must reverse all three steps atomically.

For v1, we handle this by recording compound operations as a single special
record type (`UNDO_INSERT_LINE`) that carries enough information to reverse
everything:

```c
/* The undo record for pressing Enter at (row=5, col=8) with indent=4: */
UndoRecord {
    type     = UNDO_INSERT_LINE,
    row      = 5,            /* The row that was split */
    col      = 8,            /* Where the split happened */
    line_text = "    code",  /* The text moved to the new line (with indent) */
    line_len  = 8,
}
```

To undo this:
1. Take the text of line 6 (the new line created by Enter): `"    code"`
2. Remove line 6 from the buffer
3. Append the text (after removing leading indentation) back to the end of line 5

This works correctly for the common case. A more general grouping mechanism
(recording arbitrary sequences of operations as a single undo step) is a
v2.0 enhancement.

---

## Complexity Analysis

| Operation          | Time      | Notes                                           |
|--------------------|-----------|-------------------------------------------------|
| Push record        | O(1)      | Allocate node, update top pointer               |
| Pop record (undo)  | O(1)      | Read top node, free it, update top pointer      |
| Clear redo stack   | O(D)      | D = redo stack depth. Free each node.           |
| Execute undo       | O(L)      | L = line length. Buffer mutation is O(L).       |
| Find max depth     | N/A       | Not needed — we walk the stack only on undo     |

---

## Memory Usage

Each `UndoNode` for a char-level operation uses `~40 bytes` on a 64-bit system
(struct + pointer alignment).

For a typing session:
- 5,000 keypresses → 200 KB of undo memory
- 50,000 keypresses → 2 MB of undo memory

This is acceptable without any limit. However, `UndoStack.max_depth` allows
the user to configure a maximum undo history size to bound memory usage:

```c
/* In config: max undo history = 1000 records */
state->undo->max_depth = 1000;
```

When `max_depth` is reached, the oldest record (at the bottom of the stack)
is freed to make room for the new one. This requires walking to the bottom
of the stack, which is O(N) — acceptable since it only happens after 1000
operations and is a one-time cost.

---

## Implementation Preview

The full implementation lives in `src/undo.c` and `src/undo.h`.

Public API:

```c
/* Create and destroy the undo stack */
UndoStack *undo_stack_new(int max_depth);
void       undo_stack_free(UndoStack *stack);

/* Push a record (call BEFORE mutating the buffer) */
void undo_push_insert_char(UndoStack *s, int row, int col, char c);
void undo_push_delete_char(UndoStack *s, int row, int col, char c);
void undo_push_insert_line(UndoStack *s, int row, int col,
                           const char *line_text, int line_len);
void undo_push_delete_line(UndoStack *s, int row, int col,
                           const char *line_text, int line_len);
void undo_push_insert_pair(UndoStack *s, int row, int col, char c1, char c2);

/* Undo / redo — modifies the buffer, returns 0 on success, -1 if empty */
int undo_undo(UndoStack *s, Buffer *buf, int *out_row, int *out_col);
int undo_redo(UndoStack *s, Buffer *buf, int *out_row, int *out_col);

/* Clear the redo stack (call after any new edit) */
void undo_clear_redo(UndoStack *s);

/* Query */
int undo_can_undo(const UndoStack *s);
int undo_can_redo(const UndoStack *s);
```

The `out_row` and `out_col` parameters return the cursor position that
should be restored after the undo/redo operation. This allows `editor.c`
to correctly reposition the cursor without guessing.
