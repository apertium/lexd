#include "lexdcompiler.h"
#include <unicode/unistr.h>
#include <memory>

using namespace icu;
using namespace std;

bool tag_filter_t::combinable(const tag_filter_t &other) const
{
  // TODO: make ops combinable even with non-empty filters?
  if(empty() || other.empty())
    return true;
  return intersectset(pos(), other.neg()).empty() && intersectset(other.pos(), neg()).empty() && ops().empty() && other.ops().empty();
}
bool tag_filter_t::combine(const tag_filter_t &other)
{
  if(!combinable(other))
    return false;
  unionset_inplace(_pos, other._pos);
  unionset_inplace(_neg, other._neg);
  for(const auto &op: other._ops)
    _ops.push_back(op);
  return true;
}

void expand_alternation(vector<pattern_t> &pats, const vector<pattern_element_t> &alternation);
vector<pattern_element_t> distribute_tag_expressions(const pattern_element_t &token)
{
  vector<pattern_element_t> result;
  for(const auto &f: token.tag_filter.distribute())
  {
    pattern_element_t new_token = token;
    new_token.tag_filter = f;
    result.push_back(new_token);
  }
  return result;
}

bool tag_filter_t::compatible(const tags_t &tags) const
{
  return subset(pos(), tags) && intersectset(neg(), tags).empty() && ops().empty();
}
bool tag_filter_t::applicable(const tags_t &tags) const
{
  return subset(neg(), tags) && ops().empty();
}
bool tag_filter_t::try_apply(tags_t &tags) const
{
  if(!applicable(tags))
    return false;
  subtractset_inplace(tags, neg());
  unionset_inplace(tags, pos());
  return true;
}
bool pattern_element_t::compatible(const lex_seg_t &tok) const
{
  return left.name.empty() || right.name.empty() || tag_filter.compatible(tok.tags);
}
const UnicodeString &LexdCompiler::name(string_ref r) const
{
  return id_to_name[(unsigned int)r];
}

trans_sym_t LexdCompiler::alphabet_lookup(const UnicodeString &symbol)
{
  if (!symbol.hasMoreChar32Than(0, symbol.length(), 1)) {
    return trans_sym_t((int)symbol.char32At(0));
  } else {
    UString temp;
    temp.append(symbol.getBuffer(), symbol.length());
    alphabet.includeSymbol(temp);
    return trans_sym_t(alphabet(temp));
  }
}
trans_sym_t LexdCompiler::alphabet_lookup(trans_sym_t l, trans_sym_t r)
{
  return trans_sym_t(alphabet((int)l, (int)r));
}

LexdCompiler::LexdCompiler()
  : shouldAlign(false), shouldCompress(false), shouldCombine(true),
    tagsAsFlags(false), shouldHypermin(false), tagsAsMinFlags(false),
    input(NULL), inLex(false), inPat(false), lineNumber(0), doneReading(false),
    anonymousCount(0), transitionCount(0)
{
  id_to_name.push_back("");
  name_to_id[""] = string_ref(0);
  lexicons[string_ref(0)] = vector<entry_t>();

  left_sieve_name = internName("<");
  token_t lsieve_tok = {.name=left_sieve_name, .part=1};
  pattern_element_t lsieve_elem = {.left=lsieve_tok, .right=lsieve_tok,
                                   .tag_filter=tag_filter_t(),
                                   .mode=Normal};
  left_sieve_tok = vector<pattern_element_t>(1, lsieve_elem);

  right_sieve_name = internName(">");
  token_t rsieve_tok = {.name=right_sieve_name, .part=1};
  pattern_element_t rsieve_elem = {.left=rsieve_tok, .right=rsieve_tok,
                                   .tag_filter=tag_filter_t(),
                                   .mode=Normal};
  right_sieve_tok = vector<pattern_element_t>(1, rsieve_elem);
}

LexdCompiler::~LexdCompiler()
{}

void
LexdCompiler::die(const wstring &msg)
{
  wcerr << L"Error on line " << lineNumber << ": " << msg << endl;
  exit(EXIT_FAILURE);
}

void LexdCompiler::appendLexicon(string_ref lexicon_id, const vector<entry_t> &to_append)
{
  if(lexicons.find(lexicon_id) == lexicons.end())
    lexicons[lexicon_id] = to_append;
  else
    lexicons[lexicon_id].insert(lexicons[lexicon_id].begin(), to_append.begin(), to_append.end());
}

void
LexdCompiler::finishLexicon()
{
  if(inLex)
  {
    if(currentLexicon.size() == 0) die(L"Lexicon '" + to_wstring(name(currentLexiconId)) + L"' is empty.");
    appendLexicon(currentLexiconId, currentLexicon);
    
    currentLexicon.clear();
    currentLexicon_tags.clear();
  }
  inLex = false;
}

string_ref
LexdCompiler::internName(const UnicodeString& name)
{
  if(name_to_id.find(name) == name_to_id.end())
  {
    name_to_id[name] = string_ref(id_to_name.size());
    id_to_name.push_back(name);
  }
  return name_to_id[name];
}

string_ref
LexdCompiler::checkName(UnicodeString& name)
{
  const static UnicodeString forbidden = " :?|()<>[]*+";
  name.trim();
  int l = name.length();
  if(l == 0) die(L"Unnamed pattern or lexicon");

  for(const auto &c: char_iter(forbidden)) {
    if(name.indexOf(c) != -1) {
      die(L"Lexicon/pattern names cannot contain character '" + to_wstring(c) + L"'");
    }
  }
  return internName(name);
}

tags_t
LexdCompiler::readTags(char_iter &iter, UnicodeString &line)
{
  tag_filter_t filter = readTagFilter(iter, line);
  if(filter.neg().empty() && filter.ops().empty())
    return tags_t((set<string_ref>)filter.pos());
  else
     die(L"Cannot declare negative tag in lexicon");
  return tags_t();
}

