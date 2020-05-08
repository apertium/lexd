#include "lexdcompiler.h"
#include <variant>

typedef vector<token_t> pattern_t;
typedef monostate none_t;
typedef variant<none_t, token_t> maybe_token_t;
const maybe_token_t none;
void expand_alternation(vector<pattern_t> &pats, const vector<maybe_token_t> &alternation);

LexdCompiler::LexdCompiler()
  : shouldAlign(false), input(NULL), inLex(false), inPat(false), lineNumber(0), doneReading(false), flagsUsed(0)
  {}

LexdCompiler::~LexdCompiler()
{}

void
LexdCompiler::die(wstring msg)
{
  fclose(input); // segfaults occasionally happen if this isn't here
  wcerr << L"Error on line " << lineNumber << ": " << msg << endl;
  exit(EXIT_FAILURE);
}

void
LexdCompiler::finishLexicon()
{
  if(inLex)
  {
    if(currentLexicon.size() == 0) die(L"Lexicon '" + currentLexiconName + L"' is empty.");
    if(lexicons.find(currentLexiconName) == lexicons.end())
    {
      Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
      lexicons[currentLexiconName] = lex;
    } else {
      lexicons[currentLexiconName]->addEntries(currentLexicon);
    }
    currentLexicon.clear();
  }
  currentLexiconName.clear();
  inLex = false;
}

void
LexdCompiler::checkName(wstring& name)
{
  const wchar_t *forbidden = L" :?|";
  if(name.size() > 0 && name.back() == L' ') name.pop_back();
  if(name.size() == 0) die(L"Unnamed pattern or lexicon");
  for(const wchar_t *pc = forbidden; *pc; pc++)
  {
    if(name.find(*pc) != wstring::npos)
      die(wstring(L"Lexicon/pattern names cannot contain character '") + (*pc) + L"'");
  }
}

