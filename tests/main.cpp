// SHOGUN named tests. Every test prints its numbers; a failure prints FAIL with got/want/tolerance.
#include "testutil.h"

#include <cstdio>

int tu::gFails = 0;
int tu::gChecks = 0;

void runBlockTests();
void runVoiceTests();
void runEngineTests();
void runModTests();

int main() {
  runBlockTests();
  runVoiceTests();
  runEngineTests();
  runModTests();
  if (tu::gFails != 0) {
    std::printf("%d of %d checks failed\n", tu::gFails, tu::gChecks);
    return 1;
  }
  std::printf("all %d named checks passed\n", tu::gChecks);
  return 0;
}
