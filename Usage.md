# Lexd Syntax

## Invocation

The `lexd` binary generates [AT&T format] transducers.

[AT&T Format]: https://wiki.apertium.org/wiki/ATT_format

Sample, save to `verb.lexd`:
```verb.lexd
PATTERNS
VerbRoot VerbInfl

LEXICON VerbRoot
sing
walk
dance

LEXICON VerbInfl
<v><pres>:
<v><pres><p3><sg>:s
```

Compile it (without flag diacritics) to ATT transducer format:
```
$ lexd verb.lexd > verb-generator.att
```

To compile to an `lttoolbox` transducer binary dictionary, use
`lt-comp`; this can be used for lookup with `lt-proc`:
```
$ lt-comp rl verb-generator.att verb-analyser.bin
main@standard 17 19
$ echo 'sings' | lt-proc verb-analyser.bin
^sings/sing<v><pres><p3><sg>$
```

To extract forms, use the [HFST] to first compile to `hfst` binary
format:

[HFST]: https://hfst.github.io/

```
$ hfst-txt2fst verb-generator.att -o verb-generator.hfst
```

Then you can use `hfst-fst2strings`:
```
$ hfst-fst2strings verb-generator.hfst
sing<v><pres>:sing
sing<v><pres><p3><sg>:sings
walk<v><pres>:walk
walk<v><pres><p3><sg>:walks
dance<v><pres>:dance
dance<v><pres><p3><sg>:dances
```

## Basic Syntax

A Lexd rule file defines lexicons and patterns. Each lexicon consists of a list of entries which have an analysis side and a generation side, similar to lexicons in HFST Lexc. Patterns, meanwhile, replace Lexc's continuation lexicons. Each pattern consists of a list of lexicons or named patterns which the compiler concatenates in that order.

```
PATTERNS
VerbRoot VerbInfl

LEXICON VerbRoot
sing
walk
dance

LEXICON VerbInfl
<v><pres>:
<v><pres><p3><sg>:s
```

forms generated:
```
sing/sing<v><pres>
sings/sing<v><pres><p3><sg>
walk/walk<v><pres>
walks/walk<v><pres><p3><sg>
dance/dance<v><pres>
dances/dance<v><pres><p3><sg>
```

Symbols enclosed in angle brackets or braces will be automatically interpreted as multicharacter symbols (presumably tags and archiphonemes, respectively):

```
PATTERNS
X

LEXICON X
x<ij>:x{i}
```

resulting ATT file:
```
0	1	x	x	0.000000	
1	2	<ij>	{i}	0.000000	
2	0.000000
```

Any character can be escaped with a backslash:

```
PATTERNS
X

LEXICON X
x\<ij>:x{i}
```

resulting ATT file:
```
0	1	x	x	0.000000	
1	2	<	{i}	0.000000	
2	3	i	@0@	0.000000	
3	4	j	@0@	0.000000	
4	5	>	@0@	0.000000	
5	0.000000
```

And comments begin with `#`.

## Alignment

Patterns can list different sides of each lexicon in different places. When the compiler encounters a one-sided lexicon reference in a pattern, it attaches all entries from that side of that lexicon to the transducer and then builds the rest of the pattern, attaching a separate copy for each entry. However, in these copies, for any subsequent mentions of that lexicon, only the corresponding segment of that entry will be attached, thus avoiding over-generation. The same lexicon can be mentioned arbitrarily many times, making it straightforward to write rules for phenomena such as reduplication.

```
PATTERNS
:VerbInfl VerbRoot VerbInfl:
:VerbInfl :VerbRoot VerbRoot VerbInfl: Redup:

LEXICON VerbRoot
bloop
vroom

LEXICON VerbInfl
<v><pres>:en

LEXICON Redup
<redup>:
```

forms generated:
```
enbloop/bloop<v><pres>
envroom/vroom<v><pres>
enbloopbloop/bloop<v><pres><redup>
envroomvroom/vroom<v><pres><redup>
```

