#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define READ_BLOCKSIZE 10
#define ctrl(x) ((x) & 0x1f)
#define cx cursor.x
#define cy cursor.y

struct line
{
    unsigned int len;
    unsigned char *data;
    unsigned int length;
    unsigned int real_length;
};

struct line *lines = NULL;
unsigned int num_lines;
unsigned int len_line_number; // The length of the number of the last line

FILE *fp = NULL;

struct
{
    unsigned int x;
    unsigned int y;
}
cursor = {0, 0};

struct
{
    unsigned int x;
    unsigned int y;
}
text_scroll = {0, 0};

char *filename;

char colors_on;
char needs_to_free_filename;

void setcolor(int c)
{
    if (colors_on)
    {
        attrset(c);
    }
}

unsigned int last_cursor_x = 0;

void message(char *msg)
{
    unsigned int len = strlen(msg);

    unsigned int lines = 0;
    for (unsigned int i = 0; i < len; i++)
    {
        if (msg[i] == '\n')
        {
            lines++;
        }
    }

    char *s = strtok(msg, "\n");

    clear();
    setcolor(COLOR_PAIR(1));
    attron(A_BOLD);
    for (unsigned int i = (LINES - lines) / 2; s != NULL; i++)
    {
        mvprintw(i, (COLS - strlen(s)) / 2, "%s", s);

        s = strtok(NULL, "\n");
    }
    attroff(A_BOLD);
    setcolor(COLOR_PAIR(2));
    refresh();
    
    getch();
}

void savefile()
{
    FILE *fpw = fopen(filename, "w");

    if (fpw == NULL)
    {
        char buf[1000];
        snprintf(buf, 1000, "Could not open the file\nErrno: %d\nPress any key", errno);

        message(buf);

        return;
    }

    for (unsigned int i = 0; i < num_lines; i++)
    {
        fputs((const char *)lines[i].data, fpw);
        if (i < num_lines - 1)
        {
            fputc('\n', fpw);
        }
    }

    fclose(fpw);
}

void read_lines()
{
    if (fp == NULL)
    {
        num_lines = 1;
        lines = malloc(sizeof(struct line));

        lines[0].len = READ_BLOCKSIZE;
        lines[0].data = malloc(lines[0].len);
        lines[0].length = 0;
        lines[0].real_length = 0;
        lines[0].data[0] = '\0';

        return;
    }

    num_lines = 0;
    for (unsigned int i = 0; !feof(fp); i++)
    {
        lines = realloc(lines, ++num_lines * sizeof(struct line));     

        lines[i].len = READ_BLOCKSIZE;
        lines[i].data = malloc(lines[i].len);
        lines[i].length = 0;

        char c;
        unsigned int j;
        for (j = 0; (c = fgetc(fp)) != '\n' && c != EOF; j++)
        {
            if (j >= READ_BLOCKSIZE)
            {
                lines[i].len += READ_BLOCKSIZE;
                lines[i].data = realloc(lines[i].data, lines[i].len);
            }

            unsigned char uc = *(unsigned char *)&c;
            
            if (uc >= 0xC0 && uc <= 0xDF)
            {
                if (uc == '\0')
                {
                    break;
                }
                if (j + 1 >= READ_BLOCKSIZE)
                {
                    lines[i].len += READ_BLOCKSIZE;
                    lines[i].data = realloc(lines[i].data, lines[i].len);
                }
                for (unsigned int k = 0; k < 2; k++)
                {
                    lines[i].data[j + k] = c;
                    if (k < 1)
                    {
                        c = fgetc(fp);
                    }
                }
                j++;
            }
            else if (uc >= 0xE0 && uc <= 0xEF)
            {
                if (uc == '\0')
                {
                    break;
                }
                if (j + 2 >= READ_BLOCKSIZE)
                {
                    lines[i].len += READ_BLOCKSIZE;
                    lines[i].data = realloc(lines[i].data, lines[i].len);
                }
                for (unsigned int k = 0; k < 3; k++)
                {
                    lines[i].data[j + k] = c;
                    if (k < 2)
                    {
                        c = fgetc(fp);
                    }
                }
                j += 2;
            }
            else if (uc >= 0xF0 && uc <= 0xF7)
            {
                if (uc == '\0')
                {
                    break;
                }
                if (j + 3 >= READ_BLOCKSIZE)
                {
                    lines[i].len += READ_BLOCKSIZE;
                    lines[i].data = realloc(lines[i].data, lines[i].len);
                }
                for (unsigned int k = 0; k < 4; k++)
                {
                    lines[i].data[j + k] = c;
                    if (k < 3)
                    {
                        c = fgetc(fp);
                    }
                }
                j += 3;
            }
            else
            {
                lines[i].data[j] = uc;
            }

            lines[i].length++;
        }
        lines[i].real_length = j;
        lines[i].data[j] = '\0';
    }
}

