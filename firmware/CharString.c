//
//  Character String
//

#include "CharString.h"

CharString_Iter CharString_begin (
    const CharString_t *str)
{
    return &str->body[0];
}

CharString_Iter CharString_end (
    const CharString_t *str)
{
    return &str->body[0] + str->length;
}

void CharString_clear (
    CharString_t* str)
{
    str->length = 0;
    str->body[0] = 0;
}

void CharString_append (
    const char* srcStr,
    CharString_t* destStr)
{
    CharString_appendSubstringCS(srcStr, strlen(srcStr), destStr);
}

void CharString_appendP (
    PGM_P srcStr,
    CharString_t* destStr)
{
    CharString_appendSubstringP(srcStr, strlen_P(srcStr), destStr);
}

void CharString_appendSubstringP (
    PGM_P substrBegin,
    const uint8_t substrLen,
    CharString_t* destStr)
{
    const uint8_t capacity = destStr->capacity;
    const uint8_t remainingCapacity = capacity - destStr->length;
    const uint8_t charsToAppend =
        (remainingCapacity < substrLen)
        ? remainingCapacity
        : substrLen;
    strncpy_P(destStr->body + destStr->length, substrBegin, charsToAppend);
    destStr->length += charsToAppend;
    destStr->body[destStr->length] = 0;
}

void CharString_appendCS (
    const CharString_t* srcStr,
    CharString_t* destStr)
{
    CharString_appendSubstringCS(srcStr->body, srcStr->length, destStr);
}

void CharString_appendSubstringCS (
    CharString_Iter substrBegin,
    const uint8_t substrLen,
    CharString_t* destStr)
{
    const uint8_t capacity = destStr->capacity;
    const uint8_t remainingCapacity = capacity - destStr->length;
    const uint8_t charsToAppend =
        (remainingCapacity < substrLen)
        ? remainingCapacity
        : substrLen;
    strncpy(destStr->body + destStr->length, substrBegin, charsToAppend);
    destStr->length += charsToAppend;
    destStr->body[destStr->length] = 0;
}

void CharString_appendC (
    const char ch,
    CharString_t* destStr)
{
    if (destStr->length < destStr->capacity) {
        char* cp = destStr->body + destStr->length;
        *cp++ = ch;
        *cp = 0;
        ++destStr->length;
    }
}

void CharString_copyIters (
    CharString_Iter begin,
    CharString_Iter end,
    CharString_t* destStr)
{
    CharString_clear(destStr);
    CharString_appendSubstringCS(begin, end - begin, destStr);
}

void CharString_appendNewline (
    CharString_t* destStr)
{
    CharString_appendP(PSTR("\r\n"), destStr);
}

void CharString_truncate (
    const uint8_t truncatedLength,
    CharString_t* destStr)
{
    if (truncatedLength < destStr->length) {
        destStr->length = truncatedLength;
        destStr->body[truncatedLength] = 0;
    }
}