tag_filter_t
LexdCompiler::readTagFilter(char_iter& iter, UnicodeString& line)
{
  tag_filter_t tag_filter;
  auto tag_start = (++iter).span();
  bool tag_nonempty = false;
  bool negative = false;
  vector<shared_ptr<op_tag_filter_t>> ops;
  for(; iter != iter.end() && (*iter).length() > 0; ++iter)
  {
    if(*iter == "]" || *iter == "," || *iter == " ")
    {
      if(!tag_nonempty)
        die(L"Empty tag at char " + to_wstring(iter.span().first));
      UnicodeString s = line.tempSubStringBetween(tag_start.first, iter.span().first);
      if(!tag_filter.combine(
        negative ? tag_filter_t(neg_tag_filter_t {checkName(s)})
                 : tag_filter_t(pos_tag_filter_t {checkName(s)})
      ))
        die(L"Illegal tag filter.");
      tag_nonempty = false;
      negative = false;
      if(*iter == "]")
      {
        iter++;
        return tag_filter_t(tag_filter.pos(), tag_filter.neg(), ops);
      }
    }
    else if(!tag_nonempty && *iter == "-")
    {
      negative = true;
    }
    else if(!tag_nonempty && (*iter == "|" || *iter == "^"))
    {
      const UnicodeString s = *iter;
      if(negative)
        die(L"Illegal negated operation.");
      *iter++;
      if (*iter == "[")
      {
        shared_ptr<op_tag_filter_t> op;
        tags_t operands = readTags(iter, line);
        if (s == "|")
          op = make_shared<or_tag_filter_t>(operands);
        else if (s == "^")
          op = make_shared<xor_tag_filter_t>(operands);
        ops.push_back(op);
      }
      else
        die(L"Expected list of operands.");
      if(*iter == "]")
      {
        iter++;
        return tag_filter_t(tag_filter.pos(), tag_filter.neg(), ops);
      }
    }
    else if(!tag_nonempty)
    {
      tag_nonempty = true;
      tag_start = iter.span();
    }
  }
  die(L"End of line in tag list, expected ']'");
  return tag_filter_t();
}

lex_seg_t
LexdCompiler::processLexiconSegment(char_iter& iter, UnicodeString& line, unsigned int part_count)
{
  lex_seg_t seg;
  bool inleft = true;
  bool left_tags_applied = false, right_tags_applied = false;
  tag_filter_t tags;
  if((*iter).startsWith(" "))
  {
    if((*iter).length() > 1)
    {
      // if it's a space with a combining diacritic after it,
      // then we want the diacritic
      UnicodeString cur = *iter;
      cur.retainBetween(1, cur.length());
      seg.left.symbols.push_back(alphabet_lookup(cur));
    }
    ++iter;
  }
  if((*iter).startsWith("/") && seg.left.symbols.size() == 0)
  {
    die(L"Lexicon entries beginning with unescaped / are reserved for future use. Please use \\/");
  }
  if(iter == iter.end() && seg.left.symbols.size() == 0)
    die(L"Expected " + to_wstring(currentLexiconPartCount) + L" parts, found " + to_wstring(part_count));
  for(; iter != iter.end(); ++iter)
  {
    if((*iter).startsWith(" ") || *iter == ']')
      break;
    else if(*iter == "[")
    {
      auto &tags_applied = inleft ? left_tags_applied : right_tags_applied;
      if(tags_applied)
        die(L"Already provided tag list for this side.");
      tags = readTagFilter(iter, line);
      --iter;
      tags_applied = true;
    }
    else if(*iter == ":")
    {
      if(inleft)
        inleft = false;
      else
        die(L"Lexicon entry contains multiple colons");
    }
    else if(*iter == "\\")
    {
	  if(shouldCombine)
	  {
		(inleft ? seg.left : seg.right).symbols.push_back(alphabet_lookup(*++iter));
	  }
	  else
	  {
		++iter;
		for(int c = 0; c < (*iter).length(); c++)
		{
		  (inleft ? seg.left : seg.right).symbols.push_back(alphabet_lookup((*iter)[c]));
		}
	  }
    }
    else if(*iter == "{" || *iter == "<")
    {
      UChar end = (*iter == "{") ? '}' : '>';
      int i = iter.span().first;
      for(; iter != iter.end() && *iter != end; ++iter) ;

      if(*iter == end)
      {
        trans_sym_t sym = alphabet_lookup(line.tempSubStringBetween(i, iter.span().second));
        (inleft ? seg.left : seg.right).symbols.push_back(sym);
      }
      else
      {
        die(L"Multichar entry didn't end; searching for " + to_wstring(end));
      }
    }
	else if(!shouldCombine)
	{
	  for(int c = 0; c < (*iter).length(); c++)
	  {
		(inleft ? seg.left : seg.right).symbols.push_back(alphabet_lookup((*iter)[c]));
	  }
	}
    else (inleft ? seg.left : seg.right).symbols.push_back(alphabet_lookup(*iter));
  }
  if(inleft)
  {
    seg.right = seg.left;
  }

  seg.tags = currentLexicon_tags;

  if(!tags.try_apply(seg.tags))
  {
    tags_t diff = subtractset(tags.neg(), seg.tags);
    for(string_ref t: diff)
      wcerr << L"Bad tag '-" << to_wstring(name(t)) << L"'" << endl;
    die(L"Negative tag has no default to unset.");
  }

  return seg;
}

