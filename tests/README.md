# Lexd Test Data

The example files in this directory are adapted from the following repositories:

- `heb`
  - These were created specifically as test data, though the list of roots was extracted from https://github.com/apertium/apertium-heb
- `kik`
  - `.lexc` and `.twoc` files were copied from https://github.com/ksteimel/apertium-kik
- `lin`
  - `.lexc` and `.twoc` files were copied from https://github.com/apertium/apertium-lin
- `wad`
  - These files are a simplified version of https://github.com/apertium/apertium-wad

Timing data for each of these languages can be obtained by running the following command from this directory.

```bash
./timing.sh [repetitions] code
```

This will print each command run along with execution time and maximum memory usage. Specifying a number of repetitions will repeat each command and report the total time.
