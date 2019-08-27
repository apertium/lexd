#ifndef __LEXDLEXICON__
#define __LEXDLEXICON__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/transducer.h>

#include <vector>

using namespace std;

enum Side
{
  SideLeft,
  SideRight,
  SideBoth
};

class Lexicon
{
private:
  vector<vector<pair<vector<int>, vector<int>>>> entries;
  bool hasMerged;
  bool hasSeparate;
  vector<Transducer*> merged;
  vector<vector<pair<Transducer*, Transducer*>>> separate;
  bool shouldAlign;
  int entryCount;
  int partCount;

public:
  Lexicon(vector<vector<pair<vector<int>, vector<int>>>> entries, bool shouldAlign);
  ~Lexicon();
  Transducer* getTransducer(Alphabet& alpha, Side side, int part, int index);
  int getEntryCount()
  {
    return entryCount;
  }
  int getPartCount()
  {
    return partCount;
  }
};

#endif
