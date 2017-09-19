/*
 * =====================================================================================
 *
 *       Filename:  ghi.c
 *
 *    Description:  Build your own editor - Tutorial
 *                  http://viewsourcecode.org/snaptoken/kilo
 *
 *        Version:  1.0
 *        Created:  30/08/2017 16:24:38
 *       Compiler:  gcc
 *
 *         Author:  congdv (), congdaovan94@gmail.com
 *
 * =====================================================================================
 */

/* includes */

/* For warning when at getline() function */
#define _DEFUALT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <ctype.h>
#include <errno.h>
#include <fcntl.h> // maniplate file descriptor
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> // Winsize
#include <sys/types.h>
#include <termios.h> //Enable rawmode
#include <time.h>
#include <unistd.h>

#include "unicode.h"

/*** defines ***/
#define GHI_VERSION "0.0.1"
#define GHI_TAB_STOP 8
#define GHI_QUIT_TIMES 3 // Warn user to force quit
#define CTRL_KEY(k) ((k) & 0x1f) //00011111 , 3 bit is ctrl and 5 bit is character ascii

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT  = 1000,
    ARROW_RIGHT,
    ARROW_UP   ,
    ARROW_DOWN ,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** data ***/
// Store a row of text in editor
// Editor row
typedef struct erow {
    int size;
    int rsize; // render size
    char *chars;
    char *render;
    alchars alc; // Chars for store unicode character
    alchars renderAlc;
} erow;

struct editorConfig {
    int cx,cy; // x,y
    int rx; // Fix move over tabs when tab is spaces
    int rowoff; // Row offset
    int coloff; // Col offset
    int screenrows;
    int screencols;
    int numrows; // number of rows display
    erow *row; // Support multiple line
    int dirty; // State modified a file
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_terminos;    // Terminal attribute
};

struct editorConfig E; // Make global variable for config

/*** Append buffer ***/
/* Replace write out byte by append buffer */
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0} // Represent constructor for append bufffer

/* Append string s into struct abuf with len */
void abAppend(struct abuf *ab, const char *s, int len) {
    // extend location for store string append
    // When relloc address memory in ab will destroy
    // and create new address
    char *new = realloc(ab->b, ab->len + len);
    if(new == NULL) return;

    memcpy(&new[ab->len],s,len);// Put string into loction mem
    // Referent new point append
    ab->b = new; // Add new memory
    ab->len += len;
}

/* Free string */
void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void convertToUnicode(struct abuf *ab, unsigned codePoint);

/*** terminal ***/
/* Error handling */
void die(const char *s) {
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);

    perror(s); // From stdio.h
    exit(1);// stdlib.h
}

/* Restart configuration terminal */
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_terminos) == 1) {
        die("tcsetattr");
    } // This function will restore
}

/* Turn off mode on terminal
 * - Echo mode
 * - canical mode
 * - Ctrl-c and Ctrl-z signals
 * - Ctrl-s and Ctrl-q those are freezing and resume ouput
 * - Ctrl-v some OS
 * - all outpt processing like "\n" and "\r\n"
 */
void enableRawMode() {

    if(tcgetattr(STDIN_FILENO, &E.orig_terminos) == -1) {
        die("tcgetattr");
    }// Get attribute of terminal
    atexit(disableRawMode); // This function will be call when program finishing
                            // So we will register function disable to here
                            // This function from stdlib.h
    struct termios raw = E.orig_terminos;  // Make global variable not effect

    // Turn off canonical mode
    // Canonical mode like when you type a line then when enter you will commit out to system
    // Non-Canical when any type character it will commit out to system
    // Like command line in terminal(canical) and Vim(non-canical)
    // See more: https://stackoverflow.com/questions/358342/canonical-vs-non-canonical-terminal-input
    raw.c_iflag &= ~(ICRNL | IXON); // Turn off Ctrl-s and Ctrl-q
    raw.c_oflag &= ~(OPOST); // Turn off output processing
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // Turn off ICANON mode, so whenever you press q then the program
                                     // will quit
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
        die("tcsetattr");
    }// Set attribute of terminal

}

int editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    // Arrow key like \x1bA ,\x1bB,\x1bC,\x1bD
    if(c =='\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0],1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1],1) != 1) return '\x1b';
        if(seq[0] == '[') {
            // Configuration for page up and page down key
            // page up key has value <esc>[5~
            // page down key has value <esc>[6~
            if(seq[1] >= '0' && seq[1] <= '9') {
                if(read(STDIN_FILENO,&seq[2],1) != 1) return '\x1b';
                if(seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;

                }
            }
        } else if (seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO,"\x1b[6n",4) != 4) return -1;

    while( i < sizeof(buf) -1 ) {
        if(read(STDIN_FILENO, &buf[i],1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2],"%d;%d",rows,cols) != 2) return -1;
    //printf("\r\n&buf[1]: '%s'\r\n",&buf[1]); // This is cols and rows
    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12) return -1;
        // Some time ioctl is not working on some OS
        // We use read buffer when open buffer to calculate
        // cols and rows
        return getCursorPosition(rows,cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations  ***/

int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for(j = 0; j < cx; j++) {
        if(row->chars[j] == '\t')
            rx += (GHI_TAB_STOP - 1) - (rx % GHI_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for(cx = 0; cx < row->size; cx++) {
        if(row->chars[cx] == '\t')
            cur_rx += (GHI_TAB_STOP - 1) - (cur_rx % GHI_TAB_STOP);
        cur_rx++;

        if(cur_rx > rx) return cx;
    }

    return cx;
}

void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;

    // Count tabs character
    for(j = 0; j < row->size;j++) {
        if(row->chars[j] == '\t') tabs++;
    }

    free(row->render);
    // 7 tabs because we have one tabs from default
    row->render = malloc(row->size +tabs*(GHI_TAB_STOP - 1) + 1);

    int idx = 0;
    for(j = 0; j < row->size;j++) {
        // Replace tab character with spaces
        if(row->chars[j] == '\t') {
            row->render[idx++]=' ';
            while(idx % GHI_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            // Check printable character
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorUpdateUnicodeRow(erow *row) {
    int tabs = 0;
    int j;
    for(j = 0; j < row->size;j++) {
        achar *ac = getBucketAt(row->alc,j);
        if(ac->length == 1 && ac->bytes[0] == '\t') {
            tabs++;
        }
    }
    // Free old render to render new string
    
    freeChars(row->renderAlc);
    row->renderAlc = newChar();

    int idx = 0;
    for(j = 0; j < row->size; j++) {
        achar *ac = getBucketAt(row->alc,j);
        if(ac == NULL) {
            fprintf(stderr,"Error at %d",j);
        }
        // Replace tab character with spaces
        if(ac->length == 1 && ac->bytes[0] == '\t') {
            appendNewChar(row->renderAlc,' ');
            idx++;
            while(idx % GHI_TAB_STOP != 0) {
                appendNewChar(row->renderAlc,' ');
                idx++;
            }
        } else {
            appendNewStringWithLen(row->renderAlc,ac->bytes,ac->length);
            idx++;
        }
    }
    row->rsize = idx;
}
/* Insert row at with s and len of s*/
void editorInsertRow(int at, char *s, size_t len) {
    if(at < 0 || at > E.numrows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    // Move row contains chars from cursor to end currently into next row
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    // row start at
    E.row[at].size = len;
    E.row[at].chars = malloc(len+1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render=NULL;

    editorUpdateRow(&E.row[at]);

    // Initalize row append unicode character
    E.row[at].alc = newChar();
    appendNewStringWithLen(E.row[at].alc,s,len);

    E.row[at].size = getLen(E.row[at].alc);

    E.row[at].rsize = 0;

    E.row[at].renderAlc=newChar();

    editorUpdateUnicodeRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if(at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

/* Insert character to a row */
void editorRowInsertChar(erow *row, int at, int c) {
    if(at < 0 || at > row->size) at = row->size;

    // Shift left from this cursor characte with the rest line characters
    // to next character index position
    // row->size - at + 1, the rest line with current character
    // When append chars
    
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at+1], &row->chars[at], row->size - at + 1);
    row->size++;

    // Copy buffer to row chars inserted
    row->chars[at] = c;


    editorUpdateRow(row);
   
    // Insert unicode char
    insertChar(row->alc,at,c);

    editorUpdateUnicodeRow(row);
    
    E.dirty++;//Mark changed

}

void editorRowAppendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    //editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
    if(at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);

    // For unicode char
    alchars alc = row->alc;
    deleteBucketAt(alc,at);
    editorUpdateUnicodeRow(row);
    E.dirty++;
}


/*** editor operations ***/
void editorInsertChar(int c) {
    if(E.cy == E.numrows) {
        editorInsertRow(E.numrows,"",0);
    }
    editorRowInsertChar(&E.row[E.cy],E.cx,c);
    E.cx++;
}

/* Insert new line*/
void editorInsertNewLine() {
    if (E.cx == 0) {
        editorInsertRow(E.cy,"",0);
    } else {
        /*
        editorInsertRow(E.cy + 1, &row->chars[E.cx],row->size - E.cx);
        // Truncate string
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        //editorUpdateRow(row);
        */
        /* TODOS: Error here Crash whole PC*/
        char *line = getString(E.row[E.cy].alc);
        editorInsertRow(E.cy+1, &line[E.cx],E.row[E.cy].size - E.cx);
        erow *row = &E.row[E.cy]; 
        alchars alc = row->alc;

        deleteBuckets(alc,E.cx,-1);

        row->size = E.cx;
        editorUpdateUnicodeRow(&E.row[E.cy]);
        free(line);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar() {
    if (E.cy == E.numrows ) return;
    if (E.cx == 0 && E.cy == 0) return;
    erow *row = &E.row[E.cy];
    if(E.cx > 0) {
        editorRowDelChar(row,E.cx - 1);
        E.cx--;
    } else {
        // When cursor at begin a line
        // And remove back to previous line
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

/*** File I/O ***/

/**
 * Read from rows characters and store in long string
 */
char *editorRowsToString(int *buflen) {
    int totlen = 0;
    int j;
    for(j = 0; j < E.numrows; j++) {
        totlen += E.row[j].size + 1;
    }
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for(j = 0; j < E.numrows; j++) {
        memcpy(p,E.row[j].chars, E.row[j].size);
        p += E.row[j].size; // move pointer to next new line
        *p = '\n';
        p++;
    }
    return buf;
}

void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename,"r");
    if(!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    // read all lines in the file by the loop
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        // Abandon newline characters
        while(linelen > 0 && (line[linelen - 1] == '\n' ||
                              line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows,line,linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s",NULL);
        if(E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowsToString(&len);

    // Open creat a new file and Read write to a file
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    // 0644 si standard permissions you usally want for text file
    // see more: http://www.filepermissions.com/directory-permission/0644
    if (fd != -1) {
        // set file size
        if(ftruncate(fd,len) != -1) {
            // Write to file
            if(write(fd,buf,len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s",strerror(errno));
}

/*** find ***/

void editorFindCallback(char *query, int key) {
    /* Search forward and backward*/
    static int last_match = -1;
    static int direction = 1;

    if(key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if(key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if(key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }
    if(last_match == -1) direction = 1;
    int current = last_match;

    int i;
    for(i = 0; i < E.numrows; i++) {
        current += direction;
        // go to tail of file
        if(current == -1) current = E.numrows - 1;
        // Go to head file for search
        else if(current == E.numrows) current = 0;

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if(match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row,match - row->render);
            E.rowoff = E.numrows;
            break;
        }
    }
}

void editorFind() {

    // Restore cursor position
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter to cancel)",editorFindCallback);
    if(query) {
        free(query);
    }else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

/*** Output ***/

void editorScroll() {
    E.rx = 0;
    if(E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    // scroll vertical
    if(E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    // scroll horizontal
    if(E.cx < E.coloff) {
        E.coloff = E.rx;
    }
    if(E.cx >= E.coloff + E.screencols) {
        E.coloff = E.rx = E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for( y = 0; y < E.screenrows; y++ ) {
        int filerow = y + E.rowoff;
        if(filerow >= E.numrows) {
            // Write information version in the midle
            if(E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[90];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                        "Ghi editor -- version %s", GHI_VERSION);
                if(welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if(padding) {
                    abAppend(ab,"~",1);
                    padding--;
                }
                while(padding--) abAppend(ab," ",1);
                abAppend(ab,welcome,welcomelen);
            } else {
                // Add signal start line like in vim
                abAppend(ab,"~",1);
            }

        } else {
            /*
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            char *c = &E.row[filerow].render[E.coloff];
            int j;
            for(j = 0; j < len; j++) {
                if(isdigit(c[j])) {
                    abAppend(ab,"\x1b[31m",5);
                    abAppend(ab,&c[j],1);
                    abAppend(ab,"\x1b[39m",5);
                }else {
                    abAppend(ab,&c[j],1);
                }
            }
            */

            /*Render unicode*/
            alchars alc = E.row[filerow].renderAlc;
            int len = getLen(alc) - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            int j;
            for(j = 0; j < len; j++) {
                achar *ac = getBucketAt(alc,j);
                abAppend(ab,ac->bytes,ac->length);
            }
        }
        abAppend(ab,"\x1b[K",3);// Clear a line before add line to display out
        abAppend(ab,"\r\n",2);
    }
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab,"\x1b[7m",4); // switch to inverted colors
    char status[80],rstatus[80];
    int len = snprintf(status, sizeof(status),"%.20s - %d lines %s",
            E.filename ? E.filename:"[No Name]",E.numrows,
            E.dirty ? "(modified)" :"");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
            E.cy + 1, E.numrows);
    if(len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while(len < E.screencols) {
        if(E.screencols - len == rlen) {
            abAppend(ab,rstatus,rlen);
            break;
        } else {
            abAppend(ab," ",1);
            len++;
        }
        abAppend(ab," ",1);
        len++;
    }
    abAppend(ab,"\x1b[m",3); // Switch to normal color
    abAppend(ab,"\r\n",2);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab,"\x1b[K",3); // Erase to end of line
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if(msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab,E.statusmsg, msglen);
}

void editorRefreshScreen() {
    editorScroll();

    // Initialize append buffer
    struct abuf ab = ABUF_INIT;

    // Write out screen
    abAppend(&ab,"\x1b[?25l",6); /*Hide cursor*/
    abAppend(&ab,"\x1b[H",3);/*Go home*/

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    // Expand screen area
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + 1);
    abAppend(&ab,buf,strlen(buf));

    abAppend(&ab,"\x1b[?25h",6);/* Show cursor */

    // Print out screen allocation buffer that was write
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input  ***/

/* Pointer function */
char *editorPrompt(char *prompt, void (*callback)(char *,int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1) {
        editorSetStatusMessage(prompt,buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if( c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE ) {
            if(buflen != 0) buf[--buflen] = '\0';
        } else if(c == '\x1b') {
            editorSetStatusMessage("");
            if(callback) {
                callback(buf,c);
            }
            free(buf);
            return NULL;
        }
        if(c == '\r') {
            if(buflen != 0) {
                editorSetStatusMessage("");
                if(callback) {
                    callback(buf,c);
                }
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if(buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf,bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }


        if(callback) {
            callback(buf,c);
        }
    }
}

void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            // X not negative
            if(E.cx != 0) {
                E.cx--;
            } else if(E.cy > 0) { // Move end previous line
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size) {
                E.cx++;
            } else if(row && E.cx == row->size) {// Move start next line
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    // Snap cursor to the end line
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen) {
        E.cx = rowlen;
    }
}
void editorProcessKeypress() {
    static int quit_times = GHI_QUIT_TIMES;

    int c = editorReadKey();

    switch(c) {
        case '\r':
            editorInsertNewLine();
            break;
        case CTRL_KEY('q'):
            if(E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO,"\x1b[2J",4);// Clear screen
            write(STDOUT_FILENO,"\x1b[H",3);
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            if(E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;
        case CTRL_KEY('f'):
            editorFind();
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c == PAGE_UP) {
                    E.cy = E.rowoff;
                } else if(c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if(E.cy > E.numrows) E.cy = E.numrows;
                }

                int times = E.screenrows;
                while(times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_RIGHT:
        case ARROW_LEFT:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b': // Escape key
            /*ToDos*/
            break;
        default:
            editorInsertChar(c); // Whenever type will be inserted
            break;
    }

    quit_times = GHI_QUIT_TIMES;
}

/*** Unicode Vienamese ***/

void convertToUnicode(struct abuf *ab, unsigned codePoint) {
    char buf[4];
    if(codePoint < 0x80) {
        buf[0] = codePoint;
        abAppend(ab,buf,1);
    } else if (codePoint <= 0x7FF) {
        buf[0] = ((codePoint >> 6) + 0xC0);
        buf[1] = ((codePoint & 0x3F) + 0x80);
        abAppend(ab,buf,2);
    } else if (codePoint <= 0xFFFF) {
        buf[0] = ((codePoint >> 12) + 0xE0);
        buf[1] = (((codePoint >> 6) &0x3F) +0x80);
        buf[2] = ((codePoint & 0x3F) + 0x80);
        abAppend(ab,buf,3);
    } else if(codePoint <= 0x10FFFF) {
        buf[0] = ((codePoint >> 18) + 0xF0);
        buf[1] = (((codePoint >> 12) & 0x3F) + 0x80);
        buf[2] = (((codePoint >> 6) & 0x3F) + 0x80);
        buf[3] = ((codePoint & 0x3F) + 0x80);
        abAppend(ab,buf,4);
    } else {
        fprintf(stderr,"Error");
    }
}


/*** init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if(getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowSize");
    }

    E.screenrows -= 2;

}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if(argc >= 2) {
        editorOpen(argv[1]);
    }
    editorSetStatusMessage("HELP: Crl-Q = quit | Ctrl-Q = quit | Ctrl-F = find");

    /* Read from stdin key */
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }


    return 0;
}
