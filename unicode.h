/* ============================================================
   *File : unicode.h
   *Date : 2017-09-08
   *Creator : @congdv
   *Description : Header file
   ============================================================ */
#ifndef UNICODE_H
#define UNICODE_H

typedef struct achar achar;
struct achar {
    char *bytes;
    int length;
};

typedef struct alchars *alchars; // Definition to itselft

alchars newChar();
/* Create new char*/
alchars createNewChar(unsigned c);
/* Add new char*/
void appendNewChar(alchars alc,unsigned c);

void insertChar(alchars alc, int at,unsigned c);
/* Add new string */
void appendNewString(alchars alc,const char *s);

/* Add new string with len*/
void appendNewStringWithLen(alchars alc,const char *s,int len);

/* Free chars*/
void freeChars(alchars alc);

/* Get string */
const char *getStringPointer(alchars alc);

/* Get string length*/
int getStringLen(const char *s);

/* Get length buckets*/
int getLen(alchars alc);

/* Get string value of bucket at index*/
achar *getBucketAt(alchars alc,int index);

/* Delete a bucket at index*/
void deleteBucketAt(alchars alc, int index);

/* Delete buckets from a to b index*/
void deleteBuckets(alchars,int from, int to);

#endif // End UNICODE_H
