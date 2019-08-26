#include "lexdcompiler.h"

#include <lttoolbox/lt_locale.h>

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();
  if(argc != 3)
  {
    wcerr << L"Usage: lexd rule_file att_file" << endl;
    exit(EXIT_FAILURE);
  }
  LexdCompiler comp;
  comp.process(argv[1], argv[2]);
  return 0;
}
