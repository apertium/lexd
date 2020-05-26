#include "lexdcompiler.h"
#include "icu-iter.h"
#include <variant>
#include <unicode/unistr.h>

using namespace icu;
using namespace std;

typedef monostate none_t;
typedef variant<none_t, token_pair_t> maybe_token_pair_t;
const maybe_token_pair_t none;
void expand_alternation(vector<pattern_t> &pats, const vector<maybe_token_pair_t> &alternation);

int LexdCompiler::alphabet_lookup(const UnicodeString &symbol)
{
  wstring wsymbol = to_wstring(symbol);
  if(wsymbol.length() == 1)
    return (int)wsymbol[0];
  else
  {
    alphabet.includeSymbol(wsymbol);
    return alphabet(wsymbol);
  }
}

LexdCompiler::LexdCompiler()
  : shouldAlign(false), shouldCompress(false), input(NULL), inLex(false), inPat(false), lineNumber(0), doneReading(false), flagsUsed(0)
  {}

LexdCompiler::~LexdCompiler()
{}

void
LexdCompiler::die(const wstring &msg)
{
  u_fclose(input); // segfaults occasionally happen if this isn't here
  wcerr << L"Error on line " << lineNumber << ": " << msg << endl;
  exit(EXIT_FAILURE);
}


void
LexdCompiler::finishLexicon()
{
  if(inLex)
  {
    if(currentLexicon.size() == 0) die(L"Lexicon '" + to_wstring(currentLexiconName) + L"' is empty.");
    if(lexicons.find(currentLexiconName) == lexicons.end())
    {
      //Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
      //lexicons[currentLexiconName] = lex;
      lexicons[currentLexiconName] = currentLexicon;
    } else {
      vector<entry_t>& lex = lexicons[currentLexiconName];
      lex.insert(lex.begin(), currentLexicon.begin(), currentLexicon.end());
      //lexicons[currentLexiconName].insert
      //lexicons[currentLexiconName]->addEntries(currentLexicon);
    }
    currentLexicon.clear();
  }
  currentLexiconName.remove();
  inLex = false;
}

void
LexdCompiler::checkName(UnicodeString& name)
{
  const static wchar_t* forbidden = L" :?|><";
  name.trim();
  int l = name.length();
  if(l == 0) die(L"Unnamed pattern or lexicon");

  for(int i = 0; i < (int)wcslen(forbidden); i++)
  {
    if(name.indexOf((char16_t*)forbidden, i, 1, 0, l) != -1)
      die(L"Lexicon/pattern names cannot contain character '" + wstring(&forbidden[i], 1) + L"'");
  }
}