token_t
LexdCompiler::readToken(char_iter& iter, UnicodeString& line)
{
  auto begin_charspan = iter.span();

  const UnicodeString boundary = " :()[]+*?|<>";

  for(; iter != iter.end() && boundary.indexOf(*iter) == -1; ++iter);
  UnicodeString name;
  line.extract(begin_charspan.first, (iter == iter.end() ? line.length() : iter.span().first) - begin_charspan.first, name);

  if(name.length() == 0)
    die(L"Symbol '" + to_wstring(*iter) + L"' without lexicon name at u16 " + to_wstring(iter.span().first) + L"-" + to_wstring(iter.span().second-1));

  unsigned int part = 1;
  if(*iter == "(")
  {
    iter++;
    begin_charspan = iter.span();
    for(; iter != iter.end() && (*iter).length() > 0 && *iter != ")"; iter++)
    {
      if((*iter).length() != 1 || !u_isdigit((*iter).charAt(0)))
        die(L"Syntax error - non-numeric index in parentheses: " + to_wstring(*iter));
    }
    if(*iter != ")")
      die(L"Syntax error - unmatched parenthesis");
    if(iter.span().first == begin_charspan.first)
      die(L"Syntax error - missing index in parenthesis");
    part = (unsigned int)stoul(to_wstring(line.tempSubStringBetween(begin_charspan.first, iter.span().first)));
    ++iter;
  }

  return token_t {.name = internName(name), .part = part};
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

pattern_element_t
LexdCompiler::readPatternElement(char_iter& iter, UnicodeString& line)
{
  const UnicodeString boundary = " :()[]+*?|<>";
  pattern_element_t tok;
  if(*iter == ":")
  {
    iter++;
    if(boundary.indexOf(*iter) != -1)
    {
      if(*iter == ":")
        die(L"Syntax error - double colon");
      else
        die(L"Colon without lexicon or pattern name");
    }
    tok.right = readToken(iter, line);
  }
  else if(boundary.indexOf(*iter) != -1)
  {
    die(L"Unexpected symbol '" + to_wstring(*iter) + L"' at " + to_wstring(iter.span().first));
  }
  else
  {
    tok.left = readToken(iter, line);
    if(*iter == "[")
    {
      tok.tag_filter.combine(readTagFilter(iter, line));
    }
    if(*iter == ":")
    {
      iter++;
      if(iter != iter.end() && (*iter).length() > 0)
      {
        if(boundary.indexOf(*iter) == -1)
        {
          tok.right = readToken(iter, line);
        }
      }
    }
    else
    {
      tok.right = tok.left;
    }
  }
  if(*iter == "[")
  {
    tok.tag_filter.combine(readTagFilter(iter, line));
  }
  tok.mode = readModifier(iter);

  return tok;
}

void
LexdCompiler::processPattern(char_iter& iter, UnicodeString& line)
{
  vector<pattern_t> pats_cur(1);
  vector<pattern_element_t> alternation;
  bool final_alternative = true;
  bool sieve_forward = false;
  bool just_sieved = false;
  const UnicodeString boundary = " :()[]+*?|<>";
  const UnicodeString token_boundary = " )|<>";
  const UnicodeString token_side_boundary = token_boundary + ":+*?";
  const UnicodeString token_side_name_boundary = token_side_boundary + "([]";
  const UnicodeString modifier = "+*?";
  const UnicodeString decrement_after_token = token_boundary + "([]";

  for(; iter != iter.end() && *iter != ')' && (*iter).length() > 0; ++iter)
  {
    if(*iter == " ") ;
    else if(*iter == "|")
    {
      if(alternation.empty())
        die(L"Syntax error - initial |");
      if(!final_alternative)
        die(L"Syntax error - multiple consecutive |");
      if(just_sieved)
        die(L"Syntax error - sieve and alternation operators without intervening token");
      final_alternative = false;
    }
    else if(*iter == "<")
    {
      if(sieve_forward)
        die(L"Syntax error - cannot sieve backwards after forwards.");
      if(alternation.empty())
        die(L"Backward sieve without token?");
      if(just_sieved)
        die(L"Syntax error - multiple consecutive sieve operators");
      if(!final_alternative)
        die(L"Syntax error - alternation and sieve operators without intervening token");
      expand_alternation(pats_cur, alternation);
      expand_alternation(pats_cur, left_sieve_tok);
      alternation.clear();
      just_sieved = true;
    }
    else if(*iter == ">")
    {
      sieve_forward = true;
      if(alternation.empty())
        die(L"Forward sieve without token?");
      if(just_sieved)
        die(L"Syntax error - multiple consecutive sieve operators");
      if(!final_alternative)
        die(L"Syntax error - alternation and sieve operators without intervening token");
      expand_alternation(pats_cur, alternation);
      expand_alternation(pats_cur, right_sieve_tok);
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
      entry.push_back(processLexiconSegment(++iter, line, 0));
      if(*iter == " ") iter++;
      if(*iter != "]")
        die(L"Missing closing ] for anonymous lexicon");
      currentLexicon.push_back(entry);
      finishLexicon();
      if(final_alternative && !alternation.empty())
      {
        expand_alternation(pats_cur, alternation);
        alternation.clear();
      }
      ++iter;
      pattern_element_t anon;
      anon.left = {.name=currentLexiconId, .part=1};
      anon.right = anon.left;
      anon.mode = readModifier(iter);
      alternation.push_back(anon);
      --iter;
      final_alternative = true;
      just_sieved = false;
    }
    else if(*iter == "(")
    {
      string_ref temp = currentPatternId;
      UnicodeString name = UnicodeString::fromUTF8(" " + to_string(anonymousCount++));
      currentPatternId = internName(name);
      ++iter;
      processPattern(iter, line);
      if(*iter == " ")
        *iter++;
      if(iter == iter.end() || *iter != ")")
        die(L"Missing closing ) for anonymous pattern");
      ++iter;
      tag_filter_t filter;
      if(*iter == "[")
        filter = readTagFilter(iter, line);
      if(final_alternative && !alternation.empty())
      {
        expand_alternation(pats_cur, alternation);
        alternation.clear();
      }
      pattern_element_t anon;
      anon.left = {.name=currentPatternId, .part=1};
      anon.right = anon.left;
      anon.mode = readModifier(iter);
      anon.tag_filter = filter;
      for(const auto &tok : distribute_tag_expressions(anon))
        alternation.push_back(tok);
      --iter;
      currentPatternId = temp;
      final_alternative = true;
      just_sieved = false;
    }
    else if(*iter == "?" || *iter == "*" || *iter == "+")
    {
      die(L"Syntax error - unexpected modifier at u16 " + to_wstring(iter.span().first) + L"-" + to_wstring(iter.span().second-1));
    }
    else
    {
      if(final_alternative && !alternation.empty())
      {
        expand_alternation(pats_cur, alternation);
        alternation.clear();
      }
      for(const auto &tok : distribute_tag_expressions(readPatternElement(iter, line)))
        alternation.push_back(tok);
      iter--;
      final_alternative = true;
      just_sieved = false;
    }
  }
  if(!final_alternative)
    die(L"Syntax error - trailing |");
  if(just_sieved)
    die(L"Syntax error - trailing sieve (< or >)");
  expand_alternation(pats_cur, alternation);
  for(const auto &pat : pats_cur)
  {
    patterns[currentPatternId].push_back(make_pair(lineNumber, pat));
  }
}

void
LexdCompiler::processNextLine()
{
  UnicodeString line;
  UChar c;
  bool escape = false;
  bool comment = false;
  bool lastWasSpace = false;
  while((c = u_fgetc(input)) != L'\n')
  {
    bool space = false;
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
      if(line.length() > 0 && !lastWasSpace)
      {
        line += ' ';
      }
      space = (line.length() > 0);
    }
    else line += c;
    lastWasSpace = space;
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
    name.trim();
    finishLexicon();
    if(name.length() > 1 && name.indexOf('[') != -1)
    {
      UnicodeString tags = name.tempSubString(name.indexOf('['));
      auto c = char_iter(tags);
      currentLexicon_tags = readTags(c, tags);
      if(c != c.end() && *c == ":")
      {
        wcerr << L"WARNING: One-sided tags are deprecated and will soon be removed (line " << lineNumber << L")" << endl;
        ++c;
        if(*c == "[")
          unionset_inplace(currentLexicon_tags, readTags(c, tags));
	else
          die(L"Expected start of default right tags '[' after ':'.");
      }
      if(c != c.end())
        die(L"Unexpected character '" + to_wstring(*c) + L"' after default tags.");
      name.retainBetween(0, name.indexOf('['));
    }
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
    string_ref altid = checkName(alt);
    string_ref lexid = checkName(name);
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
      die(L"Unexpected " + to_wstring(*iter));
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
    if(iter != iter.end())
      die(L"Lexicon entry has '" + to_wstring(*iter) + L"' (found at u16 " + to_wstring(iter.span().first) + L"), more than " + to_wstring(currentLexiconPartCount) + L" components");
    currentLexicon.push_back(entry);
  }
  else die(L"Expected 'PATTERNS' or 'LEXICON'");
}

