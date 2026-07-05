#ifndef CEDIT_TERMINAL_H
#define CEDIT_TERMINAL_H

struct EditorState;

typedef enum KeyType
{
    KEY_CHAR,           /* A printable character (space through ~)      */
    KEY_CTRL,           /* A control character (Ctrl+A through Ctrl+Z)  */
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_ESCAPE,
    KEY_UNKNOWN,
} KeyType;

typedef struct KeyEvent
{
    KeyType type;
    int     value;  
} KeyEvent;

#define CTRL(x) ((x) & 0x1f)

void terminal_init(int *rows, int *cols);

void terminal_cleanup(void);

KeyEvent terminal_read_key(void);

int terminal_was_resized(void);

void terminal_render(struct EditorState *state);

void terminal_get_size(int *rows, int *cols);

int terminal_prompt(struct EditorState *state, const char *prompt, char *buf, int   buflen);

#endif 
