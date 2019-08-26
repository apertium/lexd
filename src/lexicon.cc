#include "lexicon.h"

Lexicon::Lexicon(vector<pair<vector<int>, vector<int>>> entries, bool shouldAlign)
  : entries(entries), hasMerged(false), hasSeparate(false), shouldAlign(shouldAlign)
  {}

Lexicon::~Lexicon() {}

const Transducer&
Lexicon::getMerged(const Alphabet& alpha)
{
  if(hasMerged) return merged;
  //if(shouldAlign) ...
  for(auto pr : entries)
  {
    int state = merged.getInitial();
    for(unsigned int i = 0; i < pr.first.size() || i < pr.second.size(); i++)
    {
      int l = (i < pr.first.size()) ? pr.first[i] : 0;
      int r = (i < pr.second.size()) ? pr.second[i] : 0;
      state = merged.insertSingleTransduction(alpha(l, r), state);
    }
    merged.setFinal(state);
  }
  hasMerged = true;
  return merged;
}

vector<pair<Transducer*, Transducer*>>
Lexicon::getSeparate(const Alphabet& alpha)
{
  if(hasSeparate) return separate;
  for(auto pr : entries)
  {
    Transducer* left = new Transducer();
    Transducer* right = new Transducer();
    int state = left->getInitial();
    for(auto l : pr.first)
    {
      state = left->insertSingleTransduction(alpha(l, 0), state);
    }
    left->setFinal(state);
    state = right->getInitial();
    for(auto r : pr.second)
    {
      state = right->insertSingleTransduction(alpha(0, r), state);
    }
    right->setFinal(state);
    separate.push_back(make_pair(left, right));
  }
  hasSeparate = true;
  return separate;
}
