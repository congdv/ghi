/* ============================================================
   *File : test_utf_8.c
   *Date : 2017-09-06
   *Creator : @congdv
   *Description : Test utf-8 in vietnamese output
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>


#define VOWEL_SOUND "eoEO"



struct termios orig_terminos;

void die(const char *s) {
    perror(s);
    exit(1);
}

struct abuf {
    char *chars;
    int len;
};

int isValidChangeChars(struct abuf *ab, int c);
void replaceUnicodeChars(struct abuf *ab, int c); 

#define ABUF_INIT {NULL,0} // Represent constructor for append bufffer
void abAppend(struct abuf *ab,int c) {
    if(isValidChangeChars(ab,c)) {
        replaceUnicodeChars(ab,c);
    } else {
        char *new = realloc(ab->chars,ab->len + 1);
        if(new == NULL) return;
        
        new[ab->len] = c;
        ab->len++;
        ab->chars = new;
    }
}

void abAppendString(struct abuf *ab,const char *s, int len) {
    char *new = realloc(ab->chars,ab->len + len);
    if(new == NULL) return;
    
    memcpy(&new[ab->len],s,len);
    ab->chars = new;
    ab->len += len;
}

void removeLastChars(struct abuf *ab,int len) {
    if(ab->len == 0 || ab->len - len <= 0)
        return;
    char *new = realloc(ab->chars,ab->len - len);
    ab->chars = new;
    ab->len -= len;
}

void write_utf8(struct abuf *ab, unsigned codePoint) {
    if(codePoint < 0x80) {
    } else if (codePoint <= 0x7FF) {
        char buf[2];
        buf[0] = ((codePoint >> 6) + 0xC0);
        buf[1] = ((codePoint & 0x3F) + 0x80);
        removeLastChars(ab,1);
        abAppendString(ab,buf,2);
    } else if (codePoint <= 0xFFFF) {
        char buf[3];
        buf[0] = ((codePoint >> 12) + 0xE0);
        buf[1] = (((codePoint >> 6) &0x3F) +0x80);
        buf[2] = ((codePoint & 0x3F) + 0x80);
        removeLastChars(ab,1);
        abAppendString(ab,buf,3);
    } else if(codePoint <= 0x10FFFF) {
        char buf[4];
        buf[0] = ((codePoint >> 18) + 0xF0);
        buf[1] = (((codePoint >> 12) & 0x3F) + 0x80);
        buf[2] = (((codePoint >> 6) & 0x3F) + 0x80);
        buf[3] = ((codePoint & 0x3F) + 0x80);
        removeLastChars(ab,1);
        abAppendString(ab,buf,4);
    } else {
        fprintf(stderr,"Error");
    }
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_terminos);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO,&orig_terminos);
    atexit(disableRawMode);

    struct termios raw = orig_terminos;
    raw.c_lflag &= ~(ECHO | ICANON);

    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
}

int isValidChangeChars(struct abuf *ab, int c) {
    char *str = ab->chars;
    if(ab->len == 0)
        return 0;
    if(str[ab->len - 1] == c && c == 'e')
        return 1;
    return 0;
}

void replaceUnicodeChars(struct abuf *ab, int c) {
    if(c == 'e') {
        write_utf8(ab,0x00EA);
    }
}


int main() {

    struct abuf ab = ABUF_INIT;
    struct abuf screen = ABUF_INIT;

    char c;
    int count_char = 0;
    enableRawMode();
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        abAppendString(&screen,"\x1b[2J",4);
        abAppendString(&screen,"\x1b[H",3);
        //abAppendString(&ab,"\x1b[K",3);
        abAppend(&ab,c);
        write(STDOUT_FILENO,screen.chars,screen.len);
        write(STDOUT_FILENO,ab.chars,ab.len);
        if(c == 'e' && count_char == 0) {
            count_char++;
        } else if(c == 'e' && count_char == 1) {
            count_char++;
        }
    }
    /*
    write_utf8(0x0323);
    write_utf8(0x1EAC);
    write_utf8(0x00EA);
    */
    return 0;
}

