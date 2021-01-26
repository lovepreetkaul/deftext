/*** includes ***/

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
    int screenrows;
    int screencols;
    /** Struct created to store the original state of termios to set it back on exit .
    * **/
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) 
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    /** Sets the Input Flags for the termios.
     *
     *  BRKINT - (Legacy) A break condition causes a SIGINT similar to CTRL-C.
     *  INPCK  - (Legacy) Enables parity checking. Not Applicable to modern
     *           terminals.
     *  ISTRIP - (Legacy) Causes 8th bit of each input byte to be stripped,
     *           set to 0. Already turned off. 
     *  ICRNL  - Causes carriage returns to be translated as newlines.
     *           Disabled to fix the CTRL-M showing 10 instead of 13.
     *  IXON   - Toggles software flow control characters CTRL-S and CTRL-Q
     *  **/
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
    
    /** Sets the Output flags for the termios. 
     *  OPOST - Causes output processing of \n as \r\n. **/
    raw.c_oflag &= ~(OPOST);

    /** Sets Control flags for the terminal. 
     *  CS8    - (Legacy) Sets the character size to 8 bits per byte. Already 
     *           set this way.
     * **/
    raw.c_cflag |= (CS8);

    /** Sets the Local flags for termios. Also called 'dumping ground'!
     *
     * ECHO    - Causes the input characters to be echoed back.
     * ICANON  - Toggles canonical mode for the terminal. Input is now read 
     *           byte-by-byte instead of line-by-line.
     * IEXTEN  - CTRL-V causes the terminal to wait for a followup character to 
     *           be interpreted literally.
     * ISIG    - Disables the CTRL-C and CTRL-Z to terminate the program.
     * **/
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /** Minimum number for bytes required for read() to return.**/
    raw.c_cc[VMIN] = 0;

    /** Time for read to return. Measured in multiples of 100 milliseconds. **/
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 ) { 
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};


#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}


/*** output ***/


/** Draw the tildes just like vim.**/
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        abAppend(ab, "~", 1);

        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    /** Clears the screen. Refer to vt100 terminal sequences.
     * \x1b - 27. Control character. Along with [ creates an escape sequence.
     * J    - Command to erase screen. 2 being a parameter to clean the entire
     *        screen.
     * **/
    abAppend(&ab, "\x1b[2J", 4);

    /** H - Reposition cursor to the defined width; height. defaults to 1;1 so
     *      not specified.**/
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


void editorCleanScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
            editorCleanScreen();
            exit(0);
            break;
    }
}
 
/*** init ***/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) 
        die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) { 
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
