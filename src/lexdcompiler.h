#ifndef __LEXDCOMPILER__
#define __LEXDCOMPILER__

#include "icu-iter.h"

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <unicode/ustdio.h>
#include <unicode/unistr.h>

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>

using namespace std;
using namespace icu;

struct string_ref {
  unsigned int i;
  string_ref() : i(0) {}
  explicit string_ref(unsigned int _i) : i(_i) {}
  explicit operator unsigned int() const { return i; }
  bool operator == (string_ref other) const { return i == other.i; }
  bool operator != (string_ref other) const { return !(*this == other); }
  bool operator < (string_ref other) const { return i < other.i; }
  bool operator !() const { return empty(); }
  string_ref operator || (string_ref other) const {
    return i ? *this : other;
  }
  bool empty() const { return i == 0; }
  bool valid() const { return i != 0; }
};

template<>
struct std::hash<string_ref> {
  size_t operator()(const string_ref &t) const
  {
    return std::hash<unsigned int>()(t.i);
  }
};

template<typename T>
bool subset(const set<T> &xs, const set<T> &ys)
{
  if(xs.size() > ys.size())
    return false;
  for(auto x: xs)
    if(ys.find(x) == ys.end())
      return false;
  return true;
}

template<typename T>
bool subset_strict(const set<T> &xs, const set<T> &ys)
{
  if(xs.size() >= ys.size())
    return false;
  return subset(xs, ys);
}

template<typename T>
set<T> unionset(const set<T> &xs, const set<T> &ys)
{
  set<T> u = xs;
  unionset_inplace(xs, ys);
  return u;
}

template<typename T>
void unionset_inplace(set<T> &xs, const set<T> &ys)
{
  xs.insert(ys.begin(), ys.end());
  return;
}

template<typename T>
set<T> intersectset(const set<T> &xs, const set<T> &ys)
{
  set<T> i = xs;
  for(auto x: xs)
    if(ys.find(x) == ys.end())
      i.erase(x);
  return i;
}

template<typename T>
set<T> subtractset(const set<T> &xs, const set<T> &ys)
{
  set<T> i = xs;
  subtractset_inplace(i, ys);
  return i;
}

template<typename T>
void subtractset_inplace(set<T> &xs, const set<T> &ys)
{
  for(const auto &y: ys)
    xs.erase(y);
}

struct lex_token_t;

class tags_t : public set<string_ref>
{
  using set<string_ref>::set;
  public:
  tags_t(const set<string_ref> &s) : set(s) { }
};
class pos_tag_filter_t : public set<string_ref>
{
  using set<string_ref>::set;
};
class neg_tag_filter_t : public set<string_ref>
{
  using set<string_ref>::set;
};

struct tag_filter_t {
  tag_filter_t() = default;
  tag_filter_t(const pos_tag_filter_t &pos) : _pos(pos) {}
  tag_filter_t(const neg_tag_filter_t &neg) : _neg(neg) {}
  bool operator<(const tag_filter_t &t) const
  {
    return _pos < t._pos || (_pos == t._pos && _neg < t._neg);
  }
  bool operator==(const tag_filter_t &t) const
  {
    return _pos == t._pos && _neg == t._neg;
  }
  bool compatible(const tags_t &tags) const;
  bool combinable(const tag_filter_t &other) const;
  bool applicable(const tags_t &tags) const;
  bool try_apply(tags_t &tags) const;
  const pos_tag_filter_t &pos() const { return _pos; }
  const neg_tag_filter_t &neg() const { return _neg; }

  bool combine(const tag_filter_t &other);

  private:
  pos_tag_filter_t _pos;
  neg_tag_filter_t _neg;
};

struct token_t {
  string_ref name;
  unsigned int part;
  tag_filter_t tag_filter;
  bool operator<(const token_t &t) const
  {
    return name < t.name || (name == t.name &&  part < t.part) || (name == t.name && part == t.part && tag_filter < t.tag_filter);
  }
  bool operator==(const token_t &t) const
  {
    return name == t.name && part == t.part && tag_filter == t.tag_filter;
  }
  bool compatible(const lex_token_t &tok) const;
};

struct trans_sym_t {
  int i;
  trans_sym_t() : i(0) {}
  explicit trans_sym_t(int _i) : i(_i) {}
  explicit operator int() const { return i; }
  bool operator == (trans_sym_t other) const { return i == other.i; }
  bool operator < (trans_sym_t other) const { return i < other.i; }
  trans_sym_t operator || (trans_sym_t other) const {
    return i ? *this : other;
  }
};

