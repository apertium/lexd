# Apertium-recursive

A recursive structural transfer module for Apertium

Compiling
---------

```bash
./autogen.sh
make
```

Running
-------

```bash
# compile the rules file
src/rtx-comp rule-file bytecode-file

# run the rules
src/rtx-proc bytecode-file < input

# decompile the rules and examine the bytecode
src/rtx-decomp bytecode-file text-file

# compile XML rule files
src/trx-comp bytecode-file xml-files...

# generate random sentences from a rules file
apertium-recursive/src/randsen.py start_node pair_directory source_language_directory
```

Options for ```rtx-comp```:
 - ```-e``` don't compile a rule with a particular name
 - ```-l``` load lexicalized weights from a file
 - ```-s``` output summaries of the rules to stderr

Options for ```trx-comp```:
 - ```-l``` load lexicalized weights from a file

Options for ```rtx-proc```:
 - ```-a``` indicates that the input comes from apertium-anaphora
 - ```-f``` trace which parse branches are discarded
 - ```-r``` print which rules are applying
 - ```-s``` trace the execution of the bytecode interpreter
 - ```-t``` mimic the behavior of apertium-transfer and apertium-interchunk
 - ```-T``` print the parse tree rather than applying output rules
 - ```-b``` print both the parse tree and the output
 - ```-m``` set the mode of tree output, available modes are:
   - ```nest``` (default) print the tree as text indented with tabs
   - ```flat``` print the tree as text
   - ```latex``` print the tree as LaTeX source using the ```forest``` library
   - ```dot``` print the tree as a Dot graph
   - ```box``` print the tree using [box-drawing characters](https://en.wikipedia.org/wiki/Box-drawing_character)
 - ```-e``` a combination of ```-f``` and ```-r```
   - Intended use: ```rtx-proc -e -m latex rules.bin < input.txt 2> trace.tex```
 - ```-F``` filter branches for things besides parse errors (experimental)

Testing
-------

```bash
make test
```

Using in a Pair
---------------

In ```Makefile.am``` add:
```
$(PREFIX1).rtx.bin: $(BASENAME).$(PREFIX1).rtx
	rtx-comp $< $@

$(PREFIX2).rtx.bin: $(BASENAME).$(PREFIX2).rtx
	rtx-comp $< $@
```

and add

```
$(PREFIX1).rtx.bin \
$(PREFIX2).rtx.bin
```

to ```TARGETS_COMMON```.

In ```modes.xml```, replace ```apertium-transfer```, ```apertium-interchunk```, and ```apertium-postchunk``` with:
```
<program name="rtx-proc">
  <file name="abc-xyz.rtx.bin"/>
</program>
```

Documentation
-------------

 - GSoC project proposal: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer
 - File format documentation: http://wiki.apertium.org/wiki/Apertium-recursive/Formalism
 - Bytecode documentation: http://wiki.apertium.org/wiki/Apertium-recursive/Bytecode
 - Progress reports: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Progress and https://github.com/apertium/apertium-recursive/issues/1
 - Examples of functioning rule sets can be found in [apertium-eng-kir](https://github.com/apertium/apertium-eng-kir/blob/rtx/apertium-eng-kir.kir-eng.rtx), [`eng-spa.rtx`](eng-spa.rtx), and [`tests/`](tests/)