To handle more complex cases, such as infixation and Semitic triliteral roots, lexicon entries can have multiple segments which patterns can refer to independently.

```
PATTERNS
C(1) :V(1) C(2) :V(2) C(3) V(2):

LEXICON C(3)
sh m r
y sh v

LEXICON V(2)
:a <v><p3><sg>:a
:o <v><pprs>:e
```

forms generated:
```
shamar/shmr<v><p3><sg>
shomer/shmr<v><pprs>
yashav/yshv<v><p3><sg>
yoshev/yshv<v><pprs>
```

It is also possible to give lexicons multiple names using the `ALIAS` command, which allows patterns to refer to multiple independent copies, which can then be used for productive compounding.

```
PATTERNS
NounStem NounInfl
NounStem NounInflComp Comp NounStem2 NounInfl

LEXICON Comp
<comp>+:

LEXICON NounStem
shoop
blarg

ALIAS NounStem NounStem2

LEXICON NounInfl
<n><sg>:
<n><pl>:ah

LEXICON NounInflComp
<n>:a
```

forms generated:
```
shoop/shoop<n><sg>
shoopah/shoop<n><pl>
shoopashoop/shoop<n><comp>+shoop<n><sg>
shoopashoopah/shoop<n><comp>+shoop<n><pl>
shoopablarg/shoop<n><comp>+blarg<n><sg>
shoopablargah/shoop<n><comp>+blarg<n><pl>
blarg/blarg<n><sg>
blargah/blarg<n><pl>
blargashoop/blarg<n><comp>+shoop<n><sg>
blargashoopah/blarg<n><comp>+shoop<n><pl>
blargablarg/blarg<n><comp>+blarg<n><sg>
blargablargah/blarg<n><comp>+blarg<n><pl>
```

Patterns can be named and included in other patterns. In addition to being less repetitive to write, it also compiles faster.

```
PATTERN VerbStem
VerbRoot
VerbRoot Causative
AuxRoot

PATTERNS
VerbStem Tense PersonNumber
```
This is equivalent to
```
PATTERNS
VerbRoot Tense PersonNumber
VerbRoot Causative Tense PersonNumber
AuxRoot Tense PersonNumber
```

## Pattern Operators

Some simple operators are supported to help write patterns concisely:
- the option quantifier `?` can be applied after to a single token
```
PATTERNS
Negation? Adjective
# equivalent to:
# Negation Adjective
# Adjective
```

Placing the `?` quantifier between a lexicon name and the segment number will that lexicon as a whole optional in that pattern:

```
PATTERNS
OptionalCircumfix?(1) Stem OptionalCircumfix?(2)
# equivalent to:
# OptionalCircumfix(1) Stem OptionalCircumfix(2)
# Stem
```

The quantifiers `*` (repeat 0 or more times) and `+` (repeat 1 or more times)
function similarly.

- the alternation operator `|` between two tokens causes one pattern
  for each alternate
```
PATTERNS
VerbStem Case

PATTERN Case
Absolutive
Oblique Ergative|Genitive
# equivalent to:
# Oblique Ergative
# Oblique Genitive
```

- the sieve operators `<` and `>` allow left and right extensions
```
PATTERNS
VerbStem > Nominalisation > Case
# equivalent to:
# VerbStem
# VerbStem Nominalisation
# VerbStem Nominalisation Case
```

## Anonymous Lexicons and Patterns

Patterns can contain anonymous lexicons to avoid needing to explicitly
declare lexicons for very simple things.

```
PATTERNS
NounStem [<n>:] NounNumber

LEXICON NounStem
sock
ninja

LEXICON NounNumber
<sg>:
<pl>:s
```

forms generated:
```
ninja/ninja<n><sg>
ninjas/ninja<n><pl>
sock/sock<n><sg>
socks/sock<n><pl>
```

