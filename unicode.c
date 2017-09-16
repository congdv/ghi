/* ============================================================
   *File : unicode.c
   *Date : 2017-09-07
   *Creator : @congdv
   *Description : Built library for support read and write
character
   Struct data like:
   [1]-[2]
    |   \
    V    >
    [0x41(a)][0x00EA(e^)]
    When read each element and write out screen
    UTF-8 Table
    With UTF-8 first byte present number of bytes when read bit 1 on first
    Bits of code point | First cp| Last cp | Bytes | Byte 1  | Byte 2  | Byte 3  | Byte 4  |
            7          | 0x0000  | 0x007F  | 1     |0xxxxxxxx|
           11          | 0x0080  | 0x07FF  | 2(110)|110xxxxxx|10xxxxxxx|
           16          | 0x0800  | 0xFFFF  | 3     |1110xxxxx|10xxxxxxx|10xxxxxxx|
           21          | 0x10000 | 0x1FFFFF| 4     |11110xxxx|10xxxxxxx|10xxxxxxx|10xxxxxxx|
   ============================================================ */
#include "unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


struct alchars {
    achar **chars;
    int length;
};

typedef struct buffers {
    char *s;
    int len;
} buffers;


#define INIT {NULL,0}

buffers bf = INIT;

void refreshBuffers() {
    if(bf.s != NULL)  {
        free(bf.s);
        bf.s = NULL;
        bf.len = 0;
    }
    
}
/*Decode Utf-8*/
achar *decode( unsigned codePoint) {
    achar *chars = malloc(sizeof(achar));

    if(codePoint < 0x80) {
        chars->bytes = malloc(sizeof(char));
        chars->length = 1;
        chars->bytes[0] = codePoint & 0xFF;
    } else if (codePoint <= 0x7FF) {
        char *buf = malloc(sizeof(char) * 2);
        buf[0] = ((codePoint >> 6) + 0xC0);
        buf[1] = ((codePoint & 0x3F) + 0x80);
        chars->bytes = buf;
        chars->length = 2;
    } else if (codePoint <= 0xFFFF) {
        char *buf = malloc(sizeof(char) * 3);
        buf[0] = ((codePoint >> 12) + 0xE0);
        buf[1] = (((codePoint >> 6) &0x3F) +0x80);
        buf[2] = ((codePoint & 0x3F) + 0x80);
        chars->bytes = buf;
        chars->length = 3;
    } else if(codePoint <= 0x10FFFF) {
        char *buf = malloc(sizeof(char) * 4);
        buf[0] = ((codePoint >> 18) + 0xF0);
        buf[1] = (((codePoint >> 12) & 0x3F) + 0x80);
        buf[2] = (((codePoint >> 6) & 0x3F) + 0x80);
        buf[3] = ((codePoint & 0x3F) + 0x80);
        chars->bytes = buf;
        chars->length = 4;
    } else {
        chars->bytes = NULL;
        chars->length = 0;
    }
    return chars;
}

/*From bytes with utf-8 format will be */
void encode(struct alchars *alc,const char *s, int numByte) { 
    /*When enter a string contain utf-8 char
     * This char will be 8 byte but last byte contain data
     * What you need to encode become utf-8
     * So need & 0xff to get last byte*/
    for(int i = 0; i < numByte; i++) {
        if((unsigned char)s[i] < 0x80) {
            appendNewChar(alc,s[i]);
        } else if((s[i]&0xff) < 0xE0) {
            /*Do encode*/
            /*Check next byte is valid*/
            if(i+1 < numByte && ((s[i+1]&0xff) & 0xC0) == 0x80) {
                appendNewChar(alc,((s[i]&0xff) << 6) + (s[i+1] & 0xff) - 0x3080);
                i++;
            }
        } else if((s[i]&0xff) < 0xF0) {
            if(i + 1 < numByte && i + 2 < numByte) {
                appendNewChar(alc,((s[i]&0xff) << 12) + ((s[i+1]&0xff) << 6) + (s[i+2]&0xff) - 0xE2080);
                i+=2;
            }
        }else if((s[i] & 0xff) < 0xF5) {
            if(i + 1 < numByte && i + 2 < numByte && i+3 < numByte) {
                appendNewChar(alc,((s[i] & 0xff) << 18 )+ ((s[i+1] & 0xff) << 12) + ((s[i+2] & 0xff) << 6) + (s[i+3] & 0xff) - 0x3C82080);
                i+=3;
            }
        }
    }
}

