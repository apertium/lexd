#include "lexdcompiler.h"
#include <variant>
#include <unicode/unistr.h>

using namespace icu;
using namespace std;

void expand_alternation(vector<pattern_t> &pats, const vector<pattern_element_t> &alternation);

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
  : shouldAlign(false), shouldCompress(false),
    input(NULL), inLex(false), inPat(false), lineNumber(0), doneReading(false),
    flagsUsed(0), anonymousCount(0)
{
  id_to_name.push_back("");
  name_to_id[""] = 0;
}

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
    if(currentLexicon.size() == 0) die(L"Lexicon '" + to_wstring(id_to_name[currentLexiconId]) + L"' is empty.");
    if(lexicons.find(currentLexiconId) == lexicons.end())
    {
      lexicons[currentLexiconId] = currentLexicon;
    } else {
      vector<entry_t>& lex = lexicons[currentLexiconId];
      lex.insert(lex.begin(), currentLexicon.begin(), currentLexicon.end());
    }
    currentLexicon.clear();
  }
  inLex = false;
}

unsigned int
LexdCompiler::internName(UnicodeString& name)
{
  if(name_to_id.find(name) == name_to_id.end())
  {
    name_to_id[name] = id_to_name.size();
    id_to_name.push_back(name);
  }
  return name_to_id[name];
}

unsigned int
LexdCompiler::checkName(UnicodeString& name)
{
  const static wchar_t* forbidden = L" :?|()<>[]*+";
  name.trim();
  int l = name.length();
  if(l == 0) die(L"Unnamed pattern or lexicon");

  for(int i = 0; i < (int)wcslen(forbidden); i++)
  {
    if(name.indexOf((char16_t*)forbidden, i, 1, 0, l) != -1)
      die(L"Lexicon/pattern names cannot contain character '" + wstring(&forbidden[i], 1) + L"'");
  }
  return internName(name);
}

pair<vector<int>, vector<int>>
LexdCompiler::processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count)
{
  vector<int> left;
  vector<int> right;
  bool inleft = true;
  if((*iter).startsWith(" "))
  {
    if((*iter).length() > 1)
    {
      // if it's a space with a combining diacritic after it,
      // then we want the diacritic
      UnicodeString cur = *iter;
      cur.retainBetween(1, cur.length());
      left.push_back(alphabet_lookup(cur));
    }
    ++iter;
  }
  if(iter == iter.end() && left.size() == 0)
    die(L"Expected " + to_wstring(currentLexiconPartCount) + L" parts, found " + to_wstring(part_count));
  for(; iter != iter.end(); ++iter)
  {
    if((*iter).startsWith(" ") || *iter == '[' || *iter == ']')
      break;
    else if(*iter == ":")
    {
      if(inleft)
        inleft = false;
      else
        die(L"Lexicon entry contains multiple colons");
    }
    else if(*iter == "\\")
    {
      (inleft ? left : right).push_back(alphabet_lookup(*++iter));
    }
    else if(*iter == "{" || *iter == "<")
    {
      UChar end = (*iter == "{") ? '}' : '>';
      int i = iter.span().first;
      for(; iter != iter.end() && *iter != end; ++iter) ;

      if(*iter == end)
      {
        int sym = alphabet_lookup(line.tempSubStringBetween(i, iter.span().second));
        (inleft ? left : right).push_back(sym);
      }
      else
        die(L"Multichar entry didn't end; searching for " + wstring((wchar_t*)&end, 1));
    }
    else (inleft ? left : right).push_back(alphabet_lookup(*iter));
  }
  if(inleft)
    right = left;
  return make_pair(left, right);
}

