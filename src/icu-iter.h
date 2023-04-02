#ifndef _LEXD_ICU_ITER_H_
#define _LEXD_ICU_ITER_H_

#include <unicode/brkiter.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ustring.h>
#include <lttoolbox/ustring.h>
#include <map>
#include <string>

class charspan_iter
{
  private:
    icu::BreakIterator* it;
    UErrorCode _status;
    const icu::UnicodeString *s;
    std::pair<int, int> _span;
  public:
    charspan_iter(const icu::UnicodeString &s);
    charspan_iter(const charspan_iter &other);
    friend charspan_iter rev_charspan_iter(const icu::UnicodeString &s);
    
    ~charspan_iter();
    
    const UErrorCode &status() const;
    const std::pair<int, int> &operator*() const;
    charspan_iter operator++(int);
    charspan_iter &operator++();
    charspan_iter &operator--();
    charspan_iter operator--(int);
    const icu::UnicodeString &string() const;
    const std::pair<int, int> &span() const;
    bool operator!=(const charspan_iter &other) const;
    bool operator==(const charspan_iter &other) const;
    charspan_iter begin();
    charspan_iter end();
    // checks (*this == end()), but without explicitly constructing
    // end(), so it's much faster
    bool at_end() const;
};
charspan_iter rev_charspan_iter(const icu::UnicodeString &s);

class char_iter
{
  private:
    charspan_iter it;
  public:
    char_iter(const icu::UnicodeString &s);
    char_iter(const charspan_iter &it);
    char_iter(const char_iter &cit);
    friend char_iter rev_char_iter(const icu::UnicodeString &s);
    
    char_iter &operator++();
    char_iter operator++(int);
    char_iter &operator--();
    char_iter operator--(int);
    char_iter begin();
    char_iter end();
    bool at_end();
    const icu::UnicodeString &string() const;
    inline icu::UnicodeString operator*() const { return string().tempSubStringBetween(it.span().first, it.span().second); }
    bool operator!=(const char_iter &other) const;
    bool operator==(const char_iter &other) const;
    std::pair<int, int> span() const;
};

char_iter rev_char_iter(const icu::UnicodeString &s);

UString to_ustring(const icu::UnicodeString &str);

#endif