/* Append a utf-8 char*/
void appendNewChar(alchars alc,unsigned c){
    achar **newAlloc = realloc(alc->chars, (alc->length + 1)*sizeof(achar *));
    if(newAlloc == NULL)
       return;

    newAlloc[alc->length] = decode(c);
    alc->length++;
    alc->chars = newAlloc;
}


/* Append to struct for store string*/
void bufAppend(buffers *buf, char *s, int len) {
    // Add Null char
    char *new = realloc(buf->s,buf->len + len);
    if(new == NULL)
        return;
    memcpy(&new[buf->len],s,len);
    buf->s = new;
    buf->len += len;
}

void bufFree(buffers *buf) {
    if(buf != NULL && buf->len > 0)
        free(buf->s);
}

alchars newChar(void) {
    alchars alc = malloc(sizeof(struct alchars));
    alc->chars = NULL;
    alc->length = 0;
    return alc;
}
/* Create new char*/
alchars createNewChar(unsigned c) {
    alchars alc = malloc(sizeof(struct alchars));
    alc->length = 0;
    alc->chars = NULL;
    appendNewChar(alc,c);
    return alc;
}

void freeAlchars(alchars alc) {
    if(alc != NULL) {
        achar ** chars = alc->chars;
        for(int i = 0; i < alc->length; i++) {
            if(chars[i]) {
                free(chars[i]->bytes);
                free(chars[i]);
            }
        }
        free(chars);
        free(alc);
    }
}
/* Free chars*/
void freeChars(alchars alc) {
    freeAlchars(alc);
    bufFree(&bf);
}

/* Get string */
const char *getStringPointer(alchars alc) {
    refreshBuffers();
    achar ** chars = alc->chars;
    for(int i = 0; i < alc->length; i++) {
        bufAppend(&bf,chars[i]->bytes,chars[i]->length);
    }
    bufAppend(&bf,"\0",1);
    return bf.s;
}


int getStringLen(const char *s) {
    alchars alc = newChar();
    encode(alc,s,strlen(s));
    int len = alc->length;
    freeAlchars(alc);
    return len;
}
int getLen(alchars alc) {
    return alc->length;
}
/* Add new string */
void appendNewString(alchars alc,const char *s) {
    encode(alc,s,strlen(s));
}

void appendNewStringWithLen(alchars alc,const char *s,int len) {
    encode(alc,s,len);
}
achar *getBucketAt(alchars alc,int index) {
    if(index < 0 && index >= alc->length)
        return NULL;
    // Get value of bucket
    return alc->chars[index];
}

void insertChar(alchars alc, int at,unsigned c) {
    if(at < 0 && at > alc->length)
        return;
    // Extend 1 bucket
    alc->chars  = realloc(alc->chars, (alc->length + 1)*(sizeof(achar *)));
    
    // Shift right bytes pointer to 
    for(int i = alc->length; i > at; i--) {
       alc->chars[i] = alc->chars[i-1]; 
    }

    // Set index of bucket
    alc->chars[at] = decode(c);
    alc->length++;

}


void deleteBucketAt(alchars alc, int index) {
    if(index < 0 && index >= alc->length) 
        return;

    achar *ac = alc->chars[index]; 

    // Shift left bytes pointer to 
    for(int i = index; i < alc->length - 1; i++) {
       alc->chars[i] = alc->chars[i+1]; 
    }

    // Free bucket index
    free(ac->bytes);
    free(ac);
    alc->length--;
}
