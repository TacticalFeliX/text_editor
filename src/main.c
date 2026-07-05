#define _POSIX_C_SOURCE 200809L

#include "editor.h"

#include <stdio.h>    /* fprintf, printf, stderr */
#include <stdlib.h>   /* atoi, exit, EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h>   /* strcmp */

#define CEDIT_VERSION "1.0.0"

static void print_usage(const char *prog_name);
static void print_version(void);

int main(int argc, char *argv[])
{
    const char *filename = NULL;
    int         tabsize  = 0;

    {
        int i;
        for (i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            {
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            }
            else if (strcmp(argv[i], "--version") == 0 ||
                     strcmp(argv[i], "-v") == 0)
            {
                print_version();
                return EXIT_SUCCESS;
            }
            else if (strcmp(argv[i], "--tabsize") == 0)
            {
                if (i + 1 >= argc)
                {
                    fprintf(stderr, "cedit: --tabsize requires a number\n");
                    return EXIT_FAILURE;
                }
                i++;
                tabsize = atoi(argv[i]);
                if (tabsize < 1 || tabsize > 16)
                {
                    fprintf(stderr,
                            "cedit: --tabsize must be between 1 and 16\n");
                    return EXIT_FAILURE;
                }
            }
            else if (argv[i][0] == '-')
            {
                fprintf(stderr, "cedit: unknown option: %s\n", argv[i]);
                fprintf(stderr, "Run 'cedit --help' for usage.\n");
                return EXIT_FAILURE;
            }
            else
            {
                if (filename != NULL)
                {
                    fprintf(stderr,
                            "cedit: only one file argument is supported\n");
                    return EXIT_FAILURE;
                }
                filename = argv[i];
            }
        }
    }

    EditorState *state = editor_init(filename);
    if (state == NULL)
    {
        return EXIT_FAILURE;
    }

    if (tabsize > 0)
        state->tab_size = tabsize;

    editor_run(state);

    editor_free(state);

    return EXIT_SUCCESS;
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options] [file]\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  --tabsize N   Set tab width to N spaces (1-16, default: 4)\n");
    printf("  --help        Show this help message and exit\n");
    printf("  --version     Show version and exit\n");
    printf("\n");
    printf("Key bindings:\n");
    printf("  Ctrl+S        Save file\n");
    printf("  Ctrl+O        Open file\n");
    printf("  Ctrl+Q        Quit (press twice if unsaved changes)\n");
    printf("  Ctrl+Z        Undo\n");
    printf("  Ctrl+Y        Redo\n");
    printf("  Ctrl+F        Find\n");
    printf("  Ctrl+N        Find next\n");
    printf("  Ctrl+P        Find previous\n");
    printf("  Ctrl+R        Replace\n");
    printf("  Ctrl+G        Go to line\n");
    printf("  Home/End      Start/end of line\n");
    printf("  PgUp/PgDn     Scroll by page\n");
}

static void print_version(void)
{
    printf("cedit %s\n", CEDIT_VERSION);
    printf("A lightweight terminal text editor written in C.\n");
}
