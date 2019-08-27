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
  vector<pair<int, vector<pair<wstring, pair<Side, int>>>>> patterns;
  Alphabet alphabet;
  Transducer transducer;
  bool inLex;
  bool inPat;
  vector<vector<pair<vector<int>, vector<int>>>> currentLexicon;
  wstring currentLexiconName;
  unsigned int currentLexiconPartCount;
  int lineNumber;
  bool doneReading;
  void die(wstring msg);
  void processNextLine(FILE* input);
  map<wstring, int> matchedParts;
  void buildPattern(int state, unsigned int pat, unsigned int pos);
public:
  LexdCompiler();
  ~LexdCompiler();
  void setShouldAlign(bool val)
  {
    shouldAlign = val;
  }
  void process(const string& infile, const string& outfile);
};

#endif