void show_lines()
{
    for (unsigned int i = text_scroll.y; i < text_scroll.y + LINES; i++)
    {
        move(i - text_scroll.y, 0);
        if (i >= num_lines)
        {
            setcolor(COLOR_PAIR(1));
            for (unsigned int j = 0; j < len_line_number - 1; j++)
            {
                addch(' ');
            }
            addch('~');
            setcolor(COLOR_PAIR(2));
            continue;
        }
        
        setcolor(COLOR_PAIR(1));

        printw("%*d ", len_line_number, i + 1);

        setcolor(COLOR_PAIR(2));


        unsigned int size = 0;
        for (unsigned int j = 0; size < (unsigned int)COLS - len_line_number - 1 + text_scroll.x; j++)
        {
            if (lines[i].data[j] == '\0')
            {
                break;
            }

            if (lines[i].data[j] >= 0xC0 && lines[i].data[j] <= 0xDF)
            {
                if (lines[i].data[j] == '\0')
                {
                    break;
                }
                if (size >= text_scroll.x)
                {
                    printw("%.2s", &lines[i].data[j]);
                }
                j++;
            }
            else if (lines[i].data[j] >= 0xE0 && lines[i].data[j] <= 0xEF)
            {
                if (lines[i].data[j] == '\0')
                {
                    break;
                }
                if (size >= text_scroll.x)
                {
                    printw("%.3s", &lines[i].data[j]);
                }
                j += 2;setcolor(1);
            }
            else if (lines[i].data[j] >= 0xF0 && lines[i].data[j] <= 0xF7)
            {
                if (lines[i].data[j] == '\0')
                {
                    break;
                }
                if (size >= text_scroll.x)
                {
                    printw("%.4s", &lines[i].data[j]);
                }
                j += 3;
            }
            else
            {
                if (size >= text_scroll.x)
                {
                    printw("%c", lines[i].data[j]);
                }
            }

            if (lines[i].data[j] == '\0')
            {
                break;
            }
            size++;
        }

    }
}

void free_lines()
{
    for (unsigned int i = 0; i < num_lines; i++)
    {
        free(lines[i].data);
        lines[i].len = 0;
    }
}

