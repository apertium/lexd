#include "icu-iter.h"
#include <iostream>
#include <string>
#include <cstdint>
#include <i18n.h>
using namespace std;
using namespace icu;

charspan_iter::charspan_iter(const UnicodeString &s)
  : _status(U_ZERO_ERROR), s(&s)
{
  it = BreakIterator::createCharacterInstance(Locale::getDefault(), _status);
  if(U_FAILURE(_status))
  {
    I18n(LEXD_I18N_DATA, "lexd").error("LEXD1000", {"code"}, {_status}, true);
  }
  it->setText(s);
  _span.first = it->first();
  _span.second = it->next();
}

charspan_iter::charspan_iter(const charspan_iter &other)
  : _status(other._status), s(other.s), _span(other._span)
{
  it = other.it->clone();
}

charspan_iter::~charspan_iter()
{
  delete it;
}

charspan_iter rev_charspan_iter(const UnicodeString &s)
{
  return --charspan_iter(s).end();
}

const UErrorCode &charspan_iter::status() const
{
    return _status;
}

const pair<int, int> &charspan_iter::operator*() const
{
  return _span;
}

charspan_iter charspan_iter::operator++(int)
{
  if (at_end()) return *this;
  auto other = charspan_iter(*this);
  other._span = make_pair(other._span.second, other.it->next());
  if(other._span.first == other._span.second)
    other._span.second = other.it->next();
  return other;
}

charspan_iter &charspan_iter::operator++()
{
  if (!at_end())
  {
    _span = make_pair(_span.second, it->next());
    if(_span.first == _span.second)
      _span.second = it->next();
  }
  return *this;
}

charspan_iter &charspan_iter::operator--()
{
  if(*this != begin())
  {
    _span = make_pair(it->previous(), _span.first);
    if(_span.first == _span.second)
      _span.first = it->previous();
  }
  return *this;
}

charspan_iter charspan_iter::operator--(int)
{
  if(*this == begin())
    return *this;
  auto other = charspan_iter(*this);
  other._span = make_pair(other.it->previous(), other._span.first);
  if(other._span.first == other._span.second)
    other._span.first = other.it->previous();
  return other;
}

const UnicodeString &charspan_iter::string() const
{
  return *s;
}

const pair<int, int> &charspan_iter::span() const
{
  return _span;
}

bool charspan_iter::operator!=(const charspan_iter &other) const
{
  return s != other.s || _span != other._span;
}

bool charspan_iter::operator==(const charspan_iter &other) const
{
  return s == other.s && _span == other._span;
}

charspan_iter charspan_iter::begin()
{
  return charspan_iter(*s);
}

charspan_iter charspan_iter::end()
{
  charspan_iter cs_it(*s);
  cs_it._span.first = cs_it.it->last();
  cs_it._span.second = BreakIterator::DONE;
  return cs_it;
}

bool charspan_iter::at_end() const
{
  return _span.second == BreakIterator::DONE;
}


char_iter::char_iter(const UnicodeString &s) : it(s)
{
}
char_iter::char_iter(const charspan_iter &it) : it(it)
{
}
char_iter::char_iter(const char_iter &cit) : it(cit.it)
{
}

char_iter rev_char_iter(const UnicodeString &s) {
  return char_iter(rev_charspan_iter(s));
}

char_iter &char_iter::operator++() {
  ++it;
  return *this;
}
char_iter char_iter::operator++(int) {
  char_iter it(*this);
  ++this->it;
  return it;
}
char_iter &char_iter::operator--() {
  --it;
  return *this;
}
char_iter char_iter::operator--(int) {
  char_iter it = *this;
  --this->it;
  return it;
}
char_iter char_iter::begin()
{
  return char_iter(string());
}
char_iter char_iter::end()
{
  return char_iter(it.end());
}
bool char_iter::at_end()
{
  return it.at_end();
}
const UnicodeString &char_iter::string() const
{
  return it.string();
}
bool char_iter::operator!=(const char_iter &other) const
{
    return it != other.it;
}
bool char_iter::operator==(const char_iter &other) const
{
    return it == other.it;
}
pair<int, int> char_iter::span() const
{
  return it.span();
}

UString to_ustring(const UnicodeString &str)
{
  UString temp;
  temp.append(str.getBuffer(), (unsigned int)str.length());
  return temp;
}
