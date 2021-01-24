#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/** Struct created to store the original state of termios to set it back on exit .
 * **/
struct termios orig_termios;

void die(const char *s) {
    perror(s);
    exit(1);
}

/** Method to disable Raw mode - Reset terminal flags to original state. 
 * **/
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) 
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");

    atexit(disableRawMode);

    struct termios raw = orig_termios;

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

int main() {
    enableRawMode();

    while (1) { 
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");

        if (iscntrl(c)) {
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }
    return 0;
}
