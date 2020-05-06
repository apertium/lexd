#include "lexicon.h"

Lexicon::Lexicon(vector<vector<pair<vector<int>, vector<int>>>> entries, bool shouldAlign)
  : entries(entries), hasMerged(false), hasSeparate(false), shouldAlign(shouldAlign)
{
  entryCount = entries.size();
  partCount = entries[0].size();
}

Lexicon::~Lexicon() {}

void
Lexicon::addEntries(vector<vector<pair<vector<int>, vector<int>>>> newEntries)
{
  entryCount += newEntries.size();
  entries.insert(entries.end(), newEntries.begin(), newEntries.end());
}

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

Transducer*
Lexicon::getTransducerWithFlags(Alphabet& alpha, Side side, int part, wstring flag)
{
  Transducer* t = new Transducer();
  for(unsigned int e = 0; e < entries.size(); e++)
  {
    int state = t->getInitial();
    if(flag.size() > 0)
    {
      wstring f = L"@U." + flag + L"." + to_wstring(e) + L"@";
      alpha.includeSymbol(f);
      int s = alpha(f);
      state = t->insertSingleTransduction(alpha(s, s), state);
    }
    pair<vector<int>, vector<int>>& ent = entries[e][part-1];
    unsigned int max = ent.first.size();
    if(side == SideRight || (side == SideBoth && ent.second.size() > max)) max = ent.second.size();
    for(unsigned int i = 0; i < max; i++)
    {
      int l = (side != SideRight && i < ent.first.size()) ? ent.first[i] : 0;
      int r = (side != SideLeft && i < ent.second.size()) ? ent.second[i] : 0;
      state = t->insertSingleTransduction(alpha(l, r), state);
    }
    t->setFinal(state);
  }
  t->minimize();
  return t;
}
