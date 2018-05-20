//
//  Character String Span
//
//  What it does:
//    Provides a span (i.e. substring) of characters within a CharString
//
//  How to use it:
//    Define a Span from a CharString like this:
//       CharStringSpan_init(str);
//
#ifndef CHARSTRINGSPAN_H
#define CHARSTRINGSPAN_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharString.h"

typedef struct CharStringSpan_struct {
    CharString_Iter m_begin;
    CharString_Iter m_end; // non-inclusive
} CharStringSpan_t;

extern void CharStringSpan_init (
    const CharString_t *str,
    CharStringSpan_t *span);

// initializes 'span' to the characters of 'str' starting
// at offset
extern void CharStringSpan_initRight (
    const CharString_t *str,
    const int offset,
    CharStringSpan_t *span);

// takes the first numChars characters out of 'span'
// and returns them in 'left'. If offset is beyond
// the end of the span the whole span goes to 'left'
extern void CharStringSpan_extractLeft (
    const int numChars,
    CharStringSpan_t *span,
    CharStringSpan_t *left);

inline void CharStringSpan_clear (
    CharStringSpan_t *span)
{
    span->m_begin = 0;
    span->m_end = 0;
}

inline CharString_Iter CharStringSpan_begin (
    const CharStringSpan_t *span)
{
    return span->m_begin;
}

inline CharString_Iter CharStringSpan_end (
    const CharStringSpan_t *span)
{
    return span->m_end;
}

extern char CharStringSpan_front(
    const CharStringSpan_t *span);

inline void CharStringSpan_setBegin (
    const CharString_Iter begin,
    CharStringSpan_t *span)
{
    span->m_begin = begin;
}

inline void CharStringSpan_setEnd (
    const CharString_Iter end,
    CharStringSpan_t *span)
{
    span->m_end = end;
}

extern void CharStringSpan_set (
    const CharString_Iter begin,
    const CharString_Iter end,
    CharStringSpan_t *span);

extern void CharStringSpan_incrBegin (
    CharStringSpan_t *span);

inline bool CharStringSpan_isEmpty (
    const CharStringSpan_t* span)
{
    return (span->m_begin == span->m_end);
}

inline uint8_t CharStringSpan_length (
    const CharStringSpan_t* span)
{
    return (uint8_t)(span->m_end - span->m_begin);
}

extern bool CharStringSpan_equalsP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr);

extern bool CharStringSpan_equalsNocaseP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr);

extern int CharStringSpan_compareP (
    const CharStringSpan_t* leftSpan,
    PGM_P rightStr);

#endif  // CHARSTRINGSPAN_H