pair<unsigned int, unsigned int>
LexdCompiler::readToken(char_iter& iter, UnicodeString& line)
{
  if(*iter == ' ' || *iter == ':') ++iter;
  int i = iter.span().first;
  const UnicodeString boundary = " :()[]+*?|<>";
  for(; iter != iter.end() && boundary.indexOf(*iter) == -1 && (*iter).length() > 0; ++iter) ;
  UnicodeString name;
  line.extract(i, (iter != iter.end() ? iter.span().first : line.length()) - i, name);
  if(name.length() == 0)
    die(L"Colon without lexicon name");
  unsigned int part = 1;
  if(iter != iter.end() && *iter == '(')
  {
    ++iter;
    i = iter.span().first;
    for(; iter != iter.end() && *iter != ')'; ++iter)
    {
      if((*iter).length() != 1 || !u_isdigit((*iter).charAt(0)))
        die(L"Syntax error - non-numeric index in parentheses: " + to_wstring(*iter));
    }
    if(*iter != ')')
      die(L"Syntax error - unmached parenthesis");
    int len = iter.span().first - i;
    if(len == 0)
      die(L"Syntax error - missing index in parenthesis");
    part = (unsigned int)stoul(to_wstring(line.tempSubStringBetween(i, i+len)));
    ++iter;
  }
  // TODO: check for tags here
  return make_pair(internName(name), part);
}

RepeatMode
LexdCompiler::readModifier(char_iter& iter)
{
  if(*iter == "?")
  {
    ++iter;
    return Question;
  }
  else if(*iter == "*")
  {
    ++iter;
    return Star;
  }
  else if(*iter == "+")
  {
    ++iter;
    return Plus;
  }
  return Normal;
}

void
LexdCompiler::processPattern(char_iter& iter, UnicodeString& line)
{
  vector<pattern_t> pats_cur(1);
  vector<pattern_element_t> alternation;
  vector<vector<pattern_t>> patsets_fin;
  vector<vector<pattern_t>> patsets_pref(1);
  patsets_pref[0].push_back(pattern_t());
  bool final_alternative = true;
  bool sieve_forward = false;
  bool just_sieved = false;
  const UnicodeString boundary = " :()[]+*?|<>";

  for(; iter != iter.end() && *iter != ')' && (*iter).length() > 0; ++iter)
  {
    if(*iter == " ") ;
    else if(*iter == "|")
    {
      if(alternation.empty())
        die(L"Syntax error - initial |");
      final_alternative = false;
    }
    else if(*iter == "<")
    {
      if(sieve_forward)
        die(L"Syntax error - cannot sieve backwards after forwards.");
      if(alternation.empty())
        die(L"Backward sieve without token?");
      expand_alternation(pats_cur, alternation);
      alternation.clear();
      patsets_pref.push_back(pats_cur);
      pats_cur.clear();
      just_sieved = true;
    }
    else if(*iter == ">")
    {
      sieve_forward = true;
      if(alternation.empty())
        die(L"Forward sieve without token?");
      expand_alternation(pats_cur, alternation);
      patsets_fin.push_back(pats_cur);
      alternation.clear();
      just_sieved = true;
    }
    else if(*iter == "[")
    {
      UnicodeString name = UnicodeString::fromUTF8(" " + to_string(anonymousCount++));
      currentLexiconId = internName(name);
      currentLexiconPartCount = 1;
      inLex = true;
      entry_t entry;
      entry.push_back(processLexiconSegment(iter, line, 0));
      if(*iter == " ") iter++;
      if(*iter != "]")
        die(L"Missing closing ] for anonymous lexicon");
      currentLexicon.push_back(entry);
      finishLexicon();
      alternation.push_back({.lname=currentLexiconId, .rname=currentLexiconId,
                             .lpart=1, .rpart=1, .mode=readModifier(iter)});
      final_alternative = true;
      just_sieved = false;
    }
    else if(*iter == "(")
    {
      unsigned int temp = currentPatternId;
      UnicodeString name = UnicodeString::fromUTF8(" " + to_string(anonymousCount++));
      currentPatternId = internName(name);
      ++iter;
      processPattern(iter, line);
      if(iter == iter.end() || *iter != ")")
        die(L"Missing closing ) for anonymous pattern");
      ++iter;
      alternation.push_back({.lname=currentPatternId, .rname=currentPatternId,
                             .lpart=1, .rpart=1, .mode=readModifier(iter)});
      currentPatternId = temp;
      final_alternative = true;
      just_sieved = false;
    }
    else
    {
      if(final_alternative && !alternation.empty())
      {
        expand_alternation(pats_cur, alternation);
        alternation.clear();
      }
      pair<unsigned int, unsigned int> left;
      pair<unsigned int, unsigned int> right;
      if(*iter == ":")
      {
        left = make_pair(0, 0);
        ++iter;
        right = readToken(iter, line);
      }
      else
      {
        left = readToken(iter, line);
        if(*iter == ":")
        {
          ++iter;
          if(iter == iter.end() || (*iter).length() == 0 || boundary.indexOf(*iter) != -1)
            right = make_pair(0, 0);
          else
            right = readToken(iter, line);
        }
        else
          right = left;
      }
      alternation.push_back({.lname=left.first, .rname=right.first,
                             .lpart=left.second, .rpart=right.second,
                             .mode=readModifier(iter)});
      final_alternative = true;
      just_sieved = false;
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
          patterns[currentPatternId].push_back(make_pair(lineNumber, p));
        }
      }
    }
  }
}

