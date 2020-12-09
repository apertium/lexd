#include "lexdcompiler.h"

#include <lttoolbox/lt_locale.h>
#include <unicode/ustdio.h>
#include <libgen.h>
#include <getopt.h>

using namespace std;

void endProgram(char *name)
{
  if(name != NULL)
  {
    cout << basename(name) << ": compile lexd files to transducers" << endl;
    cout << "USAGE: " << basename(name) << " [-abcfmx] [rule_file [output_file]]" << endl;
    cout << "   -a, --align:      align labels (prefer a:0 b:b to a:b b:0)" << endl;
    cout << "   -b, --bin:        output as Lttoolbox binary file (default is AT&T format)" << endl;
    cout << "   -c, --compress:   condense labels (prefer a:b to 0:b a:0 - sets --align)" << endl;
    cout << "   -f, --flags:      compile using flag diacritics" << endl;
    cout << "   -m, --minimize:   do hyperminimization (sets -f)" << endl;
    cout << "   -t, --tags:       compile tags and filters with flag diacritics (sets -f)" << endl;
    cout << "   -x, --statistics: print lexicon and pattern sizes to stderr" << endl;
  }
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  bool bin = false;
  bool flags = false;
  bool single = false;
  bool stats = false;
  UFILE* input = u_finit(stdin, NULL, NULL);
  FILE* output = stdout;
  LexdCompiler comp;

  LtLocale::tryToSetLocale();

#if HAVE_GETOPT_LONG
  int option_index=0;
#endif

  while (true) {
#if HAVE_GETOPT_LONG
    static struct option long_options[] =
    {
      {"align",     no_argument, 0, 'a'},
      {"bin",       no_argument, 0, 'b'},
      {"compress",  no_argument, 0, 'c'},
      {"flags",     no_argument, 0, 'f'},
      {"help",      no_argument, 0, 'h'},
      {"minimize",  no_argument, 0, 'm'},
      {"single",    no_argument, 0, 's'},
      {"tags",      no_argument, 0, 't'},
      {"statistics",no_argument, 0, 'x'},
      {0, 0, 0, 0}
    };

    int cnt=getopt_long(argc, argv, "abcfhmstx", long_options, &option_index);
#else
    int cnt=getopt(argc, argv, "abcfhmstx");
#endif
    if (cnt==-1)
      break;

    switch (cnt)
    {
      case 'a':
        comp.setShouldAlign(true);
        break;

      case 'b':
        bin = true;
        break;

      case 'c':
        comp.setShouldAlign(true);
        comp.setShouldCompress(true);
        break;

      case 'f':
        flags = true;
        break;

      case 'm':
        flags = true;
        comp.setShouldHypermin(true);
        break;

      case 's':
        single = true;
        break;

      case 't':
        flags = true;
        comp.setTagsAsFlags(true);
        break;

      case 'x':
        stats = true;
        break;

      case 'h': // fallthrough
      default:
        endProgram(argv[0]);
        break;
    }
  }

  string infile;
  string outfile;
  switch(argc - optind)
  {
    case 0:
      break;

    case 1:
      infile = argv[argc-1];
      break;

    case 2:
      infile = argv[argc-2];
      outfile = argv[argc-1];
      break;

    default:
      endProgram(argv[0]);
      break;
  }

  if(infile != "" && infile != "-")
  {
    input = u_fopen(infile.c_str(), "rb", NULL, NULL);
    if(!input)
    {
      cerr << "Error: Cannot open file '" << infile << "' for reading." << endl;
      exit(EXIT_FAILURE);
    }
  }

  if(outfile != "" && outfile != "-")
  {
    output = fopen(outfile.c_str(), "wb");
    if(!output)
    {
      cerr << "Error: Cannot open file '" << outfile << "' for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  comp.readFile(input);
  u_fclose(input);
  Transducer* transducer = (single ? comp.buildTransducerSingleLexicon() : comp.buildTransducer(flags));
  if(stats)
    comp.printStatistics();
  if(!transducer)
    cerr << "Warning: output is empty transducer." << endl;
  else if(bin)
  {
    // TODO: finish this!
    //fwrite(HEADER_LTTOOLBOX, 1, 4, output);
    //uint64_t features = 0;
    //write_le(output, features);

    // letters
    //Compression::wstring_write(L"", output);
    //comp.alphabet.write(output);
    //Compression::multibyte_write(1, output);
    //Compression::wstring_write(L"main", output);
    transducer->write(output);
  }
  else
  {
    transducer->show(comp.alphabet, output, 0, true);
  }
  if(output != stdout) fclose(output);
  delete transducer;
  return 0;
}
