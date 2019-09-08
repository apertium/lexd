#include "state.h"

State::State(vector<int> al, vector<Element*> left, Element* center, vector<Element*> right)
 : isFirst(true), alpha(al), stateId(-1)
{
  Element* cur = NULL;
  if(left.size() > 0)
  {
    side = -1;
    cur = left[0];
    left.erase(0);
  }
  else if(center != NULL)
  {
    side = 0;
    cur = center;
    center = NULL;
  }
  else if(right.size() > 0)
  {
    side = 1;
    cur = right[0];
    right.erase(0);
  }
  if(cur == NULL)
  {
    side = 2;
  }
  else if(cur->setId == -1)
  {
    State* nx = new State(alpha, left, center, right);
    nx->isFirst = false;
    next.push_back(make_pair(cur->symbols[0], nx));
  }
  else
  {
    int split = cur->setId;
    bool splitfound = false;
    unsigned int count = cur->symbols.size();
    for(auto it : left)
    {
      if(it->setId == split)
      {
        splitfound = true;
        break;
      }
    }
    for(auto it : right)
    {
      if(it->setId == split)
      {
        splitfound = true;
        break;
      }
    }
    if(center != NULL && center->setId == split) splitfound = true;
    if(splitfound && count > 1)
    {
      vector<vector<Element*>> leftops;
      vector<Element*> centerops = splitElement(center, split, count);
      vector<vector<Element*>> rightops;
      leftops.resize(cur->symbols.size());
      rightops.resize(cur->symbols.size());
      for(auto it : left)
      {
        vector<Element*> temp = splitElement(it, split, count);
        for(unsigned int i = 0; i < count; i++)
        {
          leftops[i].push_back(temp[i]);
        }
      }
      for(auto it : right)
      {
        vector<Element*> temp = splitElement(it, split, count);
        for(unsigned int i = 0; i < count; i++)
        {
          rightops[i].push_back(temp[i]);
        }
      }
      for(unsigned int i = 0; i < count; i++)
      {
        State* nx = new State(alpha, leftops[i], centerops[i], rightops[i]);
        nx->isFirst = false;
        next.push_back(make_pair(cur->symbols[i], nx));
      }
    }
    else
    {
      State* nx = new State(alpha, left, center, right);
      nx->isFirst = false;
      next.push_back(make_pair(cur->symbols[0], nx));
    }
  }
}

State::State(State* other)
{
  isFirst = other->isFirst;
  side = other->side;
  alpha = other->alpha;
  stateId = other->stateId;
  loopback = other->loopback;
  for(auto it : other->next)
  {
    State* nx = new State(it.second);
    next.push_back(make_pair(it.first, nx));
  }
}

State::~State()
{}

vector<Element*>
State::splitElement(Element* e, int set, unsigned int size)
{
  vector<Element*> ret;
  if(e == NULL || e->setId != set)
  {
    ret->resize(size, e);
  }
  else
  {
    if(e->symbols.size() != size)
    {
      wcerr << "Matched sets are different sizes" << endl;
      exit(1);
    }
    for(unsigned int i = 0; i < size; i++)
    {
      Element* n = new Element;
      n->setId = -1;
      n->symbols.push_back(e->symbols[i]);
      ret.push_back(n);
    }
  }
  return ret;
}
