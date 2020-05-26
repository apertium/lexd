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

struct pattern_element_t {
  unsigned int lname;
  unsigned int rname;
  unsigned int lpart;
  unsigned int rpart;
  RepeatMode mode;

  bool operator<(const pattern_element_t& o) const
  {
    if(lname != o.lname) return lname < o.lname;
    if(rname != o.rname) return rname < o.rname;
    if(lpart != o.lpart) return lpart < o.lpart;
    if(rpart != o.rpart) return rpart < o.rpart;
    return mode < o.mode;
  }

  bool operator==(const pattern_element_t& o) const
  {
    return (lname == o.lname) && (rname == o.rname) &&
           (lpart == o.lpart) && (rpart == o.rpart) &&
           (mode == o.mode);
  }
};

typedef vector<pattern_element_t> pattern_t;
typedef vector<pair<vector<int>, vector<int>>> entry_t;

class LexdCompiler
{
private:
  bool shouldAlign;
  bool shouldCompress;

  map<UnicodeString, unsigned int> name_to_id;
  vector<UnicodeString> id_to_name;

  map<unsigned int, vector<entry_t>> lexicons;
  // { id => [ ( line, [ pattern ] ) ] }
  map<unsigned int, vector<pair<int, pattern_t>>> patterns;
  map<unsigned int, Transducer*> patternTransducers;
  map<pattern_element_t, Transducer*> lexiconTransducers;
  map<pattern_element_t, vector<Transducer*>> entryTransducers;

  UFILE* input;
  bool inLex;
  bool inPat;
  vector<vector<pair<vector<int>, vector<int>>>> currentLexicon;
  unsigned int currentLexiconId;
  unsigned int currentLexiconPartCount;
  unsigned int currentPatternId;
  int lineNumber;
  bool doneReading;
  unsigned int flagsUsed;
  unsigned int anonymousCount;

  void die(const wstring & msg);
  void finishLexicon();
  unsigned int internName(UnicodeString& name);
  unsigned int checkName(UnicodeString& name);
  RepeatMode readModifier(char_iter& iter);
  pair<vector<int>, vector<int>> processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count);
  pair<unsigned int, unsigned int> readToken(char_iter& iter, UnicodeString& line);
  void processPattern(char_iter& iter, UnicodeString& line);
  void processNextLine();

  map<unsigned int, unsigned int> matchedParts;
  void insertEntry(Transducer* trans, vector<int>& left, vector<int>& right);
  Transducer* getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free);
  void buildPattern(int state, Transducer* t, const pattern_t& pat, vector<int> is_free, unsigned int pos);
  Transducer* buildPattern(unsigned int name);
  Transducer* buildPatternWithFlags(unsigned int name);
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