void
LexdCompiler::processNextLine()
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
    UnicodeString name = " ";
    currentPatternId = internName(name);
    inPat = true;
  }
  else if(line.length() > 7 && line.startsWith("PATTERN "))
  {
    UnicodeString name = line.tempSubString(8);
    finishLexicon();
    currentPatternId = checkName(name);
    inPat = true;
  }
  else if(line.length() > 7 && line.startsWith("LEXICON "))
  {
    UnicodeString name = line.tempSubString(8);
    finishLexicon();
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
    currentLexiconId = checkName(name);
    if(lexicons.find(currentLexiconId) != lexicons.end()) {
      if(lexicons[currentLexiconId][0].size() != currentLexiconPartCount) {
        die(L"Multiple incompatible definitions for lexicon '" + to_wstring(name) + L"'");
      }
    }
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
    unsigned int altid = checkName(alt);
    unsigned int lexid = checkName(name);
    if(lexicons.find(lexid) == lexicons.end()) die(L"Attempt to alias undefined lexicon '" + to_wstring(name) + L"'");
    lexicons[altid] = lexicons[lexid];
    inLex = false;
    inPat = false;
  }
  else if(inPat)
  {
    char_iter iter = char_iter(line);
    processPattern(iter, line);
    if(iter != iter.end() && (*iter).length() > 0)
      die(L"Unexpected )");
  }
  else if(inLex)
  {
    char_iter iter = char_iter(line);
    entry_t entry;
    for(unsigned int i = 0; i < currentLexiconPartCount; i++)
    {
      entry.push_back(processLexiconSegment(iter, line, i));
    }
    if(*iter == ' ') ++iter;
    // TODO: check for tags here
    if(iter != iter.end())
      die(L"Lexicon entry has more than " + to_wstring(currentLexiconPartCount) + L" components");
    currentLexicon.push_back(entry);
  }
  else die(L"Expected 'PATTERNS' or 'LEXICON'");
}

