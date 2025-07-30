/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_BUFFER_LINE 2048

extern void shell_tty_raw_mode(void);

// Buffer where line is stored
static int line_length;
static int cursor_pos = 0;
static char line_buffer[MAX_BUFFER_LINE];

static const char PROMPT[] = "myshell>";
static const int PROMPT_LEN = sizeof(PROMPT) - 1;

// Simple history array
// This history does not change. 
// Yours have to be updated.
static int history_write = 0;
static int history_view = 0;
char * history [] = {
  "ls -al | grep x", 
  "ps -e",
  "cat read-line-example.c",
  "vi hello.c",
  "make",
  "ls -al | grep xxx | grep yyy"
};
static const int history_capacity = sizeof(history)/sizeof(char *);

static int prev_print_len = 0;

static void move_cursor_left(int n) {
  for (int i = 0; i < n; i++) {
    write(1, "\b", 1);
  }
}
static void move_cursor_right(int n) {
  for (int i = 0; i < n; i++) {
    write(1, &line_buffer[cursor_pos + i], 1);
  }
}

static void clear_line_echo() {

  write(1, "\r", 1);
  for (int i = 0; i < PROMPT_LEN + prev_print_len; ++i) {
    write(1, " ", 1);
  }
  write(1, "\r", 1);
  write(1, PROMPT, PROMPT_LEN);
  prev_print_len = 0;
}

static void refresh_echo(void) {

  write(1, "\r", 1);
  write(1, PROMPT, PROMPT_LEN);
  write(1, line_buffer, line_length);

  int diff = prev_print_len - line_length;
  for (int i = 0; i < diff; ++i) {
    write(1, " ", 1);
  }

  int total_len_now = PROMPT_LEN + line_length;
  int target_pos = PROMPT_LEN + cursor_pos;
  for (int i = 0; i < total_len_now - target_pos; ++i) {
    write(1, "\b", 1);
  }

  prev_print_len = line_length;
}

static void redraw_from_cursor(void) {
  write(1, line_buffer + cursor_pos,
           line_length - cursor_pos);
  write(1, " ", 1);
  for (int i = 0; i < line_length - cursor_pos + 1; ++i) {
    write(1, "\b", 1);
  }
  prev_print_len = line_length;
}

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " Ctrl-D       Delete char at cursor\n"
    " <- / ->      Move cursor left/right\n"
    " Ctrl-A       Move to start of line\n"
    " Ctrl-E       Move to end of line\n"
    " up / down    Browse the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  shell_tty_raw_mode();

  line_length = cursor_pos = 0;
  line_buffer[0] = '\0';
  prev_print_len = 0;
  history_view = history_write;

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    if (read(0, &ch, 1) != 1) break;

    if (ch >=32 && ch != '\t') {
      if (line_length < MAX_BUFFER_LINE - 2) {
        memmove(line_buffer + cursor_pos + 1,
                line_buffer + cursor_pos,
                line_length - cursor_pos + 1);
        line_buffer[cursor_pos] = ch;
        line_length++;
        cursor_pos++;
        refresh_echo();
      }
    }
    else if (ch==10) {
      // <Enter> was typed. Return line

      // Print newline
      write(1,"\n",1);
      if (history_write >= 6 && history[history_write]) {
        free(history[history_write]);
      }
      history[history_write] = strdup(line_buffer);
      history_write = (history_write + 1) % history_capacity;
      break;
    }
    else if (ch == 31) {
      read_line_print_usage();
      refresh_echo();
    }
    else if (ch == 1) {
      move_cursor_left(cursor_pos);
      cursor_pos = 0;
    }
    else if (ch == 5) {
      move_cursor_right(line_length - cursor_pos);
      cursor_pos = line_length;
    }
    else if (ch == 8 || ch == 127) {
      if (cursor_pos > 0) {
        move_cursor_left(1);
        memmove(line_buffer + cursor_pos - 1,
                line_buffer + cursor_pos,
                line_length - cursor_pos + 1);
        cursor_pos--;
        line_length--;
        line_buffer[line_length] = '\0';

        redraw_from_cursor();
      }
    }
    else if (ch == 4) {
      if (cursor_pos < line_length) {
        memmove(line_buffer + cursor_pos,
                line_buffer + cursor_pos + 1,
                line_length - cursor_pos);
        line_length--;
        line_buffer[line_length] = '\0';
        redraw_from_cursor();
      }
    }
    else if (ch==27) {
      char seq[3];
      if (read(0, seq, 2) != 2) continue;
      if (seq[0] == '[' && seq[1] == '3') {
        char tilde;
        if (read(0, &tilde, 1) != 1 || tilde != '~') continue;
        if (cursor_pos < line_length) {
          memmove(line_buffer + cursor_pos,
                  line_buffer + cursor_pos + 1,
                  line_length - cursor_pos);
          line_length--;
          line_buffer[line_length] = '\0';
          redraw_from_cursor();
        }
        continue;
      }
      if (seq[0] == '[' && seq[1] == 'D') {
        if (cursor_pos > 0) {
          move_cursor_left(1);
          cursor_pos--;
        }
      }
      else if (seq[0] == '[' && seq[1] == 'C') {
        if (cursor_pos < line_length) {
          write(1, &line_buffer[cursor_pos], 1);
          cursor_pos++;
        }
      }
      else if (seq[0] == '[' && (seq[1] == 'A' || seq[1] == 'B')) {
        clear_line_echo();
        if (seq[1] == 'A') {
          history_view = (history_view - 1 + history_capacity) % history_capacity;
        }
        else {
          history_view = (history_view + 1) % history_capacity;
        }
        strcpy(line_buffer, history[history_view]);
        line_length = strlen(line_buffer);
        cursor_pos = line_length;
        line_buffer[line_length] = '\0';
        refresh_echo();
      }
    }
  }

  line_buffer[line_length] = '\n';
  line_buffer[line_length + 1] = '\0';
  return line_buffer;
}


