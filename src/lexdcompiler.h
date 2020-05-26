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

enum RepeatMode
{
  Optional = 1,
  Repeated = 2,

  Normal = 0,
  Question = 1,
  Plus = 2,
  Star = 3
};

typedef pair<UnicodeString, unsigned int> token_t;
typedef pair<token_t, token_t> token_pair_t;
typedef pair<token_pair_t, RepeatMode> pattern_element_t;
typedef vector<pattern_element_t> pattern_t;
typedef vector<pair<vector<int>, vector<int>>> entry_t;

class LexdCompiler
{
private:
  bool shouldAlign;
  bool shouldCompress;

  map<UnicodeString, vector<entry_t>> lexicons;
  // { name => [ ( line, [ ( lexicon, ( side, part ) ) ] ) ] }
  map<UnicodeString, vector<pair<int, pattern_t>>> patterns;
  map<UnicodeString, Transducer*> patternTransducers;
  map<pattern_element_t, Transducer*> lexiconTransducers;
  map<pattern_element_t, vector<Transducer*>> entryTransducers;

  UFILE* input;
  bool inLex;
  bool inPat;
  vector<vector<pair<vector<int>, vector<int>>>> currentLexicon;
  UnicodeString currentLexiconName;
  unsigned int currentLexiconPartCount;
  UnicodeString currentPatternName;
  int lineNumber;
  bool doneReading;
  unsigned int flagsUsed;
  unsigned int anonymousCount;

  void die(const wstring & msg);
  void finishLexicon();
  void checkName(UnicodeString& name);
  RepeatMode readModifier(char_iter& iter);
  pair<vector<int>, vector<int>> processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count);
  token_t readToken(char_iter& iter, UnicodeString& line);
  void processPattern(char_iter& iter, UnicodeString& line);
  void processNextLine();

  map<UnicodeString, unsigned int> matchedParts;
  void insertEntry(Transducer* trans, vector<int>& left, vector<int>& right);
  Transducer* getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, vector<int> is_free, unsigned int pos);
  Transducer* buildPattern(UnicodeString name);
  Transducer* buildPatternWithFlags(UnicodeString name);
  int alphabet_lookup(const UnicodeString &symbol);
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