bool
LexdCompiler::isLexiconToken(const pattern_element_t& tok)
{
  const bool llex = (tok.left.name.empty() || (lexicons.find(tok.left.name) != lexicons.end()));
  const bool rlex = (tok.right.name.empty() || (lexicons.find(tok.right.name) != lexicons.end()));
  if(llex && rlex)
  {
    return true;
  }
  const bool lpat = (patterns.find(tok.left.name) != patterns.end());
  const bool rpat = (patterns.find(tok.right.name) != patterns.end());
  if(tok.left.name == tok.right.name && lpat && rpat)
  {
    if(tok.left.part != 1 || tok.right.part != 1)
    {
      die(L"Cannote select part of pattern " + to_wstring(name(tok.right.name)));
    }
    return false;
  }
  // Any other scenario is an error, so we need to die()
  if(lpat && rpat)
  {
    die(L"Cannot collate pattern " + to_wstring(name(tok.left.name)) + L" with " + to_wstring(name(tok.right.name)));
  }
  else if((lpat && tok.right.name.empty()) || (rpat && tok.left.name.empty()))
  {
    die(L"Cannot select side of pattern " + to_wstring(name(tok.left.name.valid() ? tok.left.name : tok.right.name)));
  }
  else if(llex && rpat)
  {
    die(L"Cannot collate lexicon " + to_wstring(name(tok.left.name)) + L" with pattern " + to_wstring(name(tok.right.name)));
  }
  else if(lpat && rlex)
  {
    die(L"Cannot collate pattern " + to_wstring(name(tok.left.name)) + L" with lexicon " + to_wstring(name(tok.right.name)));
  }
  else
  {
    wcerr << "Patterns: ";
    for(auto pat: patterns)
      wcerr << to_wstring(name(pat.first)) << " ";
    wcerr << endl;
    wcerr << "Lexicons: ";
    for(auto l: lexicons)
      wcerr << to_wstring(name(l.first)) << " ";
    wcerr << endl;
    die(L"Lexicon or pattern '" + to_wstring(name((llex || lpat) ? tok.right.name : tok.left.name)) + L"' is not defined");
  }
  // we never reach this point, but the compiler doesn't understand die()
  // so we put a fake return value to keep it happy
  return false;
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
  if(tok.left.name == left_sieve_name)
  {
    t->linkStates(t->getInitial(), state, 0);
    buildPattern(state, t, pat, is_free, pos+1);
  }
  else if(tok.left.name == right_sieve_name)
  {
    t->setFinal(state);
    buildPattern(state, t, pat, is_free, pos+1);
  }
  else if(isLexiconToken(tok))
  {
    if(is_free[pos] == 1)
    {
      Transducer *lex = getLexiconTransducer(pat[pos], 0, true);
      if(lex)
      {
        int new_state = t->insertTransducer(state, *lex);
        buildPattern(new_state, t, pat, is_free, pos+1);
      }
      return;
    }
    else if(matchedParts.find(tok.left.name) == matchedParts.end() &&
            matchedParts.find(tok.right.name) == matchedParts.end())
    {
      unsigned int max = lexicons[tok.left.name || tok.right.name].size();
      for(unsigned int index = 0; index < max; index++)
      {
        Transducer *lex = getLexiconTransducer(pat[pos], index, false);
        if(lex)
        {
          int new_state = t->insertTransducer(state, *lex);
          if(new_state == state)
          {
            new_state = t->insertNewSingleTransduction(0, state);
          }
          if(tok.left.name.valid()) matchedParts[tok.left.name] = index;
          if(tok.right.name.valid()) matchedParts[tok.right.name] = index;
          buildPattern(new_state, t, pat, is_free, pos+1);
        }
      }
      if(tok.left.name.valid()) matchedParts.erase(tok.left.name);
      if(tok.right.name.valid()) matchedParts.erase(tok.right.name);
      return;
    }
    if(tok.left.name.valid() && matchedParts.find(tok.left.name) == matchedParts.end())
      matchedParts[tok.left.name] = matchedParts[tok.right.name];
    if(tok.right.name.valid() && matchedParts.find(tok.right.name) == matchedParts.end())
      matchedParts[tok.right.name] = matchedParts[tok.left.name];
    if(tok.left.name.valid() && tok.right.name.valid() && matchedParts[tok.left.name] != matchedParts[tok.right.name])
      die(L"Cannot collate " + to_wstring(name(tok.left.name)) + L" with " + to_wstring(name(tok.right.name)) + L" - both appear in free variation earlier in the pattern.");
    Transducer* lex = getLexiconTransducer(pat[pos], matchedParts[tok.left.name || tok.right.name], false);
    if(lex)
    {
      int new_state = t->insertTransducer(state, *lex);
      buildPattern(new_state, t, pat, is_free, pos+1);
    }
    return;
  }
  else
  {
    Transducer *p = buildPattern(tok);
    if(p)
    {
      int new_state = t->insertTransducer(state, *p);
      if(tok.mode & Optional)
        t->linkStates(state, new_state, 0);
      if(tok.mode & Repeated)
        t->linkStates(new_state, state, 0);
      buildPattern(new_state, t, pat, is_free, pos+1);
    }
    return;
  }
}

vector<int>
LexdCompiler::determineFreedom(pattern_t& pat)
{
  vector<int> is_free = vector<int>(pat.size(), 0);
  for(unsigned int i = 0; i < pat.size(); i++)
  {
    if(is_free[i] != 0)
      continue;
    const pattern_element_t& t1 = pat[i];
    for(unsigned int j = i+1; j < pat.size(); j++)
    {
      const pattern_element_t& t2 = pat[j];
      if((t1.left.name.valid() && (t1.left.name == t2.left.name || t1.left.name == t2.right.name)) ||
         (t1.right.name.valid() && (t1.right.name == t2.left.name || t1.right.name == t2.right.name)))
      {
        is_free[i] = -1;
        is_free[j] = -1;
      }
    }
    is_free[i] = (is_free[i] == 0 ? 1 : -1);
  }
  return is_free;
}