void
LexdCompiler::processNextLine()//(FILE* input)
{
  UnicodeString line;
  UChar c;
  bool escape = false;
  bool comment = false;
  while((c = u_fgetc(input)) != L'\n')
  {
    if(c == U_EOF)
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
    else if(u_isWhitespace(c))
    {
      if(line.length() > 0 && !line.endsWith(' '))
      {
        line += ' ';
      }
    }
    else line += c;
  }
  lineNumber++;
  if(escape) die(L"Trailing backslash");
  if(line.length() == 0) return;

  if(line == "PATTERNS" || line == "PATTERNS ")
  {
    finishLexicon();
    currentPatternName = " ";
    inPat = true;
  }
  else if(line.length() > 7 && line.startsWith("PATTERN "))
  {
    UnicodeString name = line.tempSubString(8);
    checkName(name);
    finishLexicon();
    currentPatternName = name;
    inPat = true;
  }
  else if(line.length() > 7 && line.startsWith("LEXICON "))
  {
    UnicodeString name = line.tempSubString(8);
    finishLexicon();
    checkName(name);
    currentLexiconPartCount = 1;
    if(name.length() > 1 && name.endsWith(')'))
    {
      UnicodeString num;
      for(int i = name.length()-2; i > 0; i--)
      {
        if(u_isdigit(name[i])) num = name[i] + num;
        else if(name[i] == L'(' && num.length() > 0)
        {
          currentLexiconPartCount = (unsigned int)stoi(to_wstring(num));
          name = name.retainBetween(0, i);
        }
        else break;
      }
      if(name.length() == 0) die(L"Unnamed lexicon");
    }
    if(lexicons.find(name) != lexicons.end()) {
      if(lexicons[name][0].size() != currentLexiconPartCount) {
        die(L"Multiple incompatible definitions for lexicon '" + to_wstring(name) + L"'");
      }
    }
    currentLexiconName = name;
    inLex = true;
    inPat = false;
  }
  else if(line.length() >= 9 && line.startsWith("ALIAS "))
  {
    finishLexicon();
    if(line.endsWith(' ')) line.retainBetween(0, line.length()-1);
    int loc = line.indexOf(" ", 6);
    if(loc == -1) die(L"Expected 'ALIAS lexicon alt_name'");
    UnicodeString name = line.tempSubString(6, loc-6);
    UnicodeString alt = line.tempSubString(loc+1);
    checkName(alt);
    if(lexicons.find(name) == lexicons.end()) die(L"Attempt to alias undefined lexicon '" + to_wstring(name) + L"'");
    lexicons[alt] = lexicons[name];
    inLex = false;
    inPat = false;
  }
  else if(inPat)
  {
    // TODO: this should do some error checking (mismatches, mostly)
    if(!line.endsWith(' ')) line += ' ';
    UnicodeString cur;
    token_pair_t tok;
    vector<pattern_t> pats_cur(1);
    vector<maybe_token_pair_t> alternation;
    vector<vector<pattern_t>> patsets_fin;
    vector<vector<pattern_t>> patsets_pref(1);
    patsets_pref[0].push_back(pattern_t());
    bool final_alternative = true;
    bool sieve_forward = false;
    bool just_sieved = false;

    for(auto ch: char_iter(line))
    {
      if(ch == " ")
      {
        bool option = false;
        if(cur.endsWith('?'))
        {
          option = true;
          cur = cur.retainBetween(0, cur.length()-1);
        }
        if(cur.length() == 0 && final_alternative && !just_sieved) die(L"Syntax error - no lexicon name");
        if(!make_token(cur, tok))
          continue;
	just_sieved = false;
        if(option)
          alternation.push_back(none);
        alternation.push_back(tok);
        final_alternative = true;
        cur.remove();
      }
      else if(ch == "|")
      {
        if(make_token(cur, tok))
          alternation.push_back(tok);
        else if(alternation.empty())
          die(L"Syntax error - initial |");
        final_alternative = false;
        cur.remove();
      }
      else if(ch == ">")
      {
        sieve_forward = true;
        if(make_token(cur, tok))
          alternation.push_back(tok);
        if(alternation.empty())
          die(L"Forward sieve without token?");
        expand_alternation(pats_cur, alternation);
        patsets_fin.push_back(pats_cur);
        alternation.clear();
        cur.remove();
        just_sieved = true;
      }
      else if(ch == "<")
      {
        if (sieve_forward)
          die(L"Syntax error - cannot sieve backwards after forwards.");
        if(make_token(cur, tok))
          alternation.push_back(tok);
        if(alternation.empty())
          die(L"Backward sieve without token?");
        expand_alternation(pats_cur, alternation);
        alternation.clear();
        patsets_pref.push_back(pats_cur);
        pats_cur.clear();
        cur.remove();
        just_sieved = true;
      }
      else
      {
        if(cur.isEmpty() && final_alternative)
        {
          expand_alternation(pats_cur, alternation);
          alternation.clear();
        }
        cur += ch;
      }
    }
    if(!final_alternative)
      die(L"Syntax error - trailing |");
    if(just_sieved)
      die(L"Syntax error - trailing sieve (< or >)");
    expand_alternation(pats_cur, alternation);
    patsets_fin.push_back(pats_cur);
    for(const auto &patset_fin: patsets_fin)
    {
      for(const auto &pat: patset_fin)
      {
        for(const auto &patset_pref: patsets_pref)
        {
          for(const auto &pat_pref: patset_pref)
          {
            pattern_t p = pat_pref;
            p.reserve(p.size() + pat.size());
            p.insert(p.end(), pat.begin(), pat.end());
            patterns[currentPatternName].push_back(make_pair(lineNumber, p));
          }
        }
      }
    }
  }
  else if(inLex)
  {
    vector<UnicodeString> pieces;
    UnicodeString cur;
    for(auto it = char_iter(line); it != it.end(); ++it)
    {
      if(*it == "\\")
      {
        cur += *it;
        cur += *++it;
      }
      else if((*it).startsWith(" "))
      {
        if(!cur.isEmpty())
          pieces.push_back(cur + ":");

        // `cur` might be more than just a space; it could include some
        // combining code point. Keep anything after the space.
        cur = *it;
        cur.retainBetween(1, cur.length());
      }
      else {
        cur += *it;
      }
    }
    if(cur.length() > 0) pieces.push_back(cur + ":");
    vector<pair<vector<int>, vector<int>>> entry;
    for(auto ln : pieces)
    {
      vector<vector<int>> parts;
      vector<int> cur;
      for(auto it = char_iter(ln); it != it.end(); ++it)
      {
        if(*it == "\\")
          cur.push_back(alphabet_lookup(*++it));
        else if(*it == ":")
        {
          parts.push_back(cur);
          cur.clear();
        }
        else if(*it == "{" || *it == "<")
        {
          UChar end = (*it == "{") ? '}' : '>';
          int i = it.span().first;
          for(; it != it.end() && *it != end; ++it) ;

          if(*it == end)
            cur.push_back(alphabet_lookup(
              ln.tempSubStringBetween(i, it.span().second)
            ));
          else
            die(L"Multichar entry didn't end; searching for " + wstring((wchar_t*)&end, 1));
        }
        else cur.push_back(alphabet_lookup(*it));
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
      die(L"Lexicon entry has wrong number of components. Expected " + to_wstring(currentLexiconPartCount) + L", got " + to_wstring((unsigned int)entry.size()));
    }
    currentLexicon.push_back(entry);
  }
  else die(L"Expected 'PATTERNS' or 'LEXICON'");
}

void
LexdCompiler::buildPattern(int state, Transducer* t, const pattern_t& pat, unsigned int pos)
{
  if(pos == pat.size())
  {
    t->setFinal(state);
    return;
  }
  const UnicodeString& lname = pat[pos].first.first;
  const UnicodeString& rname = pat[pos].second.first;
  const bool llex = (lname.length() == 0) || (lexicons.find(lname) != lexicons.end());
  const bool rlex = (rname.length() == 0) || (lexicons.find(rname) != lexicons.end());
  if(llex && rlex)
  {
    bool lempty = (lname.length() == 0);
    bool rempty = (rname.length() == 0);
    if(matchedParts.find(lname) == matchedParts.end() &&
       matchedParts.find(rname) == matchedParts.end())
    {
      unsigned int max = lexicons[lname.length() == 0 ? rname : lname].size();
      for(unsigned int index = 0; index < max; index++)
      {
        int new_state = t->insertTransducer(state, *getLexiconTransducer(pat[pos], index));
        if(new_state == state)
        {
          new_state = t->insertNewSingleTransduction(0, state);
        }
        if(!lempty) matchedParts[lname] = index;
        if(!rempty) matchedParts[rname] = index;
        buildPattern(new_state, t, pat, pos+1);
      }
      if(!lempty) matchedParts.erase(lname);
      if(!rempty) matchedParts.erase(rname);
      return;
    }
    if(!lempty && matchedParts.find(lname) == matchedParts.end())
      matchedParts[lname] = matchedParts[rname];
    if(!rempty && matchedParts.find(rname) == matchedParts.end())
      matchedParts[rname] = matchedParts[lname];
    if(!lempty && !rempty && matchedParts[lname] != matchedParts[rname])
      die(L"Cannot collate " + to_wstring(lname) + L" with " + to_wstring(rname) + L" - both appear in free variation earlier in the pattern.");
    int new_state = t->insertTransducer(state, *getLexiconTransducer(pat[pos], matchedParts[lempty ? rname : lname]));
    buildPattern(new_state, t, pat, pos+1);
    return;
  }

  bool lpat = (patterns.find(lname) != patterns.end());
  bool rpat = (patterns.find(rname) != patterns.end());
  if(lpat && rpat)
  {
    if(lname != rname)
      die(L"Cannot collate patterns " + to_wstring(lname) + L" and " + to_wstring(rname));
    if(pat[pos].first.second != 1 || pat[pos].second.second != 1)
      die(L"Cannot select part of pattern " + to_wstring(lname));
    int new_state = t->insertTransducer(state, *buildPattern(lname));
    buildPattern(new_state, t, pat, pos+1);
    return;
  }

  // if we get to here it's an error of some kind
  // just have to determine which one
  if((lpat && rname.length() == 0) || (rpat && lname.length() == 0))
    die(L"Cannot select side of pattern " + to_wstring(lname) + to_wstring(rname));
  else if((llex && rpat) || (lpat && rlex))
    die(L"Cannot collate lexicon with pattern " + to_wstring(lname) + L":" + to_wstring(rname));
  else
  {
    wcerr << "Patterns: ";
    for(auto pat: patterns)
      wcerr << to_wstring(pat.first) << " ";
    wcerr << endl;
    wcerr << "Lexicons: ";
    for(auto l: lexicons)
      wcerr << to_wstring(l.first) << " ";
    wcerr << endl;
    die(L"Lexicon or pattern '" + to_wstring(llex ? rname : lname) + L"' is not defined");
  }
}

Transducer*
LexdCompiler::buildPattern(UnicodeString name)
{
  if(patternTransducers.find(name) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[name] = NULL;
    map<UnicodeString, unsigned int> tempMatch;
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
    die(L"Cannot compile self-recursive pattern '" + to_wstring(name) + L"'");
  }
  return patternTransducers[name];
}

Transducer*
LexdCompiler::buildPatternWithFlags(UnicodeString name)
{
  UnicodeString letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if(patternTransducers.find(name) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[name] = NULL;
    for(auto& pat : patterns[name])
    {
      map<UnicodeString, UnicodeString> flags;
      map<UnicodeString, int> lexCount;
      vector<UnicodeString> clear;
      for(auto& part : pat.second)
      {
        if(lexCount.find(part.first.first) == lexCount.end()) lexCount[part.first.first] = 0;
        lexCount[part.first.first] += 1;
      }
      lineNumber = pat.first;
      int state = t->getInitial();
      for(auto& part : pat.second)
      {
        UnicodeString& lex = part.first.first;
        if(lexicons.find(lex) != lexicons.end())
        {
          if(lexCount[lex] == 1)
          {
            //state = t->insertTransducer(state, *(lexicons[lex]->getTransducerWithFlags(alphabet, part.second.first, part.second.second, L"")));
          }
          else
          {
            if(flags.find(lex) == flags.end())
            {
              unsigned int n = flagsUsed++;
              UnicodeString f;
              while(n > 0 || f.length() == 0)
              {
                f += letters[(int)n%26];
                n /= 26;
              }
              flags[lex] = f;
              clear.push_back(f);
            }
            //state = t->insertTransducer(state, *(lexicons[lex]->getTransducerWithFlags(alphabet, part.second.first, part.second.second, to_wstring(flags[lex]))));
          }
        }
        else if(patterns.find(lex) != patterns.end())
        {
          if(part.second.first != SideBoth || part.second.second != 1) die(L"Cannot select part or side of pattern '" + to_wstring(lex) + L"'");
          state = t->insertTransducer(state, *(buildPatternWithFlags(lex)));
        }
        else
        {
          die(L"Lexicon or pattern '" + to_wstring(lex) + L"' is not defined");
        }
      }
      for(auto& flag : clear)
      {
        UnicodeString cl = "@C." + flag + "@";
        int s = alphabet_lookup(cl);
        state = t->insertSingleTransduction(alphabet(s, s), state);
      }
      t->setFinal(state);
    }
    t->minimize();
    patternTransducers[name] = t;
  }
  else if(patternTransducers[name] == NULL)
  {
    die(L"Cannot compile self-recursive pattern '" + to_wstring(name) + L"'");
  }
  return patternTransducers[name];
}

void
LexdCompiler::readFile(UFILE* infile)
{
  input = infile;
  doneReading = false;
  while(!u_feof(input))
  {
    processNextLine();//(input);
    if(doneReading) break;
  }
  finishLexicon();
}

Transducer*
LexdCompiler::buildTransducer(bool usingFlags)
{
  if(usingFlags) return buildPatternWithFlags(" ");
  else return buildPattern(" ");
}

bool LexdCompiler::make_token(UnicodeString tok_s, token_pair_t &tok_out)
{
  if(tok_s.length() == 0) return false;
  int loc = tok_s.indexOf(":");
  if(loc == -1)
  {
    make_single_token(tok_s, tok_out.first);
    tok_out.second = tok_out.first;
    return true;
  }
  if(tok_s.length() == 1) die(L"Syntax error - colon without lexicon name in token");
  UnicodeString left;
  tok_s.extract(0, loc, left);
  tok_s.retainBetween(loc+1, tok_s.length());
  make_single_token(left, tok_out.first);
  make_single_token(tok_s, tok_out.second);
  return true;
}

void LexdCompiler::make_single_token(UnicodeString tok_s, token_t &tok_out)
{
  if(tok_s.length() == 0)
  {
    tok_out = make_pair("", 0);
    return;
  }
  unsigned int idx = 1;
  if(tok_s.length() > 1 && tok_s.endsWith(")"))
  {
    UnicodeString temp;
    tok_s.retainBetween(0, tok_s.length() - 1);
    for(char_iter it = rev_char_iter(tok_s); it != it.begin() && *it != "("; --it)
    {
      if((*it).length() != 1 || !u_isdigit((*it).charAt(0)))
        die(L"Syntax error - non-numeric index in parentheses: " + to_wstring(*it));
      else
        temp += *it;
    }
    tok_s = tok_s.retainBetween(0, tok_s.length() - temp.length() - 1);
    if (tok_s.isEmpty())
      die(L"Syntax error - unmatched parenthesis");
    else if(temp.isEmpty())
      die(L"Syntax error - missing index in parenthesis");
    idx = (unsigned int)stoul(to_wstring(temp));
  }
  tok_out = make_pair(tok_s, idx);
}

void expand_alternation(vector<pattern_t> &pats, const vector<maybe_token_pair_t> &alternation)
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
        pat1.push_back(get<token_pair_t>(maybe_tok));
      new_pats.push_back(pat1);
    }
  }
  pats = new_pats;
}

