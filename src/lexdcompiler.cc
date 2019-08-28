#include "lexdcompiler.h"

LexdCompiler::LexdCompiler()
  : shouldAlign(false), inLex(false), inPat(false), lineNumber(0), doneReading(false)
  {}

LexdCompiler::~LexdCompiler()
{}

void
LexdCompiler::die(wstring msg)
{
  wcerr << L"Error on line " << lineNumber << ": " << msg << endl;
  exit(EXIT_FAILURE);
}

void
LexdCompiler::processNextLine(FILE* input)
{
  wstring line;
  wchar_t c;
  bool escape = false;
  bool comment = false;
  while((c = fgetwc(input)) != L'\n')
  {
    if(c < 0 || feof(input))
    {
      doneReading = true;
      break;
    }
    if(comment) continue;
    if(escape)
    {
      line += c;
      escape = false;
    }
    else if(c == L'\\')
    {
      escape = true;
      line += c;
    }
    else if(c == L'#')
    {
      comment = true;
    }
    else if(isspace(c))
    {
      if(line.size() > 0 && line.back() != L' ')
      {
        line += L' ';
      }
    }
    else line += c;
  }
  lineNumber++;
  if(escape) die(L"Trailing backslash");
  if(line.size() == 0) return;

  if(line == L"PATTERNS" || line == L"PATTERNS ")
  {
    if(inLex)
    {
      Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
      lexicons[currentLexiconName] = lex;
      currentLexicon.clear();
    }
    currentLexiconName.clear();
    inLex = false;
    inPat = true;
  }
  else if(line.size() > 7 && line.substr(0, 8) == L"LEXICON ")
  {
    wstring name = line.substr(8);
    if(name.size() == 0) die(L"Unnamed lexicon");
    if(name.back() == L' ') name.pop_back();
    if(name.find(L" ") != wstring::npos) die(L"Lexicon names cannot contain spaces");
    if(name.find(L":") != wstring::npos) die(L"Lexicon names cannot contain colons");
    if(inLex)
    {
      Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
      lexicons[currentLexiconName] = lex;
      currentLexicon.clear();
    }
    currentLexiconPartCount = 1;
    if(name.size() > 1 && name.back() == L')')
    {
      wstring num;
      for(unsigned int i = name.size()-2; i > 0; i--)
      {
        if(isdigit(name[i])) num = name[i] + num;
        else if(name[i] == L'(' && num.size() > 0)
        {
          currentLexiconPartCount = stoi(num);
          name = name.substr(0, i);
        }
        else break;
      }
      if(name.size() == 0) die(L"Unnamed lexicon");
    }
    currentLexiconName = name;
    inLex = true;
    inPat = false;
  }
  else if(inPat)
  {
    // TODO: this should do some error checking (mismatches, mostly)
    if(line.back() != L' ') line += L' ';
    wstring cur;
    vector<pair<wstring, pair<Side, int>>> pat;
    for(auto ch : line)
    {
      if(ch == L' ')
      {
        if(cur.size() == 0) continue;
        int idx = 1;
        Side s = SideBoth;
        if(cur.front() == L':')
        {
          cur = cur.substr(1);
          s = SideRight;
        }
        else if(cur.back() == L':')
        {
          cur = cur.substr(0, cur.size()-1);
          s = SideLeft;
        }
        if(cur.size() == 0) die(L"Syntax error - colon without lexicon name");
        if(cur.size() > 1 && cur.back() == L')')
        {
          wstring temp;
          for(unsigned int i = cur.size()-2; i > 0; i--)
          {
            if(isdigit(cur[i])) temp = cur[i] + temp;
            else if(cur[i] == L'(' && temp.size() > 0)
            {
              idx = stoi(temp);
              cur = cur.substr(0, i);
              break;
            }
            else break;
          }
        }
        if(cur.size() == 0) die(L"Syntax error - no lexicon name");
        pat.push_back(make_pair(cur, make_pair(s, idx)));
        cur.clear();
      }
      else cur += ch;
    }
    patterns.push_back(make_pair(lineNumber, pat));
  }
  else if(inLex)
  {
    vector<wstring> pieces;
    wstring cur;
    for(unsigned int i = 0; i < line.size(); i++)
    {
      if(line[i] == L'\\')
      {
        cur += line.substr(i, 2);
        i++;
      }
      else if(line[i] == L' ')
      {
        pieces.push_back(cur + L":");
        cur.clear();
      }
      else cur += line[i];
    }
    if(cur.size() > 0) pieces.push_back(cur + L":");
    vector<pair<vector<int>, vector<int>>> entry;
    for(auto ln : pieces)
    {
      vector<vector<int>> parts;
      vector<int> cur;
      for(unsigned int i = 0; i < ln.size(); i++)
      {
        if(ln[i] == L'\\') cur.push_back((int)ln[++i]);
        else if(ln[i] == L':')
        {
          parts.push_back(cur);
          cur.clear();
        }
        else if(ln[i] == L'{' || ln[i] == L'<')
        {
          wchar_t end = (ln[i] == L'{') ? L'}' : L'>';
          for(unsigned int j = i+1; j < ln.size(); j++)
          {
            if(ln[j] == end)
            {
              wstring tag = ln.substr(i, j-i+1);
              alphabet.includeSymbol(tag);
              cur.push_back(alphabet(tag));
              i = j;
              break;
            }
          }
          if(ln[i] != end) cur.push_back((int)ln[i]);
        }
        else cur.push_back((int)ln[i]);
      }
      if(parts.size() == 1)
      {
        entry.push_back(make_pair(parts[0], parts[0]));
      }
      else if(parts.size() == 2)
      {
        entry.push_back(make_pair(parts[0], parts[1]));
      }
      else die(L"Lexicon entry contains multiple colons");
    }
    if(entry.size() != currentLexiconPartCount)
    {
      die(L"Lexicon entry has wrong number of components. Expected " + to_wstring(currentLexiconPartCount) + L", got " + to_wstring(entry.size()));
    }
    currentLexicon.push_back(entry);
  }
  else die(L"Expected 'PATTERNS' or 'LEXICON'");
}

