#include "lexdcompiler.h"
#include "icu-iter.h"
#include <variant>
#include <unicode/unistr.h>

using namespace icu;
using namespace std;

typedef monostate none_t;
typedef variant<none_t, token_t> maybe_token_t;
const maybe_token_t none;
void expand_alternation(vector<pattern_t> &pats, const vector<maybe_token_t> &alternation);

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
  : shouldAlign(false), input(NULL), inLex(false), inPat(false), lineNumber(0), doneReading(false), flagsUsed(0)
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
      Lexicon* lex = new Lexicon(currentLexicon, shouldAlign);
      lexicons[currentLexiconName] = lex;
    } else {
      lexicons[currentLexiconName]->addEntries(currentLexicon);
    }
    currentLexicon.clear();
  }
  currentLexiconName.remove();
  inLex = false;
}

void
LexdCompiler::checkName(UnicodeString& name)
{
  const static wchar_t* forbidden = L" :?|";
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
      if(lexicons[name]->getPartCount() != currentLexiconPartCount) {
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
    token_t tok;
    vector<pattern_t> pats(1);
    vector<maybe_token_t> alternation;
    bool final_alternative = true;

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
        if(cur.length() == 0 && final_alternative) die(L"Syntax error - no lexicon name");
	if(!make_token(cur, tok))
	  continue;
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
      else
      {
	if(cur.isEmpty() && final_alternative)
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
  UnicodeString lex = pat[pos].first;
  Side side = pat[pos].second.first;
  unsigned int part = pat[pos].second.second;
  if(lexicons.find(lex) != lexicons.end())
  {
    Lexicon* l = lexicons[lex];
    if(part > l->getPartCount()) die(to_wstring(lex) + L"(" + to_wstring(part) + L") - part is out of range");
    if(side == SideBoth && l->getPartCount() == 1)
    {
      int new_state = t->insertTransducer(state, *(l->getTransducer(alphabet, side, part, 0)));
      buildPattern(new_state, t, pat, pos+1);
    }
    else if(matchedParts.find(lex) == matchedParts.end())
    {
      for(unsigned int index = 0, max = l->getEntryCount(); index < max; index++)
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
    if(side != SideBoth || part != 1) die(L"Cannot select part or side of pattern '" + to_wstring(lex) + L"'");
    int new_state = t->insertTransducer(state, *buildPattern(lex));
    buildPattern(new_state, t, pat, pos+1);
  }
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
    die(L"Lexicon or pattern '" + to_wstring(lex) + L"' is not defined");
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
        if(lexCount.find(part.first) == lexCount.end()) lexCount[part.first] = 0;
        lexCount[part.first] += 1;
      }
      lineNumber = pat.first;
      int state = t->getInitial();
      for(auto& part : pat.second)
      {
        UnicodeString& lex = part.first;
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
            state = t->insertTransducer(state, *(lexicons[lex]->getTransducerWithFlags(alphabet, part.second.first, part.second.second, to_wstring(flags[lex]))));
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

bool LexdCompiler::make_token(UnicodeString tok_s, token_t &tok_out)
{
  if(tok_s.length() == 0) return false;
  unsigned int idx = 1;
  Side side = SideBoth;
  if(tok_s.startsWith(":"))
  {
    tok_s = tok_s.retainBetween(1, tok_s.length());
    side = SideRight;
  }
  else if(tok_s.endsWith(":"))
  {
    tok_s = tok_s.retainBetween(0, tok_s.length()-1);
    side = SideLeft;
  }
  if(tok_s.length() == 0) die(L"Syntax error - colon without lexicon name in token");
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