void
LexdCompiler::insertEntry(Transducer* trans, vector<int>& left, vector<int>& right)
{
  int state = trans->getInitial();
  if(!shouldAlign)
  {
    for(unsigned int i = 0; i < left.size() || i < right.size(); i++)
    {
      int l = (i < left.size()) ? left[i] : 0;
      int r = (i < right.size()) ? right[i] : 0;
      state = trans->insertSingleTransduction(alphabet(l, r), state);
    }
  }
  else
  {
    /*
      This code is adapted from hfst/libhfst/src/parsers/lexc-utils.cc
      It uses the Levenshtein distance algorithm to determine the optimal
      alignment of two strings.
      In hfst-lexc, the priority order for ties is SUB > DEL > INS
      which ensures that 000abc:xyz000 is preferred over abc000:000xyz
      However, we're traversing the strings backwards to simplify extracting
      the final alignment, so we need to switch INS and DEL.
      If shouldCompress is true, we set the cost of SUB to 1 in order to prefer
      a:b over 0:b a:0 without changing the alignment of actual correspondences.
    */
    const unsigned int INS = 0;
    const unsigned int DEL = 1;
    const unsigned int SUB = 2;
    const unsigned int ins_cost = 1;
    const unsigned int del_cost = 1;
    const unsigned int sub_cost = (shouldCompress ? 1 : 100);

    const unsigned int len1 = left.size();
    const unsigned int len2 = right.size();
    unsigned int cost[len1+1][len2+1];
    unsigned int path[len1+1][len2+1];
    cost[0][0] = 0;
    path[0][0] = 0;
    for(unsigned int i = 1; i <= len1; i++)
    {
      cost[i][0] = del_cost * i;
      path[i][0] = DEL;
    }
    for(unsigned int i = 1; i <= len2; i++)
    {
      cost[0][i] = ins_cost * i;
      path[0][i] = INS;
    }

    for(unsigned int i = 1; i <= len1; i++)
    {
      for(unsigned int j = 1; j <= len2; j++)
      {
        unsigned int sub = cost[i-1][j-1] + (left[len1-i] == right[len2-j] ? 0 : sub_cost);
        unsigned int ins = cost[i][j-1] + ins_cost;
        unsigned int del = cost[i-1][j] + del_cost;

        if(sub <= ins && sub <= del)
        {
          cost[i][j] = sub;
          path[i][j] = SUB;
        }
        else if(ins <= del)
        {
          cost[i][j] = ins;
          path[i][j] = INS;
        }
        else
        {
          cost[i][j] = del;
          path[i][j] = DEL;
        }
      }
    }

    for(unsigned int x = len1, y = len2; (x > 0) || (y > 0);)
    {
      int symbol;
      switch(path[x][y])
      {
        case SUB:
          symbol = alphabet(left[len1-x], right[len2-y]);
          x--;
          y--;
          break;
        case INS:
          symbol = alphabet(0, right[len2-y]);
          y--;
          break;
        default: // DEL
          symbol = alphabet(left[len1-x], 0);
          x--;
      }
      state = trans->insertSingleTransduction(symbol, state);
    }
  }
  trans->setFinal(state);
}