void
LexdCompiler::buildPattern(int state, unsigned int pat, unsigned int pos)
{
  if(pos == patterns[pat].second.size())
  {
    transducer.setFinal(state);
    return;
  }
  int line = patterns[pat].first;
  wstring lex = patterns[pat].second[pos].first;
  Side side = patterns[pat].second[pos].second.first;
  int part = patterns[pat].second[pos].second.second;
  lineNumber = line;
  if(lexicons.find(lex) == lexicons.end()) die(L"Lexicon '" + lex + L"' is not defined");
  Lexicon* l = lexicons[lex];
  if(side == SideBoth && l->getPartCount() == 1)
  {
    int new_state = transducer.insertTransducer(state, *(l->getTransducer(alphabet, side, part, 0)));
    buildPattern(new_state, pat, pos+1);
  }
  else if(matchedParts.find(lex) == matchedParts.end())
  {
    for(int index = 0, max = l->getEntryCount(); index < max; index++)
    {
      int new_state = transducer.insertTransducer(state, *(l->getTransducer(alphabet, side, part, index)));
      matchedParts[lex] = index;
      buildPattern(new_state, pat, pos+1);
    }
    matchedParts.erase(lex);
  }
  else
  {
    int new_state = transducer.insertTransducer(state, *(l->getTransducer(alphabet, side, part, matchedParts[lex])));
    buildPattern(new_state, pat, pos+1);
  }
}

void
LexdCompiler::process(const string& infile, const string& outfile)
{
  FILE* input = fopen(infile.c_str(), "r");
  if(input == NULL)
  {
    wcerr << L"Cannot open file " << outfile.c_str() << " for reading." << endl;
    exit(EXIT_FAILURE);
  }
  while(!feof(input))
  {
    processNextLine(input);
    if(doneReading) break;
  }
  if(inLex)
  {
    Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
    lexicons[currentLexiconName] = lex;
    currentLexicon.clear();
  }
  for(unsigned int i = 0; i < patterns.size(); i++)
  {
    matchedParts.clear();
    buildPattern(transducer.getInitial(), i, 0);
  }
  transducer.minimize();
  FILE* output = fopen(outfile.c_str(), "w");
  if(output == NULL)
  {
    wcerr << L"Cannot open file " << outfile.c_str() << " for writing." << endl;
    exit(EXIT_FAILURE);
  }
  for(auto& it : transducer.getTransitions())
  {
    for(auto& it2 : it.second)
    {
      auto t = alphabet.decode(it2.first);
      fwprintf(output, L"%d\t", it.first);
      fwprintf(output, L"%d\t", it2.second.first);
      wstring l = L"";
      alphabet.getSymbol(l, t.first);
      if(l == L"")  // If we find an epsilon
      {
        fwprintf(output, L"@0@\t");
      }
      else if(l == L" ")
      {
        fwprintf(output, L"@_SPACE_@\t");
      }
      else
      {
        fwprintf(output, L"%S\t", l.c_str());
      }
      wstring r = L"";
      alphabet.getSymbol(r, t.second);
      if(r == L"")  // If we find an epsilon
      {
        fwprintf(output, L"@0@\t");
      }
      else if(r == L" ")
      {
        fwprintf(output, L"@_SPACE_@\t");
      }
      else
      {
        fwprintf(output, L"%S\t", r.c_str());
      }
      fwprintf(output, L"%f\t", it2.second.second);
      fwprintf(output, L"\n");
    }
  }

  for(auto& it3 : transducer.getFinals())
  {
    fwprintf(output, L"%d\t", it3.first);
    fwprintf(output, L"%f\n", it3.second);
  }
  fclose(output);
}
