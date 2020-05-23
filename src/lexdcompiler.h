#ifndef __LEXDCOMPILER__
#define __LEXDCOMPILER__

#include "lexicon.h"

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

typedef pair<UnicodeString, unsigned int> token_t;
typedef pair<token_t, token_t> token_pair_t;
typedef vector<token_pair_t> pattern_t;
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
  map<UnicodeString, Transducer*> lexiconTransducers;
  map<token_pair_t, vector<Transducer*>> entryTransducers;

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

  void die(const wstring & msg);
  void finishLexicon();
  void checkName(UnicodeString& name);
  void processNextLine();

  map<UnicodeString, unsigned int> matchedParts;
  bool make_token(UnicodeString, token_pair_t &);
  void make_single_token(UnicodeString, token_t &);
  void insertEntry(Transducer* trans, vector<int>& left, vector<int>& right);
  Transducer* getLexiconTransducer(token_pair_t tok, unsigned int entry_index);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, unsigned int pos);
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