void process_keypress(int c)
{
    switch (c)
    {
        case KEY_UP:
            cursor.y -= (cursor.y > 0);

            if (cursor.x < last_cursor_x)
            {
                cursor.x = last_cursor_x;
                last_cursor_x = 0;
            }

            if (cursor.x > lines[cursor.y].length)
            {
                last_cursor_x = cursor.x;
                cursor.x = lines[cursor.y].length;
            }
            if (cursor.y < text_scroll.y)
            {
                text_scroll.y = cursor.y;
            }

                

            if (cursor.x < text_scroll.x)
            {
                text_scroll.x = cursor.x;
            }
            else if (cursor.x - text_scroll.x >= (unsigned int)COLS - len_line_number - 1)
            {
                text_scroll.x += (cursor.x - text_scroll.x) - ((unsigned int)COLS - len_line_number - 2);
            }
            break;
        case KEY_DOWN:
            cursor.y += (cursor.y < num_lines - 1);

            if (cursor.x < last_cursor_x)
            {
                cursor.x = last_cursor_x;
                last_cursor_x = 0;
            }

            if (cursor.x > lines[cursor.y].length)
            {
                last_cursor_x = cursor.x;
                cursor.x = lines[cursor.y].length;
            }
            if (cursor.y > text_scroll.y + LINES - 1)
            {
                text_scroll.y = cursor.y + 1 - LINES;
            }

            if (cursor.x < text_scroll.x)
            {
                text_scroll.x = cursor.x;
            }
            else if (cursor.x - text_scroll.x >= (unsigned int)COLS - len_line_number - 1)
            {
                text_scroll.x += (cursor.x - text_scroll.x) - ((unsigned int)COLS - len_line_number - 2);
            }
            break;
        case KEY_LEFT:
            cursor.x -= (cursor.x > 0);
            if (cursor.x < text_scroll.x)
            {
                text_scroll.x = cursor.x;
            }
            break;
        case KEY_RIGHT:
            cursor.x += (cursor.x < lines[cursor.y].length);
            if (cursor.x - text_scroll.x >= (unsigned int)COLS - len_line_number - 1)
            {
                text_scroll.x += (cursor.x - text_scroll.x) - ((unsigned int)COLS - len_line_number - 2);
            }
            break;
        case KEY_HOME:
            cursor.x = 0;
            if (cursor.x < text_scroll.x)
            {
                text_scroll.x = cursor.x;
            }
            break;
        case KEY_END:
            cursor.x = lines[cursor.y].length;
            if (cursor.x - text_scroll.x >= (unsigned int)COLS - len_line_number - 1)
            {
                text_scroll.x += (cursor.x - text_scroll.x) - ((unsigned int)COLS - len_line_number - 2);
            }
            break;
        case ctrl('s'):
            savefile();
            break;
    }

    unsigned int real_cx = 0;
    unsigned int offset = 0;
    unsigned int last_one_size = 1;
    for (unsigned int i = 0; i < cursor.x; i++)
    {
        if (lines[cy].data[i + offset] >= 0xC0 && lines[cy].data[i + offset] <= 0xDF)
        {
            offset++;
            real_cx++;
            last_one_size = 2;
        }
        else if (lines[cy].data[i + offset] >= 0xE0 && lines[cy].data[i + offset] <= 0xEF)
        {
            offset += 2;
            real_cx += 2;
            last_one_size = 3;
        }
        else if (lines[cy].data[i + offset] >= 0xF0 && lines[cy].data[i + offset] <= 0xF7)
        {
            offset += 3;
            real_cx += 3;
            last_one_size = 4;
        }
        else {
            last_one_size = 1;
        }
        real_cx++;
    }

    if (isprint(c))
    {
        if (lines[cy].len <= lines[cy].real_length + 1)
        {
            lines[cy].len += READ_BLOCKSIZE;
            lines[cy].data = realloc(lines[cy].data, lines[cy].len);
        }

        memmove(&lines[cy].data[real_cx + 1], &lines[cy].data[real_cx], lines[cy].real_length - real_cx);

        lines[cy].data[real_cx] = c;
        lines[cy].data[lines[cy].real_length + 1] = '\0';

        lines[cy].real_length++;
        lines[cy].length++;

        process_keypress(KEY_RIGHT);
    }
    else if (c >= 0xC0 && c <= 0xDF)
    {
        while (lines[cy].len <= lines[cy].real_length + 2)
        {
            lines[cy].len += READ_BLOCKSIZE;
            lines[cy].data = realloc(lines[cy].data, lines[cy].len);
        }

        memmove(&lines[cy].data[real_cx + 2], &lines[cy].data[real_cx], lines[cy].real_length - real_cx);

        lines[cy].data[real_cx] = c;
        lines[cy].data[real_cx + 1] = getch();
        lines[cy].data[lines[cy].real_length + 2] = '\0';

        lines[cy].real_length += 2;

        lines[cy].length++;

        process_keypress(KEY_RIGHT);
    }
    else if (c >= 0xE0 && c <= 0xEF)
    {
        while (lines[cy].len <= lines[cy].real_length + 3)
        {
            lines[cy].len += READ_BLOCKSIZE;
            lines[cy].data = realloc(lines[cy].data, lines[cy].len);
        }

        memmove(&lines[cy].data[real_cx + 3], &lines[cy].data[real_cx], lines[cy].real_length - real_cx);

        lines[cy].data[real_cx] = c;
        lines[cy].data[real_cx + 1] = getch();
        lines[cy].data[real_cx + 2] = getch();
        lines[cy].data[lines[cy].real_length + 3] = '\0';

        lines[cy].real_length += 3;

        lines[cy].length++;

        process_keypress(KEY_RIGHT);
    }
    else if (c >= 0xF0 && c <= 0xF7)
    {
        while (lines[cy].len <= lines[cy].real_length + 4)
        {
            lines[cy].len += READ_BLOCKSIZE;
            lines[cy].data = realloc(lines[cy].data, lines[cy].len);
        }

        memmove(&lines[cy].data[real_cx + 4], &lines[cy].data[real_cx], lines[cy].real_length - real_cx);

        lines[cy].data[real_cx] = c;
        lines[cy].data[real_cx + 1] = getch();
        lines[cy].data[real_cx + 2] = getch();
        lines[cy].data[real_cx + 3] = getch();
        lines[cy].data[lines[cy].real_length + 4] = '\0';

        lines[cy].real_length += 4;

        lines[cy].length++;

        process_keypress(KEY_RIGHT);
    }
    else if (c == KEY_BACKSPACE)
    {
        if (real_cx >= last_one_size)
        {
            memmove(
                &lines[cy].data[real_cx - last_one_size],
                &lines[cy].data[real_cx],
                lines[cy].real_length - real_cx
            );
            lines[cy].real_length -= last_one_size;
            lines[cy].length--;
            lines[cy].data[lines[cy].real_length] = '\0';

            process_keypress(KEY_LEFT);
        }
        else if (cy > 0) {
            unsigned char *del_line = lines[cy].data;
            unsigned int del_line_len = lines[cy].real_length;

            memmove(
                &lines[cy],
                &lines[cy + 1],
                (num_lines - cy - 1) * sizeof(struct line)
            );

            num_lines--;
            lines = realloc(lines, num_lines * sizeof(struct line));

    
            process_keypress(KEY_UP);

            cursor.x = lines[cy].length;
            
            process_keypress(KEY_RIGHT);

            while (lines[cy].len <= lines[cy].real_length + del_line_len)
            {
                lines[cy].len += READ_BLOCKSIZE;
                lines[cy].data = realloc(lines[cy].data, lines[cy].len);
            }

            
            for (unsigned int i = 0; i < del_line_len; i++)
            {
                lines[cy].data[lines[cy].real_length + i] = del_line[i];

                lines[cy].length++;

                unsigned char cc = del_line[i];
                if (cc >= 0xC0 && cc <= 0xDF)
                {
                    lines[cy].length--;
                }
                else if (cc >= 0xE0 && cc <= 0xEF)
                {
                    lines[cy].length -= 2;
                }
                else if (cc >= 0xF0 && cc <= 0xF7)
                {
                    lines[cy].length -= 3;
                }

            }

            lines[cy].real_length += del_line_len;
            lines[cy].data[lines[cy].real_length] = '\0';

            free(del_line);
        }
    }
    else if (c == '\n')
    {
        lines = realloc(lines, (num_lines + 1) * sizeof(struct line));
    
        memmove(&lines[cy + 2], &lines[cy + 1], (num_lines - cy - 1) * sizeof(struct line));

        num_lines++;

        cursor.x = 0;
        last_cursor_x = 0;
        process_keypress(KEY_DOWN);

        lines[cy].len = READ_BLOCKSIZE;
        lines[cy].data = malloc(lines[cy].len * sizeof(struct line));

        lines[cy].length = 0;
        lines[cy].real_length = 0;

        for (unsigned int i = 0; i < lines[cy - 1].real_length - real_cx; i++)
        {
            int cc = lines[cy - 1].data[i + real_cx];

            lines[cy].real_length++;
            lines[cy].length++;
            while (lines[cy].real_length >= lines[cy].len)
            {
                lines[cy].len += READ_BLOCKSIZE;
                lines[cy].data = realloc(lines[cy].data, lines[cy].len * sizeof(struct line));
            }
            lines[cy].data[i] = lines[cy - 1].data[i + real_cx];
            
            if (cc >= 0xC0 && cc <= 0xDF)
            {
                lines[cy].length--;
            }
            else if (cc >= 0xE0 && cc <= 0xEF)
            {
                lines[cy].length -= 2;
            }
            else if (cc >= 0xF0 && cc <= 0xF7)
            {
                lines[cy].length -= 3;
            }
        }
        lines[cy].data[lines[cy].real_length] = '\0';

        lines[cy - 1].length = real_cx;
        lines[cy - 1].real_length = real_cx;

        for (unsigned int i = 0; i < lines[cy - 1].real_length; i++)
        {
            int cc = lines[cy - 1].data[i];

            if (cc >= 0xC0 && cc <= 0xDF)
            {
                lines[cy - 1].length--;
            }
            else if (cc >= 0xE0 && cc <= 0xEF)
            {
                lines[cy - 1].length -= 2;
            }
            else if (cc >= 0xF0 && cc <= 0xF7)
            {
                lines[cy - 1].length -= 3;
            }
        }

        lines[cy - 1].data[lines[cy - 1].real_length] = '\0';

        char tmp[50];
        len_line_number = snprintf(tmp, 50, "%u", num_lines + 1);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        struct stat st = {0};

        char *home = getenv("HOME");

        char config[strlen(home) + strlen("/.config/") + 1];

        strcpy(config, home);
        strcat(config, "/.config/");


        if (stat(config, &st) == -1)
        {
            mkdir(config, 0777);
        }

        char config_ted[strlen(config) + strlen("ted/") + 1];

        strcpy(config_ted, config);
        strcat(config_ted, "ted/");

        if (stat(config_ted, &st) == -1)
        {
            mkdir(config_ted, 0777);
        }

        filename = malloc(strlen(config_ted) + strlen("buffer") + 1);
        
        strcpy(filename, config_ted);
        strcat(filename, "buffer");

        needs_to_free_filename = 1;
    }
    else {
        filename = argv[1];
        needs_to_free_filename = 0;
    }

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    
    fp = fopen(filename, "r");

    read_lines();

    if (fp != NULL)
    {
        fclose(fp);
    }

    {
        char tmp[50];
        len_line_number = snprintf(tmp, 50, "%u", num_lines + 1);
    }


    if (!has_colors())
    {
        colors_on = 0;
    }
    else if (start_color() != OK)
    {
        colors_on = 0;
    }
    else {
        colors_on = 1;
    }


    if (colors_on)
    {
        use_default_colors();
        init_pair(1, COLOR_RED, -1);
        init_pair(2, -1, -1);
    }


    int c;
    while (1)
    {
        clear();
        show_lines();
        move(cursor.y - text_scroll.y, cursor.x - text_scroll.x + len_line_number + 1);
        refresh();

        c = getch();

        if (c == ctrl('c'))
        {
            break;
        }

        process_keypress(c);
    }


    free_lines();

    if (needs_to_free_filename == 1)
    {
        free(filename);
    }

    endwin();
    return 0;
}