Transducer*
LexdCompiler::buildPattern(const pattern_element_t &tok)
{
  if(tok.left.part != 1 || tok.right.part != 1)
    die(L"Cannot build collated pattern " + to_wstring(name(tok.left.name)));
  if(patternTransducers.find(tok) == patternTransducers.end())
  {
    Transducer* t = new Transducer();
    patternTransducers[tok] = NULL;
    map<string_ref, unsigned int> tempMatch;
    tempMatch.swap(matchedParts);
    for(auto &pat_untagged : patterns[tok.left.name])
    {
      for(unsigned int i = 0; i < pat_untagged.second.size(); i++)
      {
        auto pat = pat_untagged;
        for(auto &pair: pat.second)
        {
          if(!pair.tag_filter.combine(tok.tag_filter.neg()))
            die(L"Incompatible tag filters.");
        }
        if(!pat.second[i].tag_filter.combine(tok.tag_filter.pos()))
          die(L"Incompatible tag filters.");

        matchedParts.clear();
        lineNumber = pat.first;
        vector<int> is_free = determineFreedom(pat.second);
        buildPattern(t->getInitial(), t, pat.second, is_free, 0);
      }
    }
    tempMatch.swap(matchedParts);
    if(!t->hasNoFinals())
    {
      t->minimize();
      patternTransducers[tok] = t;
    }
  }
  else if(patternTransducers[tok] == NULL)
  {
    die(L"Cannot compile self-recursive or empty pattern '" + to_wstring(name(tok.left.name)) + L"'");
  }
  return patternTransducers[tok];
}

int
LexdCompiler::insertPreTags(Transducer* t, int state, tag_filter_t &tags)
{
  int end = state;
  for(auto tag : tags.pos())
  {
    trans_sym_t flag = getFlag(Positive, tag, 1);
    end = t->insertSingleTransduction((int)alphabet_lookup(flag, flag), end);
  }
  for(auto tag : tags.neg())
  {
    trans_sym_t flag = getFlag(Positive, tag, 2);
    end = t->insertSingleTransduction((int)alphabet_lookup(flag, flag), end);
  }
  return end;
}

int
LexdCompiler::insertPostTags(Transducer* t, int state, tag_filter_t &tags)
{
  int end = 0;
  int flag_dest = 0;
  for(auto tag : tags.pos())
  {
    trans_sym_t flag = getFlag(Disallow, tag, 1);
    trans_sym_t clear = getFlag(Clear, tag, 0);
    if(flag_dest == 0)
    {
      flag_dest = t->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
      end = flag_dest;
    }
    else
    {
      t->linkStates(state, flag_dest, (int)alphabet_lookup(flag, flag));
    }
    end = t->insertSingleTransduction((int)alphabet_lookup(clear, clear), end);
  }
  if(end == 0)
  {
    end = state;
  }
  for(auto tag : tags.neg())
  {
    trans_sym_t clear = getFlag(Clear, tag, 0);
    end = t->insertSingleTransduction((int)alphabet_lookup(clear, clear), end);
  }
  return end;
}

Transducer*
LexdCompiler::buildPatternWithFlags(const pattern_element_t &tok, int pattern_start_state = 0)
{
  if(patternTransducers.find(tok) == patternTransducers.end())
  {
    Transducer* trans = (shouldHypermin ? hyperminTrans : new Transducer());
    patternTransducers[tok] = NULL;
    unsigned int transition_index = 0;
    vector<int> pattern_finals;
    bool did_anything = false;
    for(auto& pat : patterns[tok.left.name])
    {
      lineNumber = pat.first;
      vector<int> is_free = determineFreedom(pat.second);
      bool got_non_null = false;
      unsigned int count = (tok.tag_filter.pos().size() > 0 ? pat.second.size() : 1);
      if(tagsAsFlags) count = 1;
      for(unsigned int idx = 0; idx < count; idx++)
      {
        int state = pattern_start_state;
        vector<int> finals;
        set<string_ref> to_clear;
        bool got_null = false;
        for(unsigned int i = 0; i < pat.second.size(); i++)
        {
          pattern_element_t cur = pat.second[i];

          if(cur.left.name == left_sieve_name)
          {
            trans->linkStates(pattern_start_state, state, 0);
            continue;
          }
          else if(cur.left.name == right_sieve_name)
          {
            finals.push_back(state);
            continue;
          }

          bool isLex = isLexiconToken(cur);

          transition_index++;

          int mode_start = state;
          cur.mode = Normal;

          tag_filter_t current_tags;
          if(tagsAsFlags)
          {
            state = insertPreTags(trans, state, cur.tag_filter);
            current_tags = cur.tag_filter;
            cur.tag_filter = tag_filter_t();
          }
          else
          {
            if(i == idx)
            {
              if(!cur.tag_filter.combine(tok.tag_filter.pos()))
                die(L"Incompatible tag filters.");
            }
            if(!cur.tag_filter.combine(tok.tag_filter.neg()))
              die(L"Incompatible tag filters.");
          }

          Transducer* t;
          if(shouldHypermin)
          {
            trans_sym_t inflag = getFlag(Positive, tok.left.name, transition_index);
            trans_sym_t outflag = getFlag(Require, tok.left.name, transition_index);
            int in_tr = (int)alphabet_lookup(inflag, inflag);
            int out_tr = (int)alphabet_lookup(outflag, outflag);
            if(is_free[i] == -1 && isLex)
            {
              to_clear.insert(cur.left.name);
              to_clear.insert(cur.right.name);
            }
            if(transducerLocs.find(cur) != transducerLocs.end())
            {
              auto loc = transducerLocs[cur];
              if(loc.first == loc.second)
              {
                t = NULL;
              }
              else
              {
                t = trans;
                trans->linkStates(state, loc.first, in_tr);
                state = trans->insertSingleTransduction(out_tr, loc.second);
              }
            }
            else
            {
              int start = trans->insertSingleTransduction(in_tr, state);
              int end = start;
              if(isLex)
              {
                t = getLexiconTransducerWithFlags(cur, false);
                if(t == NULL)
                {
                  transducerLocs[cur] = make_pair(start, start);
                }
                else
                {
                  end = trans->insertTransducer(start, *t);
                  transducerLocs[cur] = make_pair(start, end);
                }
              }
              else
              {
                t = buildPatternWithFlags(cur, start);
                end = transducerLocs[cur].second;
              }
              state = trans->insertSingleTransduction(out_tr, end);
            }
          }
          else if(isLex)
          {
            t = getLexiconTransducerWithFlags(cur, (is_free[i] == 1));
            if(is_free[i] == -1)
            {
              to_clear.insert(cur.left.name);
              to_clear.insert(cur.right.name);
            }
          }
          else
          {
            t = buildPatternWithFlags(cur);
          }
          if(t == NULL || (!shouldHypermin && t->hasNoFinals()))
          {
            got_null = true;
            break;
          }
          got_non_null = true;
          if(!shouldHypermin)
          {
            state = trans->insertTransducer(state, *t);
          }
          if(tagsAsFlags)
          {
            state = insertPostTags(trans, state, current_tags);
          }
          if(pat.second[i].mode & Optional)
          {
            trans->linkStates(mode_start, state, 0);
          }
          if(pat.second[i].mode & Repeated)
          {
            trans->linkStates(state, mode_start, 0);
          }
        }
        if(!got_null || finals.size() > 0)
        {
          for(auto fin : finals)
          {
            trans->linkStates(fin, state, 0);
          }
          for(auto lex : to_clear)
          {
            if(lex.empty())
            {
              continue;
            }
            UnicodeString flag = "@C.";
            encodeFlag(flag, (int)lex.i);
            flag += "@";
            trans_sym_t f = alphabet_lookup(flag);
            state = trans->insertSingleTransduction((int)alphabet_lookup(f, f), state);
          }
          trans->setFinal(state);
          pattern_finals.push_back(state);
        }
      }
      if(!got_non_null)
      {
        continue;
      }
      did_anything = true;
    }
    if(did_anything)
    {
      if(shouldHypermin)
      {
        if(pattern_finals.size() > 0)
        {
          int end = pattern_finals[0];
          for(auto fin : pattern_finals)
          {
            if(fin != end)
            {
              trans->linkStates(fin, end, 0);
            }
            if(pattern_start_state != 0)
            {
              trans->setFinal(fin, 0, false);
            }
          }
          pattern_element_t key = tok;
          key.mode = Normal;
          transducerLocs[key] = make_pair(pattern_start_state, end);
        }
      }
      else
      {
        if(!trans->hasNoFinals())
          trans->minimize();
      }
    }
    else
    {
      if(!shouldHypermin)
        trans = NULL;
      else
      {
        wcerr << "FIXME" << endl;
      }
    }
    patternTransducers[tok] = trans;
  }
  else if(patternTransducers[tok] == NULL)
  {
    die(L"Cannot compile self-recursive pattern '" + to_wstring(name(tok.left.name)) + L"'");
  }
  return patternTransducers[tok];
}

