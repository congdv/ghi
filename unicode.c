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

   TODOS: Use link-list to store
   ============================================================ */
#include "unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


struct alchars {
    achar *head;
    int currentAt;
    int length;
    achar *tail;
    achar *currentBucket;
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
    achar *ac = decode(c);
    if(alc->head == NULL) {
        alc->tail = alc->head = ac;
        ac->previous = ac->next = NULL;
        alc->currentBucket = alc->head;
    }else {
        ac->previous = alc->tail;
        ac->next = NULL;

        alc->tail->next = ac;
        alc->tail = ac;

    }
    alc->length++;
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
    alc->head = alc->tail = NULL;
    alc->length = 0;
    alc->currentAt = 0;
    alc->currentBucket = alc->head;
    return alc;
}


achar *getBucketAt(alchars alc,int index) {
    if(index >= alc->length)
        return NULL;
    if(index == 0) {
        alc->currentAt = 0;
        alc->currentBucket = alc->head;
        return alc->head;
    }
    if(alc->currentAt == index) {
        return alc->currentBucket;
    } else if(alc->currentAt < index) {
        while(alc->currentAt < index) {
            alc->currentBucket = alc->currentBucket->next;
            alc->currentAt++;
        }
        return alc->currentBucket;
    } else {
        while(alc->currentAt > index) {
            alc->currentBucket = alc->currentBucket->previous;
            alc->currentAt--;
        }
        return alc->currentBucket;

    }
}

void insertChar(alchars alc, int at,unsigned c) {

    achar *ac = decode(c);
    // Insert into head of list
    if(at <= 0) {
        ac->next = alc->head;
        ac->previous = NULL;

        alc->head->previous = ac;
        alc->head = ac;

        alc->currentAt++;
    }
    // Insert into tail of list
    else if(at >= alc->length) {
        ac->previous = alc->tail;
        ac->next = NULL;

        alc->tail->next = ac;
        alc->tail = ac;
    } else {
       achar *temp = getBucketAt(alc,at); 
       achar *prevTemp = temp->previous;

       // Connect previous bucket
       ac->previous = prevTemp;
       prevTemp->next = ac;

       // Connect to current bucket
       ac->next = temp;
       temp->previous = ac;
    }
    alc->length++;

}

void freeAchar(achar *ac) {
    free(ac->bytes);
    free(ac);
}

void freeAllBucket(alchars alc) {
    while(alc->tail) {
        achar *temp = alc->tail;
        alc->tail = alc->tail->previous;
        freeAchar(temp);
    }
}
void freeChars(alchars alc) {
    freeAllBucket(alc);
    free(alc);
}
void deleteBucketAt(alchars alc, int index) {
    if(index < 0 && index >= alc->length) 
        return;

    if(index == 0) {
        achar *temp = alc->head;
        alc->head = temp->next;
        alc->head->previous = NULL;
        freeAchar(temp);

        if(alc->currentAt == 0) {
            alc->currentBucket = alc->head;
        } else {
            alc->currentAt--;
        }
    } else if(index == alc->length - 1) {
        achar *temp = alc->tail;
        alc->tail = temp->previous;
        alc->tail->next = NULL;
        freeAchar(temp);

        if(alc->currentAt == alc->length - 1) {
            alc->currentAt--;
            alc->currentBucket = alc->tail;
        }
    } else {
        achar *temp = getBucketAt(alc,index); 

        achar *prevTemp = temp->previous;
        achar *nextTemp = temp->next;

        nextTemp->previous = prevTemp;
        prevTemp->next = nextTemp;

        alc->currentBucket = nextTemp;

        freeAchar(temp);
    }

    alc->length--;
    if(alc->length == 0) {
        alc->currentAt = 0;
        alc->currentBucket = NULL;
        alc->head = NULL;
        alc->tail = NULL;
    } 
}

void deleteFromBucketTo(achar *fromBucket, achar *toBucket) {
    while(fromBucket != toBucket)  {
        achar *temp = fromBucket;
        fromBucket = fromBucket->next;
        freeAchar(temp);
    }
    freeAchar(toBucket);
}
void deleteBuckets(alchars alc,int from, int to) {
    int len = getLen(alc);
    // Change suitable index
    if(from == -1) {
        from = 0;
    }
    if(to == -1 || to > len - 1) {
        to = len - 1;
    }

    if(to < from)
        return;

    // Check if delete all buckets
    if(from == 0 && to == len - 1) {
        freeAllBucket(alc);
        alc->tail = alc->head = NULL;
        alc->currentAt = 0;
        alc->currentBucket = NULL;
        alc->length = 0;
        return;
    }

    // Head segment
    if(from == 0) {
        achar *toBucket = getBucketAt(alc,to);
        achar *nextToBucket = toBucket->next;
        deleteFromBucketTo(alc->head,toBucket);
        alc->head = nextToBucket;
        alc->head->previous = NULL;

        // Refresh current index
        if(alc->currentAt > to) {
            alc->currentAt -= (to - from + 1);
        } else {
            alc->currentAt = 0;
            alc->currentBucket = alc->head;
        }

    } else if (to == len - 1) {
        achar *fromBucket = getBucketAt(alc,from);
        achar *previousFromBucket = fromBucket->previous;
        previousFromBucket->next = NULL;

        deleteFromBucketTo(fromBucket,alc->tail);
        alc->tail = previousFromBucket;

        // Refresh current index
        if(alc->currentAt > from) {
            alc->currentAt = from - 1;
        } else {
            alc->currentBucket = alc->tail;
        }

    } else {
        achar *fromBucket = getBucketAt(alc,from);
        achar *toBucket = getBucketAt(alc,to);

        // Delete buckets
        achar *previousFromBucket = fromBucket->previous;
        achar *nextToBucket = toBucket->next;
        
        previousFromBucket->next = nextToBucket;
        nextToBucket->previous = previousFromBucket;

        deleteFromBucketTo(fromBucket,toBucket);
        if(alc->currentAt > to) {
            alc->currentAt -= (to - from + 1);
        } else if(alc->currentAt >= from && alc->currentAt <= to) {
            alc->currentAt = from -  1;
            alc->currentBucket = previousFromBucket;
        }
    }
    
    alc->length -= (to - from + 1);

}
char *getString(alchars alc) {
    char *str = malloc(sizeof(char)); 
    int len = 0;
    achar *temp = alc->head;
    while(temp) {
        str= realloc(str,len + temp->length);
        memcpy(&str[len],temp->bytes,temp->length);

        len+=temp->length;

        temp = temp->next;
    }
    str= realloc(str,len + 1);
    str[len] = '\0';
    return str;
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
int getStringLen(const char *s) {
    alchars alc = newChar();
    encode(alc,s,strlen(s));
    int len = alc->length;

    freeChars(alc);
    return len;
}
