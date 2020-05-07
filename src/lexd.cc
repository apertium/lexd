#include "lexdcompiler.h"

#include <lttoolbox/lt_locale.h>
#include <getopt.h>

using namespace std;

void endProgram(char *name)
{
  if(name != NULL)
  {
    cout << basename(name) << ": compile lexd files to transducers" << endl;
    cout << "USAGE: " << basename(name) << " [-bf] [rule_file [output_file]]" << endl;
    cout << "   -b, --bin:      output as Lttoolbox binary file (default is AT&T format)" << endl;
    cout << "   -f, --flags:    compile using flag diacritics" << endl;
  }
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  bool bin = false;
  bool flags = false;
  FILE* input = stdin;
  FILE* output = stdout;

  LtLocale::tryToSetLocale();

#if HAVE_GETOPT_LONG
  int option_index=0;
#endif

  while (true) {
#if HAVE_GETOPT_LONG
    static struct option long_options[] =
    {
      {"bin",       no_argument, 0, 'b'},
      {"flags",     no_argument, 0, 'f'},
      {"help",      no_argument, 0, 'h'},
      {0, 0, 0, 0}
    };

    int cnt=getopt_long(argc, argv, "bfh", long_options, &option_index);
#else
    int cnt=getopt(argc, argv, "bfh");
#endif
    if (cnt==-1)
      break;

    switch (cnt)
    {
      case 'b':
        bin = true;
        break;

      case 'f':
        flags = true;
        break;

      case 'h':
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
    input = fopen(infile.c_str(), "rb");
    if(!input)
    {
      cerr << "Error: Cannot open file '" << infile << "' for reading." << endl;
      exit(EXIT_FAILURE);
    }
  }

  if(outfile != "" && outfile != "-")
  {
    output = fopen(outfile.c_str(), (bin ? "wb" : "w"));
    if(!output)
    {
      cerr << "Error: Cannot open file '" << outfile << "' for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  LexdCompiler comp;
  comp.readFile(input);
  Transducer* transducer = comp.buildTransducer(flags);
  if(bin)
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
  if(input != stdin) fclose(input);
  if(output != stdout) fclose(output);
  delete transducer;
  return 0;
}