void
LexdCompiler::buildPattern(int state, Transducer* t, const pattern_t& pat, const vector<int> is_free, unsigned int pos)
{
  if(pos == pat.size())
  {
    t->setFinal(state);
    return;
  }
  const pattern_element_t& tok = pat[pos];
  const bool llex = (tok.lname == 0) || (lexicons.find(tok.lname) != lexicons.end());
  const bool rlex = (tok.rname == 0) || (lexicons.find(tok.rname) != lexicons.end());
  if(llex && rlex)
  {
    if(is_free[pos] == 1)
    {
      int new_state = t->insertTransducer(state, *getLexiconTransducer(pat[pos], 0, true));
      buildPattern(new_state, t, pat, is_free, pos+1);
      return;
    }
    else if(matchedParts.find(tok.lname) == matchedParts.end() &&
            matchedParts.find(tok.rname) == matchedParts.end())
    {
      unsigned int max = lexicons[tok.lname ? tok.lname : tok.rname].size();
      for(unsigned int index = 0; index < max; index++)
      {
        int new_state = t->insertTransducer(state, *getLexiconTransducer(pat[pos], index, false));
        if(new_state == state)
        {
          new_state = t->insertNewSingleTransduction(0, state);
        }
        if(tok.lname) matchedParts[tok.lname] = index;
        if(tok.rname) matchedParts[tok.rname] = index;
        buildPattern(new_state, t, pat, is_free, pos+1);
      }
      if(tok.lname) matchedParts.erase(tok.lname);
      if(tok.rname) matchedParts.erase(tok.rname);
      return;
    }
    if(tok.lname && matchedParts.find(tok.lname) == matchedParts.end())
      matchedParts[tok.lname] = matchedParts[tok.rname];
    if(tok.rname && matchedParts.find(tok.rname) == matchedParts.end())
      matchedParts[tok.rname] = matchedParts[tok.lname];
    if(tok.lname && tok.rname && matchedParts[tok.lname] != matchedParts[tok.rname])
      die(L"Cannot collate " + to_wstring(id_to_name[tok.lname]) + L" with " + to_wstring(id_to_name[tok.rname]) + L" - both appear in free variation earlier in the pattern.");
    Transducer* lex = getLexiconTransducer(pat[pos], matchedParts[tok.lname ? tok.lname : tok.rname], false);
    int new_state = t->insertTransducer(state, *lex);
    buildPattern(new_state, t, pat, is_free, pos+1);
    return;
  }

  bool lpat = (patterns.find(tok.lname) != patterns.end());
  bool rpat = (patterns.find(tok.rname) != patterns.end());
  if(lpat && rpat)
  {
    if(tok.lname != tok.rname)
      die(L"Cannot collate patterns " + to_wstring(id_to_name[tok.lname]) + L" and " + to_wstring(id_to_name[tok.rname]));
    if(tok.lpart != 1 || tok.rpart != 1)
      die(L"Cannot select part of pattern " + to_wstring(id_to_name[tok.lname]));
    int new_state = t->insertTransducer(state, *buildPattern(tok.lname));
    if(tok.mode & Optional)
      t->linkStates(state, new_state, 0);
    if(tok.mode & Repeated)
      t->linkStates(new_state, state, 0);
    buildPattern(new_state, t, pat, is_free, pos+1);
    return;
  }

  // if we get to here it's an error of some kind
  // just have to determine which one
  if((lpat && !tok.rname) || (rpat && !tok.lname))
    die(L"Cannot select side of pattern " + to_wstring(id_to_name[tok.lname ? tok.lname : tok.rname]));
  else if((llex && rpat) || (lpat && rlex))
    die(L"Cannot collate lexicon with pattern " + to_wstring(id_to_name[tok.lname]) + L":" + to_wstring(id_to_name[tok.rname]));
  else
  {
    wcerr << "Patterns: ";
    for(auto pat: patterns)
      wcerr << to_wstring(id_to_name[pat.first]) << " ";
    wcerr << endl;
    wcerr << "Lexicons: ";
    for(auto l: lexicons)
      wcerr << to_wstring(id_to_name[l.first]) << " ";
    wcerr << endl;
    die(L"Lexicon or pattern '" + to_wstring(llex ? tok.rname : tok.lname) + L"' is not defined");
  }
}

