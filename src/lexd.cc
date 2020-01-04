#include "lexdcompiler.h"

#include <lttoolbox/lt_locale.h>

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();
  string infile, outfile;
  LexdCompiler comp;
  if(argc == 4)
  {
    string flag = argv[1];
    if(flag != "-f")
    {
      wcerr << L"Usage: lexd [-f] rule_file att_file" << endl;
      exit(EXIT_FAILURE);
    }
    infile = argv[2];
    outfile = argv[3];
    comp.setUsingFlags(true);
  }
  else if(argc == 3)
  {
    infile = argv[1];
    outfile = argv[2];
  }
  else
  {
    wcerr << L"Usage: lexd [-f] rule_file att_file" << endl;
    exit(EXIT_FAILURE);
  }
  comp.process(infile, outfile);
  return 0;
}