Transducer*
LexdCompiler::getLexiconTransducer(token_pair_t tok, unsigned int entry_index)
{
  UnicodeString& lname = tok.first.first;
  UnicodeString& rname = tok.second.first;
  unsigned int lpart = tok.first.second;
  unsigned int rpart = tok.second.second;

  if(lname == rname && lexicons[lname][0].size() == 1)
  {
    if(lexiconTransducers.find(lname) != lexiconTransducers.end())
      return lexiconTransducers[lname];
    Transducer* trans = new Transducer();
    for(auto entry : lexicons[lname])
    {
      insertEntry(trans, entry[0].first, entry[0].second);
    }
    trans->minimize();
    lexiconTransducers[lname] = trans;
    return trans;
  }

  if(entryTransducers.find(tok) != entryTransducers.end())
    return entryTransducers[tok][entry_index];
  bool lempty = (lname == "");
  bool rempty = (rname == "");
  vector<entry_t> e_empty;
  vector<entry_t>& lents = (lempty ? e_empty : lexicons[lname]);
  if(!lempty && lpart > lents[0].size())
    die(to_wstring(lname) + L"(" + to_wstring(lpart) + L") - part is out of range");
  vector<entry_t>& rents = (rempty ? e_empty : lexicons[rname]);
  if(!rempty && rpart > rents[0].size())
    die(to_wstring(rname) + L"(" + to_wstring(rpart) + L") - part is out of range");
  if(!lempty && !rempty && lents.size() != rents.size())
    die(L"Cannot collate " + to_wstring(lname) + L" with " + to_wstring(rname) + L" - differing numbers of entries");
  unsigned int count = (lempty ? rents.size() : lents.size());
  vector<Transducer*> trans;
  trans.reserve(count);
  vector<int> empty;
  for(unsigned int i = 0; i < count; i++)
  {
    Transducer* t = new Transducer();
    insertEntry(t, (lempty ? empty : lents[i][lpart-1].first),
                   (rempty ? empty : rents[i][rpart-1].second));
    trans.push_back(t);
  }
  entryTransducers[tok] = trans;
  return trans[entry_index];
}