Transducer*
LexdCompiler::buildPattern(unsigned int name)
{
  if(patternTransducers.find(name) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[name] = NULL;
    map<unsigned int, unsigned int> tempMatch;
    tempMatch.swap(matchedParts);
    for(auto& pat : patterns[name])
    {
      matchedParts.clear();
      lineNumber = pat.first;
      vector<int> is_free = vector<int>(pat.second.size(), 0);
      for(unsigned int i = 0; i < pat.second.size(); i++)
      {
        if(is_free[i] != 0)
          continue;
        const pattern_element_t& t1 = pat.second[i];
        for(unsigned int j = i+1; j < pat.second.size(); j++)
        {
          const pattern_element_t& t2 = pat.second[j];
          if((t1.lname && (t1.lname == t2.lname || t1.lname == t2.rname)) ||
             (t1.rname && (t1.rname == t2.lname || t1.rname == t2.rname)))
          {
            is_free[i] = -1;
            is_free[j] = -1;
          }
        }
        is_free[i] = (is_free[i] == 0 ? 1 : -1);
      }
      buildPattern(t->getInitial(), t, pat.second, is_free, 0);
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

/*Transducer*
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
        if(lexCount.find(part.first.first.first) == lexCount.end()) lexCount[part.first.first.first] = 0;
        lexCount[part.first.first.first] += 1;
      }
      lineNumber = pat.first;
      int state = t->getInitial();
      for(auto& part : pat.second)
      {
        UnicodeString& lex = part.first.first.first;
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
          //if(part.second.first != SideBoth || part.second.second != 1) die(L"Cannot select part or side of pattern '" + to_wstring(lex) + L"'");
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
}*/

void
LexdCompiler::readFile(UFILE* infile)
{
  input = infile;
  doneReading = false;
  while(!u_feof(input))
  {
    processNextLine();
    if(doneReading) break;
  }
  finishLexicon();
}

Transducer*
LexdCompiler::buildTransducer(bool usingFlags)
{
  //if(usingFlags) return buildPatternWithFlags(name_to_id[" "]);
  //else return buildPattern(name_to_id[" "]);
  return buildPattern(name_to_id[" "]);
}

void expand_alternation(vector<pattern_t> &pats, const vector<pattern_element_t> &alternation)
{
  if(alternation.empty())
    return;
  if(pats.empty())
    pats.push_back(pattern_t());
  vector<pattern_t> new_pats;
  for(const auto &pat: pats)
  {
    for(const auto &tok: alternation)
    {
      auto pat1 = pat;
      pat1.push_back(tok);
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
LexdCompiler::getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free)
{
  if(!free && entryTransducers.find(tok) != entryTransducers.end())
    return entryTransducers[tok][entry_index];
  if(free && lexiconTransducers.find(tok) != lexiconTransducers.end())
    return lexiconTransducers[tok];

  vector<entry_t> e_empty;
  vector<entry_t>& lents = (tok.lname ? lexicons[tok.lname] : e_empty);
  if(tok.lname && tok.lpart > lents[0].size())
    die(to_wstring(id_to_name[tok.lname]) + L"(" + to_wstring(tok.lpart) + L") - part is out of range");
  vector<entry_t>& rents = (tok.rname ? lexicons[tok.rname] : e_empty);
  if(tok.rname && tok.rpart > rents[0].size())
    die(to_wstring(id_to_name[tok.rname]) + L"(" + to_wstring(tok.rpart) + L") - part is out of range");
  // TODO: filter for tags hereish
  if(tok.lname && tok.rname && lents.size() != rents.size())
    die(L"Cannot collate " + to_wstring(id_to_name[tok.lname]) + L" with " + to_wstring(id_to_name[tok.rname]) + L" - differing numbers of entries");
  unsigned int count = (tok.lname ? lents.size() : rents.size());
  vector<Transducer*> trans;
  if(free)
    trans.push_back(new Transducer());
  else
    trans.reserve(count);
  vector<int> empty;
  for(unsigned int i = 0; i < count; i++)
  {
    Transducer* t = free ? trans[0] : new Transducer();
    insertEntry(t, (tok.lname ? lents[i][tok.lpart-1].first : empty),
                   (tok.rname ? rents[i][tok.rpart-1].second : empty));
    if(!free)
    {
      if(tok.mode == Question)
        t->optional();
      else if(tok.mode == Star)
        t->zeroOrMore();
      else if(tok.mode == Plus)
        t->oneOrMore();
      trans.push_back(t);
    }
  }
  if(free)
  {
    trans[0]->minimize();
    if(tok.mode == Question)
      trans[0]->optional();
    else if(tok.mode == Star)
      trans[0]->zeroOrMore();
    else if(tok.mode == Plus)
      trans[0]->oneOrMore();
    lexiconTransducers[tok] = trans[0];
    return trans[0];
  }
  else
  {
    entryTransducers[tok] = trans;
    return trans[entry_index];
  }
}
