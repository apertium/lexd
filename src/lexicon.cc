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
  if(side == SideBoth && partCount == 1)
  {
    if(hasMerged) return merged;
    // TODO: shouldAlign
    merged = new Transducer();
    for(auto pr : entries)
    {
      int state = merged->getInitial();
      for(unsigned int i = 0; i < pr[0].first.size() || i < pr[0].second.size(); i++)
      {
        int l = (i < pr[0].first.size()) ? pr[0].first[i] : 0;
        int r = (i < pr[0].second.size()) ? pr[0].second[i] : 0;
        state = merged->insertSingleTransduction(alpha(l, r), state);
      }
      merged->setFinal(state);
    }
    merged->minimize();
    hasMerged = true;
    return merged;
  }
  else
  {
    if(!hasSeparate)
    {
      for(auto ent : entries)
      {
        vector<vector<Transducer*>> temp;
        for(auto pr : ent)
        {
          Transducer* left = new Transducer();
          Transducer* right = new Transducer();
          Transducer* merge = new Transducer();
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
          state = merge->getInitial();
          // TODO: should align
          for(unsigned int i = 0; i < pr.first.size() || i < pr.second.size(); i++)
          {
            int l = (i < pr.first.size()) ? pr.first[i] : 0;
            int r = (i < pr.second.size()) ? pr.second[i] : 0;
            state = merge->insertSingleTransduction(alpha(l, r), state);
          }
          merge->setFinal(state);
          vector<Transducer*> temp2;
          temp2.push_back(left);
          temp2.push_back(right);
          temp2.push_back(merge);
          temp.push_back(temp2);
        }
        separate.push_back(temp);
      }
      hasSeparate = true;
    }
    return separate[index][part-1][side];
  }
}
