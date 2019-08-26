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
  enum Side
  {
    SideLeft,
    SideRight,
    SideBoth
  };
  map<wstring, Lexicon*> lexicons;
  bool shouldAlign;
  vector<pair<int, vector<pair<wstring, Side>>>> patterns;
  Alphabet alphabet;
  Transducer transducer;
  bool inLex;
  bool inPat;
  vector<pair<vector<int>, vector<int>>> currentLexicon;
  wstring currentLexiconName;
  int lineNumber;
  void die(wstring msg);
  void processNextLine();
  map<wstring, Transducer*> matchedParts;
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
