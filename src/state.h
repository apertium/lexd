#ifndef __LEXDALPHA__
#define __LEXDALPHA__

#include <lttoolbox/alphabet.h>
#include <lttoolbox/transducer.h>

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

struct Element
{
  vector<vector<int>> symbols;
  int setId; // -1 = unmatched
}

class State
{
private:
  bool isFirst;
  int side; // -1 = left, 0 = center, 1 = right, 2 = end
  vector<int> alpha;
  vector<pair<vector<int>, State*>> next;
  vector<pair<vector<int>, State*>> loopback;
  int stateId;
  void addStatesToTransducer(Transducer* t, int src);
  void addToTransducer(Transducer* t);
  vector<Element*> splitElement(Element* e, int set, unsigned int size);
public:
  State(vector<int> alpha, vector<Element*> left, Element* center, vector<Element*> right);
  State(State* other); // split
  void addLoopback();
  Transducer* transducerize();
  ~State();
};

#endif
