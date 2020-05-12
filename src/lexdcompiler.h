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

typedef pair<UnicodeString, pair<Side, unsigned int>> token_t;
typedef vector<token_t> pattern_t;

class LexdCompiler
{
private:
  bool shouldAlign;

  map<UnicodeString, Lexicon*> lexicons;
  // { name => [ ( line, [ ( lexicon, ( side, part ) ) ] ) ] }
  map<UnicodeString, vector<pair<int, pattern_t>>> patterns;
  map<UnicodeString, Transducer*> patternTransducers;

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
  bool make_token(UnicodeString, token_t &);
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
  Transducer* buildTransducer(bool usingFlags);
  void readFile(UFILE* infile);
};

#endif
