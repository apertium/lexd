#ifndef __LEXDCOMPILER__
#define __LEXDCOMPILER__

#include "lexicon.h"

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

typedef pair<wstring, pair<Side, int>> token_t;
typedef vector<token_t> pattern_t;

class LexdCompiler
{
private:
  bool shouldAlign;

  map<wstring, Lexicon*> lexicons;
  // { name => [ ( line, [ ( lexicon, ( side, part ) ) ] ) ] }
  map<wstring, vector<pair<int, pattern_t>>> patterns;
  map<wstring, Transducer*> patternTransducers;

  FILE* input;
  bool inLex;
  bool inPat;
  vector<vector<pair<vector<int>, vector<int>>>> currentLexicon;
  wstring currentLexiconName;
  unsigned int currentLexiconPartCount;
  wstring currentPatternName;
  int lineNumber;
  bool doneReading;
  int flagsUsed;

  void die(wstring msg);
  void finishLexicon();
  void checkName(wstring& name);
  void processNextLine();

  map<wstring, int> matchedParts;
  bool make_token(wstring, token_t &);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, unsigned int pos);
  Transducer* buildPattern(wstring name);
  Transducer* buildPatternWithFlags(wstring name);
public:
  LexdCompiler();
  ~LexdCompiler();
  Alphabet alphabet;
  void setShouldAlign(bool val)
  {
    shouldAlign = val;
  }
  Transducer* buildTransducer(bool usingFlags);
  void readFile(FILE* infile);
};

#endif
