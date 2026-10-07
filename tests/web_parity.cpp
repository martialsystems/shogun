// Runs web/parity_scenario.txt through the web entry points natively and prints every output sample.
// web/test_wasm.mjs runs the same file through the wasm build and compares.
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void sg_init();
int sg_knob_count();
const char* sg_knob_name(int);
void sg_set_knob(int, int);
void sg_set_level(int, double);
void sg_set_master(double);
void sg_set_mode(int);
void sg_set_tempo(double);
void sg_set_scale(int);
void sg_set_running(int);
void sg_restart();
void sg_set_bar(int);
void sg_set_track(int, int, int, int, int);
void sg_set_drum(int, int, int, int, int, int);
void sg_set_note(int, int, int, int, int);
void sg_commit();
void sg_trigger(int, double, double);
void sg_trigger_note(int, int, double);
void sg_release(int);
void sg_patch(int, int);
float* sg_out_l();
float* sg_out_r();
void sg_process(int);
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  FILE* in = std::fopen(argv[1], "r");
  if (!in) return 2;
  sg_init();
  char line[256];
  while (std::fgets(line, sizeof line, in)) {
    char op[32] = {0}, fn[64] = {0};
    double a[6] = {0};
    if (line[0] == '#' || std::sscanf(line, "%31s", op) != 1) continue;
    if (!std::strcmp(op, "knob")) {
      char name[64];
      int cc;
      std::sscanf(line, "%*s %63s %d", name, &cc);
      for (int i = 0; i < sg_knob_count(); ++i)
        if (!std::strcmp(sg_knob_name(i), name)) sg_set_knob(i, cc);
    } else if (!std::strcmp(op, "call")) {
      std::sscanf(line, "%*s %63s %lf %lf %lf %lf %lf %lf", fn, a, a + 1, a + 2, a + 3, a + 4, a + 5);
      const int i0 = static_cast<int>(a[0]), i1 = static_cast<int>(a[1]), i2 = static_cast<int>(a[2]);
      const int i3 = static_cast<int>(a[3]), i4 = static_cast<int>(a[4]), i5 = static_cast<int>(a[5]);
      if (!std::strcmp(fn, "sg_set_master")) sg_set_master(a[0]);
      else if (!std::strcmp(fn, "sg_set_level")) sg_set_level(i0, a[1]);
      else if (!std::strcmp(fn, "sg_set_mode")) sg_set_mode(i0);
      else if (!std::strcmp(fn, "sg_set_tempo")) sg_set_tempo(a[0]);
      else if (!std::strcmp(fn, "sg_set_scale")) sg_set_scale(i0);
      else if (!std::strcmp(fn, "sg_set_running")) sg_set_running(i0);
      else if (!std::strcmp(fn, "sg_restart")) sg_restart();
      else if (!std::strcmp(fn, "sg_set_bar")) sg_set_bar(i0);
      else if (!std::strcmp(fn, "sg_set_track")) sg_set_track(i0, i1, i2, i3, i4);
      else if (!std::strcmp(fn, "sg_set_drum")) sg_set_drum(i0, i1, i2, i3, i4, i5);
      else if (!std::strcmp(fn, "sg_set_note")) sg_set_note(i0, i1, i2, i3, i4);
      else if (!std::strcmp(fn, "sg_commit")) sg_commit();
      else if (!std::strcmp(fn, "sg_trigger")) sg_trigger(i0, a[1], a[2]);
      else if (!std::strcmp(fn, "sg_trigger_note")) sg_trigger_note(i0, i1, a[2]);
      else if (!std::strcmp(fn, "sg_release")) sg_release(i0);
      else if (!std::strcmp(fn, "sg_patch")) sg_patch(i0, i1);
      else {
        std::fprintf(stderr, "unknown call %s\n", fn);
        return 2;
      }
    } else if (!std::strcmp(op, "process")) {
      int n = 0;
      std::sscanf(line, "%*s %d", &n);
      while (n > 0) {
        const int k = n > 1024 ? 1024 : n;
        sg_process(k);
        for (int i = 0; i < k; ++i) std::printf("%.9g %.9g\n", sg_out_l()[i], sg_out_r()[i]);
        n -= k;
      }
    }
  }
  return 0;
}
