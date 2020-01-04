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

class LexdCompiler
{
private:
  map<wstring, Lexicon*> lexicons;
  bool shouldAlign;
  bool usingFlags;
  // { name => [ ( line, [ ( lexicon, ( side, part ) ) ] ) ] }
  map<wstring, vector<pair<int, vector<pair<wstring, pair<Side, int>>>>>> patterns;
  map<wstring, Transducer*> patternTransducers;
  Alphabet alphabet;
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
  void processNextLine(FILE* input);
  map<wstring, int> matchedParts;
  void buildPattern(int state, Transducer* t, const vector<pair<wstring, pair<Side, int>>>& pat, unsigned int pos);
  Transducer* buildPattern(wstring name);
  Transducer* buildPatternWithFlags(wstring name);
public:
  LexdCompiler();
  ~LexdCompiler();
  void setShouldAlign(bool val)
  {
    shouldAlign = val;
  }
  void setUsingFlags(bool val)
  {
    usingFlags = val;
  }
  void process(const string& infile, const string& outfile);
};

#endif