Anonymous patterns function similarly:
```
PATTERNS
(VerbRoot Causative?) | AuxRoot Tense PersonNumber
# equivalent to:
# PATTERN VerbStem
# VerbRoot Causative?
# PATTERNS
# VerbStem|AuxRoot Tense PersonNumber
```

Anonymous patterns can be nested and both patterns and lexicons can be quantified:
```
PATTERNS
NounRoot ([<n>:] (Number Case)?) | (Verbalizer Tense)
```

## Tags
Lexicon entries can be tagged using square brackets:

```
LEXICON NounRoot
sock[count]
rice[mass]
sand[count,mass]
```

Or tags can be applied-by-default to an entire block:

```
LEXICON NounRoot[count]
sock
rice[mass,-count]
sand[mass]
```

When referring the lexicon, these tags can then be selected for:

```
PATTERNS
NounRoot[count] [<n>:] Number # 'sock' and 'sand', but not 'rice'
NounRoot[mass] [<n>:]         # 'rice' and 'sand', but not 'sock'
```

The absense of a tag can also be selected for:

```
PATTERNS
NounRoot[-count] [<n>:]       # 'rice' only
```

Tag selectors can also be applied to patterns:

```
PATTERN NounStem
NounRoot [<n>:]

PATTERNS
NounStem[count] Number
NounStem[mass]
```

Distribution rules are as follows:

```
(A B)[x]  = (A[x] B) | (A B[x])
(A B)[-x] = A[-x] B[-x]
```

Union and symmetric difference are implemented with the following
syntax:

```
A[|[x,y]] = A[x]    | A[y]    # union / logical or
A[^[x,y]] = A[x,-y] | A[-x,y] # symmetric difference / exclusive-or
```

These can be useful with lexically conditioned patterns. Here's an
example showing declension paradigms and noun class:

```
PATTERNS
(NounStem CaseEnding)[^[Decl1,Decl2],^[N,M,F]]

LEXICON NounStem
mensa:mens[Decl1,F]     # table
poeta:poet[Decl1,M]     # poet
dominus:domin[Decl2,M]  # master
bellum:bell[Decl2,N]    # war

LEXICON CaseEnding[Decl2]
<nom>:>us[M]
<nom>:>um[N]
<acc>:>um    # M or N

LEXICON CaseEnding[Decl1]
<nom>:>a     # any gender
<acc>:>am    # any gender
```

produces the forms (through `lexd | hfst-txt2fst | hfst-fst2strings`)

```
poeta<nom>:poet>a
poeta<acc>:poet>am
mensa<nom>:mens>a
mensa<acc>:mens>am
bellum<nom>:bell>um
bellum<acc>:bell>um
dominus<nom>:domin>us
dominus<acc>:domin>um
```

## Regular Expressions

If a lexicon entry begins with a forward slash, it is interpreted as a regular expression.

```
PATTERNS
SomeLexicon

LEXICON SomeLexicon
/x(y|zz)?[n-p]/
```

produces the forms

```
xn
xo
xp
xyn
xyo
xyp
xzzn
xzzo
xzzp
```

### Currently Supported Syntax

- Grouping with `()`
- Quantification with `?`, `*`, and `+`
  - Currently, quantifiers may only be applied to fully parenthesized groups, so `x+` is an error and must be written as `(x)+`.
- Alternation with `|`
- Character classes with `[]`
- Character ranges such as `[a-z]`
- Multichar symbols, following the same rules are normal lexicon entries
- Two-sided strings using `:`
  - `a:b` is the same as in a normal entry
  - `[ab]:c` is equivalent to `(a:c)|(b:c)`

## Weights

Weights can be added to lexicon entries with a double colon:

```
PATTERNS
[x::3.0]

LEXICON blah
y:y::2.0
z::1.0
```

Epsilon weights are also possible:

```
PATTERNS
AdjStem Substantive [::1.0] NounCases
NounStem NounCases
# noun reading more likely
```

Weights are currently not supported inside regular expressions, but such support is planned.
