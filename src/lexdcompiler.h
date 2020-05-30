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
struct token_t {
	string_ref name;
	unsigned int part;
	bool operator<(const token_t &t) const
	{
	  return name < t.name || (name == t.name &&  part < t.part);
	}
	bool operator==(const token_t &t) const
	{
	  return name == t.name && part == t.part;
	}
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

struct lex_seg_t {
        vector<trans_sym_t> left, right;
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

class LexdCompiler
{
private:
  bool shouldAlign;
  bool shouldCompress;

  map<UnicodeString, string_ref> name_to_id;
  vector<UnicodeString> id_to_name;

  const UnicodeString &name(string_ref r) const;

  map<string_ref, vector<entry_t>> lexicons;
  // { id => [ ( line, [ pattern ] ) ] }
  map<string_ref, vector<pair<int, pattern_t>>> patterns;
  map<string_ref, Transducer*> patternTransducers;
  map<pattern_element_t, Transducer*> lexiconTransducers;
  map<pattern_element_t, vector<Transducer*>> entryTransducers;

  UFILE* input;
  bool inLex;
  bool inPat;
  vector<entry_t> currentLexicon;
  string_ref currentLexiconId;
  unsigned int currentLexiconPartCount;
  string_ref currentPatternId;
  int lineNumber;
  bool doneReading;
  unsigned int flagsUsed;
  unsigned int anonymousCount;

  void die(const wstring & msg);
  void finishLexicon();
  string_ref internName(UnicodeString& name);
  string_ref checkName(UnicodeString& name);
  RepeatMode readModifier(char_iter& iter);
  lex_seg_t processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count);
  token_t readToken(char_iter& iter, UnicodeString& line);
  void processPattern(char_iter& iter, UnicodeString& line);
  void processNextLine();

  map<string_ref, unsigned int> matchedParts;
  void insertEntry(Transducer* trans, const lex_seg_t &seg);
  Transducer* getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, vector<int> is_free, unsigned int pos);
  Transducer* buildPattern(string_ref name);
  Transducer* buildPatternWithFlags(string_ref name);
  trans_sym_t alphabet_lookup(const UnicodeString &symbol);
  trans_sym_t alphabet_lookup(trans_sym_t l, trans_sym_t r);
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
};

#endif