struct lex_token_t {
  vector<trans_sym_t> symbols;
  tags_t tags;
  bool operator ==(const lex_token_t &other) const { return symbols == other.symbols && tags == other.tags; }
};

struct lex_seg_t {
  lex_token_t left, right;
  bool operator == (const lex_seg_t &t) const
  {
    return left == t.left && right == t.right;
  }
};

enum RepeatMode
{
  Optional = 1,
  Repeated = 2,

  Normal = 0,
  Question = 1,
  Plus = 2,
  Star = 3
};

struct pattern_element_t {
  token_t left, right;
  RepeatMode mode;

  bool operator<(const pattern_element_t& o) const
  {
    return left < o.left || (left == o.left && right < o.right) || (left == o.left && right == o.right && mode < o.mode);
  }

  bool operator==(const pattern_element_t& o) const
  {
    return left == o.left && right == o.right && mode == o.mode;
  }
};

typedef vector<pattern_element_t> pattern_t;
typedef vector<lex_seg_t> entry_t;
typedef int line_number_t;

class LexdCompiler
{
private:
  bool shouldAlign;
  bool shouldCompress;
  bool tagsAsFlags;

  map<UnicodeString, string_ref> name_to_id;
  vector<UnicodeString> id_to_name;

  const UnicodeString &name(string_ref r) const;

  map<string_ref, vector<entry_t>> lexicons;
  // { id => [ ( line, [ pattern ] ) ] }
  map<string_ref, vector<pair<line_number_t, pattern_t>>> patterns;
  map<token_t, Transducer*> patternTransducers;
  map<pattern_element_t, Transducer*> lexiconTransducers;
  map<pattern_element_t, vector<Transducer*>> entryTransducers;
  map<string_ref, set<string_ref>> flagsUsed;

  UFILE* input;
  bool inLex;
  bool inPat;
  vector<entry_t> currentLexicon;
  tags_t currentLexicon_tags_left, currentLexicon_tags_right;
  string_ref currentLexiconId;
  unsigned int currentLexiconPartCount;
  string_ref currentPatternId;
  line_number_t lineNumber;
  bool doneReading;
  unsigned int anonymousCount;

  string_ref left_sieve_name;
  string_ref right_sieve_name;
  vector<pattern_element_t> left_sieve_tok;
  vector<pattern_element_t> right_sieve_tok;

  void die(const wstring & msg);
  void finishLexicon();
  string_ref internName(const UnicodeString& name);
  string_ref checkName(UnicodeString& name);
  RepeatMode readModifier(char_iter& iter);
  tag_filter_t readTagFilter(char_iter& iter, UnicodeString& line);
  tags_t readTags(char_iter& iter, UnicodeString& line);
  lex_seg_t processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count);
  token_t readToken(char_iter& iter, UnicodeString& line);
  pattern_element_t readPatternElement(char_iter& iter, UnicodeString& line);
  void processPattern(char_iter& iter, UnicodeString& line);
  void processNextLine();

  bool isLexiconToken(const pattern_element_t& tok);
  vector<int> determineFreedom(pattern_t& pat);
  map<string_ref, unsigned int> matchedParts;
  void applyMode(Transducer* trans, RepeatMode mode);
  void insertEntry(Transducer* trans, const lex_seg_t &seg);
  void appendLexicon(string_ref lexicon_id, const vector<entry_t> &to_append);
  Transducer* getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, vector<int> is_free, unsigned int pos);
  Transducer* buildPattern(const token_t &tok);
  Transducer* buildPatternWithFlags(const token_t &tok);
  trans_sym_t alphabet_lookup(const UnicodeString &symbol);
  trans_sym_t alphabet_lookup(trans_sym_t l, trans_sym_t r);

  void encodeFlag(UnicodeString& str, int flag);
  trans_sym_t getFlagSymbol(string_ref lexicon, unsigned int entry_index);
  vector<trans_sym_t> getEntryFlags(lex_token_t& tok);
  vector<trans_sym_t> getSelectorFlags(token_t& tok);
  Transducer* getLexiconTransducerWithFlags(pattern_element_t& tok, bool free);

public:
  LexdCompiler();
  ~LexdCompiler();
  Alphabet alphabet;
  void setShouldAlign(bool val)
  {
    shouldAlign = val;
  }
  void setShouldCompress(bool val)
  {
    shouldCompress = val;
  }
  Transducer* buildTransducer(bool usingFlags);
  void readFile(UFILE* infile);
  void printStatistics() const;
};

#endif
