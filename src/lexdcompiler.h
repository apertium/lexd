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
  u.insert(ys.begin(), ys.end());
  return u;
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
  for(auto y: ys)
    i.erase(y);
  return i;
}

struct lex_token_t;

struct token_t {
  string_ref name;
  unsigned int part;
  set<string_ref> tags, negtags;
  bool operator<(const token_t &t) const
  {
    return name < t.name || (name == t.name &&  part < t.part) || (name == t.name && part == t.part && tags < t.tags) || (name == t.name && part == t.part && tags == t.tags && negtags < t.negtags);
  }
  bool operator==(const token_t &t) const
  {
    return name == t.name && part == t.part && tags == t.tags;
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
  set<string_ref> tags;
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

enum FlagDiacriticType
{
  Unification,
  Positive,
  Negative,
  Require,
  Disallow,
  Clear
};

class LexdCompiler
{
private:
  bool shouldAlign;
  bool shouldCompress;
  bool tagsAsFlags;
  bool shouldHypermin;

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
  map<pattern_element_t, pair<int, int>> transducerLocs;

  UFILE* input;
  bool inLex;
  bool inPat;
  vector<entry_t> currentLexicon;
  set<string_ref> currentLexicon_tags_left, currentLexicon_tags_right;
  string_ref currentLexiconId;
  unsigned int currentLexiconPartCount;
  string_ref currentPatternId;
  line_number_t lineNumber;
  bool doneReading;
  unsigned int anonymousCount;

  Transducer* hyperminTrans;

  string_ref left_sieve_name;
  string_ref right_sieve_name;
  vector<pattern_element_t> left_sieve_tok;
  vector<pattern_element_t> right_sieve_tok;

  void die(const wstring & msg);
  void finishLexicon();
  string_ref internName(const UnicodeString& name);
  string_ref checkName(UnicodeString& name);
  RepeatMode readModifier(char_iter& iter);
  void readTags(char_iter& iter, UnicodeString& line, set<string_ref>* tags, set<string_ref>* negtags);
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
  Transducer* buildPatternWithFlags(const token_t &tok, int pattern_start_state);
  trans_sym_t alphabet_lookup(const UnicodeString &symbol);
  trans_sym_t alphabet_lookup(trans_sym_t l, trans_sym_t r);

  void encodeFlag(UnicodeString& str, int flag);
  trans_sym_t getFlag(FlagDiacriticType type, unsigned int flag, unsigned int value);
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
  void setShouldHypermin(bool val)
  {
    shouldHypermin = val;
  }
  Transducer* buildTransducer(bool usingFlags);
  void readFile(UFILE* infile);
  void printStatistics() const;
};

#endif