void
LexdCompiler::buildAllLexicons()
{
  // find out if there are any lexicons that we can build without flags
  vector<pattern_element_t> lexicons_to_build;
  for(auto pattern : patterns)
  {
    for(auto pat : pattern.second)
    {
      lineNumber = pat.first;
      vector<int> free = determineFreedom(pat.second);
      for(size_t i = 0; i < pat.second.size(); i++)
      {
        if(pat.second[i].left.name == left_sieve_name ||
           pat.second[i].left.name == right_sieve_name)
        {
          continue;
        }
        if(isLexiconToken(pat.second[i]))
        {
          pattern_element_t& tok = pat.second[i];
          if(free[i] == -1)
          {
            lexiconFreedom[tok.left.name] = false;
            lexiconFreedom[tok.right.name] = false;
          }
          else
          {
            if(lexiconFreedom.find(tok.left.name) == lexiconFreedom.end())
            {
              lexiconFreedom[tok.left.name] = true;
            }
            if(lexiconFreedom.find(tok.right.name) == lexiconFreedom.end())
            {
              lexiconFreedom[tok.right.name] = true;
            }
          }
          lexicons_to_build.push_back(tok);
        }
      }
    }
  }
  lexiconFreedom[string_ref(0)] = true;
  for(auto tok : lexicons_to_build)
  {
    tok.tag_filter = tag_filter_t();
    tok.mode = Normal;
    bool free = ((tok.left.name.empty() || lexiconFreedom[tok.left.name]) &&
                 (tok.right.name.empty() || lexiconFreedom[tok.right.name]));
    getLexiconTransducerWithFlags(tok, free);
  }
}

int
LexdCompiler::buildPatternSingleLexicon(pattern_element_t tok, int start_state)
{
  if(patternTransducers.find(tok) == patternTransducers.end() || patternTransducers[tok] != NULL)
  {
    patternTransducers[tok] = NULL;
    int end = -1;
    string_ref transition_flag = internName(" ");
    for(auto pattern : patterns[tok.left.name])
    {
      int next_start_state = start_state;
      size_t next_start_idx = 0;
      lineNumber = pattern.first;
      set<string_ref> to_clear;
      size_t count = (tok.tag_filter.pos().empty() ? 1 : pattern.second.size());
      for(size_t tag_idx = 0; tag_idx < count; tag_idx++)
      {
        int state = next_start_state;
        bool finished = true;
        for(size_t i = next_start_idx; i < pattern.second.size(); i++)
        {
          pattern_element_t cur = pattern.second[i];

          if(cur.left.name == left_sieve_name)
          {
            hyperminTrans->linkStates(start_state, state, 0);
            continue;
          }
          else if(cur.left.name == right_sieve_name)
          {
            if(end == -1)
            {
              end = hyperminTrans->insertNewSingleTransduction(0, state);
            }
            else
            {
              hyperminTrans->linkStates(state, end, 0);
            }
            continue;
          }

          if(i == tag_idx)
          {
            next_start_state = state;
            next_start_idx = tag_idx;
            cur.tag_filter.combine(tok.tag_filter.pos());
          }
          cur.tag_filter.combine(tok.tag_filter.neg());

          int mode_state = state;

          if(isLexiconToken(cur))
          {
            tags_t tags = cur.tag_filter.tags();
            for(auto tag : tags)
            {
              trans_sym_t flag = getFlag(Clear, tag, 0);
              state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
            }
            pattern_element_t untagged = cur;
            untagged.tag_filter = tag_filter_t();
            untagged.mode = Normal;
            bool free = (lexiconFreedom[cur.left.name] && lexiconFreedom[cur.right.name]);
            if(!free)
            {
              to_clear.insert(cur.left.name);
              to_clear.insert(cur.right.name);
            }
            trans_sym_t inflag = getFlag(Positive, transition_flag, transitionCount);
            trans_sym_t outflag = getFlag(Require, transition_flag, transitionCount);
            transitionCount++;
            if(transducerLocs.find(untagged) == transducerLocs.end())
            {
              state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(inflag, inflag), state);
              Transducer* lex = getLexiconTransducerWithFlags(untagged, free);
              int start = state;
              state = hyperminTrans->insertTransducer(state, *lex);
              transducerLocs[untagged] = make_pair(start, state);
            }
            else
            {
              auto loc = transducerLocs[untagged];
              hyperminTrans->linkStates(state, loc.first, (int)alphabet_lookup(inflag, inflag));
              state = loc.second;
            }
            state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(outflag, outflag), state);
            for(auto tag : cur.tag_filter.pos())
            {
              trans_sym_t flag = getFlag(Require, tag, 1);
              state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
            }
            for(auto tag : cur.tag_filter.neg())
            {
              trans_sym_t flag = getFlag(Disallow, tag, 1);
              state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
            }
          }
          else
          {
            state = buildPatternSingleLexicon(cur, state);
            if(state == -1)
            {
              finished = false;
              break;
            }
          }

          if(cur.mode & Optional)
          {
            hyperminTrans->linkStates(mode_state, state, 0);
          }
          if(cur.mode & Repeated)
          {
            hyperminTrans->linkStates(state, mode_state, 0);
          }
        }
        if(finished)
        {
          for(auto lex : to_clear)
          {
            if(lex.empty())
            {
              continue;
            }
            trans_sym_t flag = getFlag(Clear, lex, 0);
            state = hyperminTrans->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
          }
          if(end == -1)
          {
            end = state;
          }
          else
          {
            hyperminTrans->linkStates(state, end, 0);
          }
        }
      }
    }
    patternTransducers.erase(tok);
    return end;
  }
  else
  {
    die(L"Cannot compile self-recursive pattern " + to_wstring(name(tok.left.name)));
    return 0;
  }
}

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
  token_t start_tok = {.name = internName(" "), .part = 1};
  pattern_element_t start_pat = {.left=start_tok, .right=start_tok,
                                 .tag_filter=tag_filter_t(),
                                 .mode=Normal};
  if(usingFlags)
  {
    if(shouldHypermin)
    {
      hyperminTrans = new Transducer();
    }
    Transducer *t = buildPatternWithFlags(start_pat);
    if(shouldHypermin)
      t->minimize();
    return t;
  }
  else return buildPattern(start_pat);
}

