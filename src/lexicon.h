#ifndef __LEXDLEXICON__
#define __LEXDLEXICON__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/transducer.h>

#include <vector>

using namespace std;

enum Side
{
  SideLeft = 0,
  SideRight = 1,
  SideBoth = 2
};

class Lexicon
{
private:
  vector<vector<pair<vector<int>, vector<int>>>> entries;
  bool hasMerged;
  bool hasSeparate;
  Transducer* merged;
  // index - part - side
  vector<vector<vector<Transducer*>>> separate;
  bool shouldAlign;
  unsigned int entryCount;
  unsigned int partCount;

public:
  Lexicon(vector<vector<pair<vector<int>, vector<int>>>> entries, bool shouldAlign);
  ~Lexicon();
  void addEntries(vector<vector<pair<vector<int>, vector<int>>>> newEntries);
  Transducer* getTransducer(Alphabet& alpha, Side side, unsigned int part, unsigned int index);
  Transducer* getTransducerWithFlags(Alphabet& alpha, Side side, unsigned int part, wstring flag);
  unsigned int getEntryCount()
  {
    return entryCount;
  }
  unsigned int getPartCount()
  {
    return partCount;
  }
};

#endif