void
LexdCompiler::processNextLine()//(FILE* input)
{
  wstring line;
  wint_t c;
  bool escape = false;
  bool comment = false;
  while((c = fgetwc(input)) != L'\n')
  {
    if(c == WEOF)
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
    else if(iswspace(c))
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
    finishLexicon();
    currentPatternName = L" ";
    inPat = true;
  }
  else if(line.size() > 7 && line.substr(0, 8) == L"PATTERN ")
  {
    wstring name = line.substr(8);
    checkName(name);
    finishLexicon();
    currentPatternName = name;
    inPat = true;
  }
  else if(line.size() > 7 && line.substr(0, 8) == L"LEXICON ")
  {
    wstring name = line.substr(8);
    finishLexicon();
    checkName(name);
    currentLexiconPartCount = 1;
    if(name.size() > 1 && name.back() == L')')
    {
      wstring num;
      for(unsigned int i = name.size()-2; i > 0; i--)
      {
        if(iswdigit(name[i])) num = name[i] + num;
        else if(name[i] == L'(' && num.size() > 0)
        {
          currentLexiconPartCount = stoi(num);
          name = name.substr(0, i);
        }
        else break;
      }
      if(name.size() == 0) die(L"Unnamed lexicon");
    }
    if(lexicons.find(name) != lexicons.end()) {
      if(lexicons[name]->getPartCount() != currentLexiconPartCount) {
        die(L"Multiple incompatible definitions for lexicon '" + name + L"'");
      }
    }
    currentLexiconName = name;
    inLex = true;
    inPat = false;
  }
  else if(line.size() >= 9 && line.substr(0,6) == L"ALIAS ")
  {
    finishLexicon();
    if(line.back() == L' ') line.pop_back();
    wstring::size_type loc = line.find(L" ", 6);
    if(loc == wstring::npos) die(L"Expected 'ALIAS lexicon alt_name'");
    wstring name = line.substr(6, loc-6);
    wstring alt = line.substr(loc+1);
    checkName(alt);
    if(lexicons.find(name) == lexicons.end()) die(L"Attempt to alias undefined lexicon '" + name + L"'");
    lexicons[alt] = lexicons[name];
    inLex = false;
    inPat = false;
  }
  else if(inPat)
  {
    // TODO: this should do some error checking (mismatches, mostly)
    if(line.back() != L' ') line += L' ';
    wstring cur;
    token_t tok;
    vector<pattern_t> pats(1);
    vector<maybe_token_t> alternation;
    bool final_alternative = true;

    for(auto ch : line)
    {
      if(ch == L' ')
      {
        bool option = false;
        if(cur.back() == L'?')
        {
          option = true;
          cur = cur.substr(0, cur.size()-1);
        }
        if(cur.size() == 0 && final_alternative) die(L"Syntax error - no lexicon name");
	if(!make_token(cur, tok))
	  continue;
        if(option)
	  alternation.push_back(none);
	alternation.push_back(tok);
	final_alternative = true;
        cur.clear();
      }
      else if(ch == L'|')
      {
        if(make_token(cur, tok))
          alternation.push_back(tok);
	else if(alternation.empty())
	  die(L"Syntax error - initial |");
	final_alternative = false;
	cur.clear();
      }
      else
      {
	if(cur.empty() && final_alternative)
	{
	  expand_alternation(pats, alternation);
	  alternation.clear();
	}
        cur += ch;
      }
    }
    if(!final_alternative)
      die(L"Syntax error - trailing |");
    expand_alternation(pats, alternation);
    for(const auto &pat: pats)
      patterns[currentPatternName].push_back(make_pair(lineNumber, pat));
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
LexdCompiler::buildPattern(int state, Transducer* t, const vector<pair<wstring, pair<Side, int>>>& pat, unsigned int pos)
{
  if(pos == pat.size())
  {
    t->setFinal(state);
    return;
  }
  wstring lex = pat[pos].first;
  Side side = pat[pos].second.first;
  int part = pat[pos].second.second;
  if(lexicons.find(lex) != lexicons.end())
  {
    Lexicon* l = lexicons[lex];
    if(part > l->getPartCount()) die(lex + L"(" + to_wstring(part) + L") - part is out of range");
    if(side == SideBoth && l->getPartCount() == 1)
    {
      int new_state = t->insertTransducer(state, *(l->getTransducer(alphabet, side, part, 0)));
      buildPattern(new_state, t, pat, pos+1);
    }
    else if(matchedParts.find(lex) == matchedParts.end())
    {
      for(int index = 0, max = l->getEntryCount(); index < max; index++)
      {
        int new_state = t->insertTransducer(state, *(l->getTransducer(alphabet, side, part, index)));
        if(new_state == state)
        {
          new_state = t->insertNewSingleTransduction(0, state);
        }
        matchedParts[lex] = index;
        buildPattern(new_state, t, pat, pos+1);
      }
      matchedParts.erase(lex);
    }
    else
    {
      int new_state = t->insertTransducer(state, *(l->getTransducer(alphabet, side, part, matchedParts[lex])));
      buildPattern(new_state, t, pat, pos+1);
    }
  }
  else if(patterns.find(lex) != patterns.end())
  {
    if(side != SideBoth || part != 1) die(L"Cannot select part or side of pattern '" + lex + L"'");
    int new_state = t->insertTransducer(state, *buildPattern(lex));
    buildPattern(new_state, t, pat, pos+1);
  }
  else
  {
    die(L"Lexicon or pattern '" + lex + L"' is not defined");
  }
}

Transducer*
LexdCompiler::buildPattern(wstring name)
{
  if(patternTransducers.find(name) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[name] = NULL;
    map<wstring, int> tempMatch;
    tempMatch.swap(matchedParts);
    for(auto& pat : patterns[name])
    {
      matchedParts.clear();
      lineNumber = pat.first;
      buildPattern(t->getInitial(), t, pat.second, 0);
    }
    tempMatch.swap(matchedParts);
    t->minimize();
    patternTransducers[name] = t;
  }
  else if(patternTransducers[name] == NULL)
  {
    die(L"Cannot compile self-recursive pattern '" + name + L"'");
  }
  return patternTransducers[name];
}

Transducer*
LexdCompiler::buildPatternWithFlags(wstring name)
{
  wstring letters = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if(patternTransducers.find(name) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[name] = NULL;
    for(auto& pat : patterns[name])
    {
      map<wstring, wstring> flags;
      map<wstring, int> lexCount;
      vector<wstring> clear;
      for(auto& part : pat.second)
      {
        if(lexCount.find(part.first) == lexCount.end()) lexCount[part.first] = 0;
        lexCount[part.first] += 1;
      }
      lineNumber = pat.first;
      int state = t->getInitial();
      for(auto& part : pat.second)
      {
        wstring& lex = part.first;
        if(lexicons.find(lex) != lexicons.end())
        {
          if(lexCount[lex] == 1)
          {
            state = t->insertTransducer(state, *(lexicons[lex]->getTransducerWithFlags(alphabet, part.second.first, part.second.second, L"")));
          }
          else
          {
            if(flags.find(lex) == flags.end())
            {
              int n = flagsUsed++;
              wstring f;
              while(n > 0 || f.size() == 0)
              {
                f += letters[n%26];
                n /= 26;
              }
              flags[lex] = f;
              clear.push_back(f);
            }
            state = t->insertTransducer(state, *(lexicons[lex]->getTransducerWithFlags(alphabet, part.second.first, part.second.second, flags[lex])));
          }
        }
        else if(patterns.find(lex) != patterns.end())
        {
          if(part.second.first != SideBoth || part.second.second != 1) die(L"Cannot select part or side of pattern '" + lex + L"'");
          state = t->insertTransducer(state, *(buildPatternWithFlags(lex)));
        }
        else
        {
          die(L"Lexicon or pattern '" + lex + L"' is not defined");
        }
      }
      for(auto& flag : clear)
      {
        wstring cl = L"@C." + flag + L"@";
        alphabet.includeSymbol(cl);
        int s = alphabet(cl);
        state = t->insertSingleTransduction(alphabet(s, s), state);
      }
      t->setFinal(state);
    }
    t->minimize();
    patternTransducers[name] = t;
  }
  else if(patternTransducers[name] == NULL)
  {
    die(L"Cannot compile self-recursive pattern '" + name + L"'");
  }
  return patternTransducers[name];
}

void
LexdCompiler::readFile(FILE* infile)
{
  input = infile;
  doneReading = false;
  while(!feof(input))
  {
    processNextLine();//(input);
    if(doneReading) break;
  }
  finishLexicon();
}

Transducer*
LexdCompiler::buildTransducer(bool usingFlags)
{
  if(usingFlags) return buildPatternWithFlags(L" ");
  else return buildPattern(L" ");
}

bool LexdCompiler::make_token(wstring tok_s, token_t &tok_out)
{
  if(tok_s.size() == 0) return false;
  int idx = 1;
  Side side = SideBoth;
  if(tok_s.front() == L':')
  {
    tok_s = tok_s.substr(1);
    side = SideRight;
  }
  else if(tok_s.back() == L':')
  {
    tok_s = tok_s.substr(0, tok_s.size()-1);
    side = SideLeft;
  }
  if(tok_s.size() == 0) die(L"Syntax error - colon without lexicon name");
  if(tok_s.size() > 1 && tok_s.back() == L')')
  {
    wstring temp;
    for(unsigned int i = tok_s.size()-2; i > 0; i--)
    {
      if(iswdigit(tok_s[i])) temp = tok_s[i] + temp;
      else if(tok_s[i] == L'(' && temp.size() > 0)
      {
        idx = stoi(temp);
        tok_s = tok_s.substr(0, i);
        break;
      }
      else break;
    }
  }
  tok_out = make_pair(tok_s, make_pair(side, idx));
  return true;
}

void expand_alternation(vector<pattern_t> &pats, const vector<maybe_token_t> &alternation)
{
  if(alternation.empty())
    return;
  if(pats.empty())
    pats.push_back(pattern_t());
  vector<pattern_t> new_pats;
  for(const auto &pat: pats)
  {
    for(const auto &maybe_tok: alternation)
    {
      auto pat1 = pat;
      if(maybe_tok != none)
        pat1.push_back(get<token_t>(maybe_tok));
      new_pats.push_back(pat1);
    }
  }
  pats = new_pats;
}