Transducer*
LexdCompiler::buildTransducerSingleLexicon()
{
  tagsAsMinFlags = true;
  token_t start_tok = {.name = internName(" "), .part = 1};
  pattern_element_t start_pat = {.left=start_tok, .right=start_tok,
                                 .tag_filter=tag_filter_t(),
                                 .mode=Normal};
  hyperminTrans = new Transducer();
  buildAllLexicons();
  int end = buildPatternSingleLexicon(start_pat, 0);
  if(end == -1)
  {
    die(L"No non-empty patterns found");
  }
  hyperminTrans->setFinal(end);
  hyperminTrans->minimize();
  return hyperminTrans;
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
LexdCompiler::insertEntry(Transducer* trans, const lex_seg_t &seg)
{
  int state = trans->getInitial();
  if(tagsAsFlags)
  {
    for(string_ref tag : seg.tags)
    {
      trans_sym_t check1 = getFlag(Require, tag, 1);
      trans_sym_t check2 = getFlag(Disallow, tag, 2);
      trans_sym_t clear = getFlag(Clear, tag, 0);
      int state2 = trans->insertSingleTransduction((int)alphabet_lookup(check1, check1), state);
      int state3 = trans->insertSingleTransduction((int)alphabet_lookup(clear, clear), state2);
      trans->linkStates(state, state3, 0);
      state = trans->insertSingleTransduction((int)alphabet_lookup(check2, check2), state3);
    }
  }
  else if(tagsAsMinFlags)
  {
    for(string_ref tag : seg.tags)
    {
      trans_sym_t flag = getFlag(Positive, tag, 1);
      state = trans->insertSingleTransduction((int)alphabet_lookup(flag, flag), state);
    }
  }
  if(!shouldAlign)
  {
    for(unsigned int i = 0; i < seg.left.symbols.size() || i < seg.right.symbols.size(); i++)
    {
      trans_sym_t l = (i < seg.left.symbols.size()) ? seg.left.symbols[i] : trans_sym_t();
      trans_sym_t r = (i < seg.right.symbols.size()) ? seg.right.symbols[i] : trans_sym_t();
      state = trans->insertSingleTransduction(alphabet((int)l, (int)r), state);
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

    const unsigned int len1 = seg.left.symbols.size();
    const unsigned int len2 = seg.right.symbols.size();
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
        unsigned int sub = cost[i-1][j-1] + (seg.left.symbols[len1-i] == seg.right.symbols[len2-j] ? 0 : sub_cost);
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
      trans_sym_t symbol;
      switch(path[x][y])
      {
        case SUB:
          symbol = alphabet_lookup(seg.left.symbols[len1-x], seg.right.symbols[len2-y]);
          x--;
          y--;
          break;
        case INS:
          symbol = alphabet_lookup(trans_sym_t(), seg.right.symbols[len2-y]);
          y--;
          break;
        default: // DEL
          symbol = alphabet_lookup(seg.left.symbols[len1-x], trans_sym_t());
          x--;
      }
      state = trans->insertSingleTransduction((int)symbol, state);
    }
  }
  trans->setFinal(state);
}

void
LexdCompiler::applyMode(Transducer* trans, RepeatMode mode)
{
  if(mode == Question)
    trans->optional();
  else if(mode == Star)
    trans->zeroOrMore();
  else if(mode == Plus)
    trans->oneOrMore();
}

Transducer*
LexdCompiler::getLexiconTransducer(pattern_element_t tok, unsigned int entry_index, bool free)
{
  if(!free && entryTransducers.find(tok) != entryTransducers.end())
    return entryTransducers[tok][entry_index];
  if(free && lexiconTransducers.find(tok) != lexiconTransducers.end())
    return lexiconTransducers[tok];

  vector<entry_t>& lents = lexicons[tok.left.name];
  if(tok.left.name.valid() && tok.left.part > lents[0].size())
    die(to_wstring(name(tok.left.name)) + L"(" + to_wstring(tok.left.part) + L") - part is out of range");
  vector<entry_t>& rents = lexicons[tok.right.name];
  if(tok.right.name.valid() && tok.right.part > rents[0].size())
    die(to_wstring(name(tok.right.name)) + L"(" + to_wstring(tok.right.part) + L") - part is out of range");
  if(tok.left.name.valid() && tok.right.name.valid() && lents.size() != rents.size())
    die(L"Cannot collate " + to_wstring(name(tok.left.name)) + L" with " + to_wstring(name(tok.right.name)) + L" - differing numbers of entries");
  unsigned int count = (tok.left.name.valid() ? lents.size() : rents.size());
  vector<Transducer*> trans;
  if(free)
    trans.push_back(new Transducer());
  else
    trans.reserve(count);
  lex_seg_t empty;
  bool did_anything = false;
  for(unsigned int i = 0; i < count; i++)
  {
    lex_seg_t& le = (tok.left.name.valid() ? lents[i][tok.left.part-1] : empty);
    lex_seg_t& re = (tok.right.name.valid() ? rents[i][tok.right.part-1] : empty);
    tags_t tags = unionset(le.tags, re.tags);
    if(!tok.tag_filter.compatible(tags))
    {
      if(!free)
        trans.push_back(NULL);
      continue;
    }
    Transducer* t = free ? trans[0] : new Transducer();
    insertEntry(t, {.left=le.left, .right=re.right, .tags=tags});
    did_anything = true;
    if(!free)
    {
      applyMode(t, tok.mode);
      trans.push_back(t);
    }
  }
  if(free)
  {
    if(!did_anything)
    {
      trans[0] = NULL;
    }
    if(trans[0])
    {
      trans[0]->minimize();
      applyMode(trans[0], tok.mode);
    }
    lexiconTransducers[tok] = trans[0];
    return trans[0];
  }
  else
  {
    entryTransducers[tok] = trans;
    return trans[entry_index];
  }
}

void
LexdCompiler::encodeFlag(UnicodeString& str, int flag)
{
  UnicodeString letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int num = flag;
  while(num > 0)
  {
    str += letters[num % 26];
    num /= 26;
  }
}

trans_sym_t
LexdCompiler::getFlag(FlagDiacriticType type, string_ref flag, unsigned int value)
{
  //wcerr << L"getFlag(" << type << L", " << to_wstring(name(flag)) << L", " << value << L")" << endl;
  UnicodeString flagstr = "@";
  switch(type)
  {
    case Unification:
      //wcerr << "  Unification" << endl;
      flagstr += "U."; break;
    case Positive:
      //wcerr << "  Positive" << endl;
      flagstr += "P."; break;
    case Negative:
      //wcerr << "  Negative" << endl;
      flagstr += "N."; break;
    case Require:
      //wcerr << "  Require" << endl;
      flagstr += "R."; break;
    case Disallow:
      //wcerr << "  Disallow" << endl;
      flagstr += "D."; break;
    case Clear:
      //wcerr << "  Clear" << endl;
      flagstr += "C."; break;
  }
  encodeFlag(flagstr, (int)flag.i);
  if(type != Clear)
  {
    flagstr += ".";
    encodeFlag(flagstr, (int)(value + 1));
  }
  flagstr += "@";
  return alphabet_lookup(flagstr);
}

Transducer*
LexdCompiler::getLexiconTransducerWithFlags(pattern_element_t& tok, bool free)
{
  if(!free && entryTransducers.find(tok) != entryTransducers.end())
    return entryTransducers[tok][0];
  if(free && lexiconTransducers.find(tok) != lexiconTransducers.end())
    return lexiconTransducers[tok];

  // TODO: can this be abstracted from here and getLexiconTransducer()?
  vector<entry_t>& lents = lexicons[tok.left.name];
  if(tok.left.name.valid() && tok.left.part > lents[0].size())
    die(to_wstring(name(tok.left.name)) + L"(" + to_wstring(tok.left.part) + L") - part is out of range");
  vector<entry_t>& rents = lexicons[tok.right.name];
  if(tok.right.name.valid() && tok.right.part > rents[0].size())
    die(to_wstring(name(tok.right.name)) + L"(" + to_wstring(tok.right.part) + L") - part is out of range");
  if(tok.left.name.valid() && tok.right.name.valid() && lents.size() != rents.size())
    die(L"Cannot collate " + to_wstring(name(tok.left.name)) + L" with " + to_wstring(name(tok.right.name)) + L" - differing numbers of entries");
  unsigned int count = (tok.left.name.valid() ? lents.size() : rents.size());
  Transducer* trans = new Transducer();
  lex_seg_t empty;
  bool did_anything = false;
  for(unsigned int i = 0; i < count; i++)
  {
    lex_seg_t& le = (tok.left.name.valid() ? lents[i][tok.left.part-1] : empty);
    lex_seg_t& re = (tok.right.name.valid() ? rents[i][tok.right.part-1] : empty);
    tags_t tags = unionset(le.tags, re.tags);
    if(!tok.tag_filter.compatible(tags))
    {
      continue;
    }
    did_anything = true;
    lex_seg_t seg;
    if(!free && tok.left.name.valid())
    {
      trans_sym_t flag = getFlag(Unification, tok.left.name, i);
      seg.left.symbols.push_back(flag);
      seg.right.symbols.push_back(flag);
    }
    if(!free && tok.right.name.valid() && tok.right.name != tok.left.name)
    {
      trans_sym_t flag = getFlag(Unification, tok.right.name, i);
      seg.left.symbols.push_back(flag);
      seg.right.symbols.push_back(flag);
    }
    if(tok.left.name.valid())
    {
      seg.left.symbols.insert(seg.left.symbols.end(), le.left.symbols.begin(), le.left.symbols.end());
    }
    if(tok.right.name.valid())
    {
      seg.right.symbols.insert(seg.right.symbols.end(), re.right.symbols.begin(), re.right.symbols.end());
    }
    seg.tags.insert(tags.begin(), tags.end());
    insertEntry(trans, seg);
  }
  if(did_anything)
  {
    trans->minimize();
    applyMode(trans, tok.mode);
  }
  else
  {
    trans = NULL;
  }
  if(free)
  {
    lexiconTransducers[tok] = trans;
  }
  else
  {
    entryTransducers[tok] = vector<Transducer*>(1, trans);
  }
  return trans;
}

void
LexdCompiler::printStatistics() const
{
  wcerr << "Lexicons: " << lexicons.size() << endl;
  wcerr << "Lexicon entries: ";
  unsigned int x = 0;
  for(const auto &lex: lexicons)
    x += lex.second.size();
  wcerr << x << endl;
  x = 0;
  wcerr << "Patterns: " << patterns.size() << endl;
  wcerr << "Pattern entries: ";
  for(const auto &pair: patterns)
    x += pair.second.size();
  wcerr << x << endl;
  wcerr << endl;
  wcerr << "Counts for individual lexicons:" << endl;
  unsigned int anon = 0;
  for(const auto &lex: lexicons)
  {
	if(empty(lex.first)) continue;
	wstring n = to_wstring(name(lex.first));
	if(n[0] == L' ') anon += lex.second.size();
	else wcerr << n << ": " << lex.second.size() << endl;
  }
  wcerr << "All anonymous lexicons: " << anon << endl;
}
