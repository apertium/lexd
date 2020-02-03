# Lexd Syntax

A Lexd rule file defines lexicons and patterns. Each lexicon consists of a list of entries which have an analysis side and a generation side, similar to lexicons in HFST Lexc. Patterns, meanwhile, replace Lexc's continuation lexicons. Each pattern consists of a list of lexicons which the compiler concatenates in that order.

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
