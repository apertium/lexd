#ifndef __LEXDLEXICON__
#define __LEXDLEXICON__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/transducer.h>

#include <vector>

using namespace std;

class Lexicon
{
private:
  vector<pair<vector<int>, vector<int>>> entries;
  bool hasMerged;
  bool hasSeparate;
  Transducer merged;
  vector<pair<Transducer*, Transducer*>> separate;
  bool shouldAlign;

public:
  Lexicon(vector<pair<vector<int>, vector<int>>> entries, bool shouldAlign);
  ~Lexicon();
  Transducer& getMerged(Alphabet& alpha);
  vector<pair<Transducer*, Transducer*>> getSeparate(Alphabet& alpha);
};

#endif
