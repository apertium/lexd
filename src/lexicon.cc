#include "lexicon.h"

Lexicon::Lexicon(vector<vector<pair<vector<int>, vector<int>>>> entries, bool shouldAlign)
  : entries(entries), hasMerged(false), hasSeparate(false), shouldAlign(shouldAlign)
{
  entryCount = entries.size();
  partCount = entries[0].size();
}

Lexicon::~Lexicon() {}

Transducer*
Lexicon::getTransducer(Alphabet& alpha, Side side, int part, int index = 0)
{
  if(side == SideBoth)
  {
    if(hasMerged) return merged[part-1];
    // TODO: shouldAlign
    for(int prt = 0; prt < partCount; prt++)
    {
      Transducer* m = new Transducer();
      for(auto pr : entries)
      {
        int state = m->getInitial();
        for(unsigned int i = 0; i < pr[prt].first.size() || i < pr[prt].second.size(); i++)
        {
          int l = (i < pr[prt].first.size()) ? pr[prt].first[i] : 0;
          int r = (i < pr[prt].second.size()) ? pr[prt].second[i] : 0;
          state = m->insertSingleTransduction(alpha(l, r), state);
        }
        m->setFinal(state);
      }
      merged.push_back(m);
    }
    hasMerged = true;
    return merged[part-1];
  }
  else
  {
    if(!hasSeparate)
    {
      for(auto ent : entries)
      {
        vector<pair<Transducer*, Transducer*>> temp;
        for(auto pr : ent)
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
          temp.push_back(make_pair(left, right));
        }
        separate.push_back(temp);
      }
      hasSeparate = true;
    }
    if(side == SideLeft)
    {
      return separate[index][part-1].first;
    }
    else
    {
      return separate[index][part-1].second;
    }
  }
}
