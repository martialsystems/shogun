#pragma once

// Shared helpers for the SHOGUN named tests (TESTPLAN.md style: named tests, printed numbers, tolerances).

#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

namespace tu {

extern int gFails;
extern int gChecks;

inline void check(bool ok, const char* test, const char* what, double got, double want, double tol) {
  ++gChecks;
  if (ok) return;
  std::printf("FAIL %s: %s got %.9g want %.9g (tol %.3g)\n", test, what, got, want, tol);
  ++gFails;
}
inline void near(const char* test, const char* what, double got, double want, double tol) {
  check(std::fabs(got - want) <= tol, test, what, got, want, tol);
}
inline void atMost(const char* test, const char* what, double got, double limit) {
  check(got <= limit, test, what, got, limit, 0.0);
}
inline void atLeast(const char* test, const char* what, double got, double limit) {
  check(got >= limit, test, what, got, limit, 0.0);
}
inline void truth(const char* test, const char* what, bool ok) { check(ok, test, what, ok ? 1 : 0, 1, 0); }

// In-place iterative radix-2 FFT (size must be a power of two).
inline void fft(std::vector<std::complex<double>>& a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2.0 * M_PI / static_cast<double>(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (size_t k = 0; k < len / 2; ++k) {
        const auto u = a[i + k];
        const auto v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}
// Magnitudes of the real FFT, bins 0..n/2.
inline std::vector<double> rfftMag(const std::vector<double>& x, const std::vector<double>* win = nullptr) {
  std::vector<std::complex<double>> a(x.size());
  for (size_t i = 0; i < x.size(); ++i) a[i] = x[i] * (win ? (*win)[i] : 1.0);
  fft(a);
  std::vector<double> m(x.size() / 2 + 1);
  for (size_t i = 0; i < m.size(); ++i) m[i] = std::abs(a[i]);
  return m;
}
inline std::vector<double> blackman(size_t n) {
  std::vector<double> w(n);
  for (size_t i = 0; i < n; ++i) {
    const double t = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n - 1);
    w[i] = 0.42 - 0.5 * std::cos(t) + 0.08 * std::cos(2.0 * t);
  }
  return w;
}
inline std::vector<double> hann(size_t n) {
  std::vector<double> w(n);
  for (size_t i = 0; i < n; ++i) w[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n - 1));
  return w;
}

// verify_dsp.py alias_db: Blackman window, worst non-harmonic peak re the largest, 20 Hz–band.
inline double aliasWorstDb(const std::vector<double>& y, double f, double fs, double band = 20000.0) {
  const size_t n = y.size();
  const auto w = blackman(n);
  auto Y = rfftMag(y, &w);
  double mx = 0.0;
  for (double v : Y) mx = v > mx ? v : mx;
  const double df = fs / static_cast<double>(n);
  double worst = 0.0;
  for (size_t k = 0; k < Y.size(); ++k) {
    const double fr = df * static_cast<double>(k);
    if (fr >= band || fr <= 20.0) continue;
    bool harm = false;
    for (int h = 1; h <= static_cast<int>(fs / 2.0 / f); ++h) {
      if (std::fabs(fr - h * f) < 3.0 * fs / static_cast<double>(n) * 4.0) {
        harm = true;
        break;
      }
    }
    if (!harm) worst = Y[k] / mx > worst ? Y[k] / mx : worst;
  }
  return 20.0 * std::log10(worst + 1e-300);
}

// verify_folder.py alias_db on one period of N samples at FS, after IDEAL decimation of an M·N render (bins ≤ N/2).
// Legit components sit on multiples of q bins; alias = everything else in (0, 20 kHz].
inline double aliasLinesDb(const std::vector<double>& yM, int M, int N, double FS, int q) {
  auto Y = rfftMag(yM);  // size M·N/2 + 1
  double al = 0.0, leg = 0.0;
  for (int k = 1; k <= N / 2; ++k) {
    if (k * FS / N > 20000.0) break;
    const double p = (Y[k] / M) * (Y[k] / M);
    if (k % q == 0) leg += p;
    else al += p;
  }
  return 10.0 * std::log10(al / leg + 1e-30);
}

// Least-squares amplitude of a sinusoid of known frequency over x[from..].
inline double sineAmp(const std::vector<double>& x, double f, double fs, size_t from) {
  double ss = 0, cc = 0, sc = 0, xs = 0, xc = 0;
  for (size_t i = from; i < x.size(); ++i) {
    const double ph = 2.0 * M_PI * f * static_cast<double>(i) / fs;
    const double s = std::sin(ph), c = std::cos(ph);
    ss += s * s;
    cc += c * c;
    sc += s * c;
    xs += x[i] * s;
    xc += x[i] * c;
  }
  const double det = ss * cc - sc * sc;
  const double a = (xs * cc - xc * sc) / det;
  const double b = (xc * ss - xs * sc) / det;
  return std::sqrt(a * a + b * b);
}

inline double rms(const std::vector<double>& x, size_t from = 0, size_t to = 0) {
  if (to == 0 || to > x.size()) to = x.size();
  double s = 0.0;
  for (size_t i = from; i < to; ++i) s += x[i] * x[i];
  return std::sqrt(s / static_cast<double>(to - from));
}
inline double peakAbs(const std::vector<double>& x, size_t from = 0, size_t to = 0) {
  if (to == 0 || to > x.size()) to = x.size();
  double p = 0.0;
  for (size_t i = from; i < to; ++i) p = std::fabs(x[i]) > p ? std::fabs(x[i]) : p;
  return p;
}
inline double db(double r) { return 20.0 * std::log10(r); }

}  // namespace tu
