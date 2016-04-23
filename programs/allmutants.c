#include "config.h"
extern int covered;


int
js_BoyerMooreHorspool3(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i <= BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool4(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i > BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool5(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i >= BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool6(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i == BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool7(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i != BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool8(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_op) */    for (i = 0; i < BMH_CHARSET_SIZE; i--)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool9(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_const) */    for (i = 1; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool10(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
 /* MUTANT (rep_const) */    for (i = -1; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool11(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
 /* MUTANT (del_stmt) */ /*     m = patlen - 1; */ 
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool12(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
 /* MUTANT (rep_const) */    m = patlen - 0;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool13(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
 /* MUTANT (rep_const) */    m = patlen - -1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool14(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
 /* MUTANT (rep_const) */    m = patlen - ((1)+1);
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool15(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i <= m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool16(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i > m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool17(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i >= m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool18(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i == m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool19(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i != m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool20(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_op) */    for (i = 0; i < m; i--) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool21(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_const) */    for (i = 1; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool22(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
 /* MUTANT (rep_const) */    for (i = -1; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool23(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
 /* MUTANT (del_stmt) */ /*         c = pat[i]; */ 
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool24(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (rep_op) */        if (c <= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool25(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (rep_op) */        if (c < BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool26(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (rep_op) */        if (c > BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool27(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (rep_op) */        if (c == BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool28(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (rep_op) */        if (c != BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool29(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
 /* MUTANT (negate) */        if(! (c >= BMH_CHARSET_SIZE))
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool30(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
 /* MUTANT (del_stmt) */ /*         skip[c] = (unsigned char)(m - i); */ 
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool31(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    covered = 1;
 /* MUTANT (rep_op) */    for (k = start - m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool32(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
 /* MUTANT (rep_op) */    for (k = start * m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool33(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
 /* MUTANT (rep_op) */    for (k = start % m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}





int
js_BoyerMooreHorspool35(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
 /* MUTANT (rep_op) */         k <= textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool36(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
 /* MUTANT (rep_op) */         k > textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool37(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
 /* MUTANT (rep_op) */         k >= textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool38(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
 /* MUTANT (rep_op) */         k == textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool39(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
 /* MUTANT (rep_op) */         k != textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool40(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k -= ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool41(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k *= ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool42(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k %= ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool43(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    covered = 1;
    for (k = start + m;
         k < textlen;
	 /* MUTANT (rep_op) */         k += ((covered = 1, c = text[k]) <= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool44(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k += ((c = text[k]) < BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool45(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k += ((c = text[k]) > BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool46(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k += ((c = text[k]) == BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool47(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
 /* MUTANT (rep_op) */         k += ((c = text[k]) != BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool48(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
 /* MUTANT (rep_op) */        for (i = k, j = m; ; i++, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool49(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
 /* MUTANT (rep_op) */        for (i = k, j = m; ; i--, j++) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool50(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_op) */            if (j <= 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool51(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_op) */            if (j > 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool52(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_op) */            if (j >= 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool53(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_op) */            if (j == 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool54(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_op) */            if (j != 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool55(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_const) */            if (j < 1)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool56(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (rep_const) */            if (j < -1)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool57(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
 /* MUTANT (negate) */            if(! (j < 0))
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool58(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_op) */                return i - 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool59(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_op) */                return i * 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool60(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_op) */                return i % 1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool61(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_const) */                return i + 0;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool62(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_const) */                return i + -1;
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool63(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
 /* MUTANT (rep_const) */                return i + ((1)+1);
            if (text[i] != pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool64(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
 /* MUTANT (rep_op) */            if (text[i] == pat[j])
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool65(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
 /* MUTANT (negate) */            if(! (text[i] != pat[j]))
                break;
        }
    }
    return -1;
}


int
js_BoyerMooreHorspool66(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
 /* MUTANT (rep_const) */    return 0;
}


int
js_BoyerMooreHorspool67(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
 /* MUTANT (rep_const) */    return 1;
}


int
js_BoyerMooreHorspool68(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
 /* MUTANT (rep_const) */    return ((-1)+1);
}


int
js_BoyerMooreHorspool69(const char *text, int textlen,
                      const char *pat, int patlen,
                      int start)
{
    int i, j, k, m;
    unsigned char skip[BMH_CHARSET_SIZE];
    char c;

    //    assert(0 < patlen && patlen <= BMH_PATLEN_MAX);
    for (i = 0; i < BMH_CHARSET_SIZE; i++)
        skip[i] = (unsigned char)patlen;
    m = patlen - 1;
    for (i = 0; i < m; i++) {
        c = pat[i];
        if (c >= BMH_CHARSET_SIZE)
            return BMH_BAD_PATTERN;
        skip[c] = (unsigned char)(m - i);
    }
    for (k = start + m;
         k < textlen;
         k += ((c = text[k]) >= BMH_CHARSET_SIZE) ? patlen : skip[c]) {
        for (i = k, j = m; ; i--, j--) {
            if (j < 0)
                return i + 1;
            if (text[i] != pat[j])
                break;
        }
    }
 /* MUTANT (rep_const) */    return ((-1)-1);
}




