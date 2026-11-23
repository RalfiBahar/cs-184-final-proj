// Owner D evaluation: ratio-tracking transmittance vs. exact optical depth.
//
// For each fixture we draw N independent estimates and report the sample mean,
// its standard error, and the exact transmittance exp(-tau). Two estimators are
// compared against the same exact answer:
//   ratio_tracking        - ratio_tracking_transmittance(); mean of estimates
//   delta_tracking_escape - fraction of delta-tracked rays that escape
// (escape probability of delta tracking equals exp(-tau) for a valid majorant).
//
// Usage: eval_transmittance <out_dir> [trials=200000] [base_seed=184]
// Writes <out_dir>/transmittance.csv and <out_dir>/timing.csv, prints a table,
// and exits nonzero if any check fails. Single-threaded.

#include "volume/transmittance.h"
#include "test_media.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace CGL;
using namespace CGL::Volume;

namespace {

struct Fixture {
  std::string name;
  std::shared_ptr<const Medium> medium;
  Ray ray;
  Interval segment;
  double tau_exact;   // exact optical depth along segment
};

struct Row {
  std::string fixture, estimator;
  uint64_t trials;
  double mean, standard_error, analytic, min_estimate, max_estimate;
  double total_ms, mean_candidates;
  bool pass;
};

const BBox kBox(Vector3D(0, 0, 0), Vector3D(1, 1, 1));

Fixture homogeneous(const std::string& name, double sigma, Ray ray, double endpoint = INF_D,
                    bool force_empty = false) {
  auto medium = std::make_shared<HomogeneousMedium>(kBox, 1.0, sigma, 0.5);
  Fixture f{name, medium, ray, Interval{}, 0.0};
  medium->clip(ray, endpoint, f.segment);
  if (force_empty) f.segment = Interval{0.5, 0.5};
  f.tau_exact = f.segment.empty() ? 0.0 : sigma * (f.segment.end - f.segment.begin);
  return f;
}

Fixture ramp(const std::string& name, double a, double b, double scale, Ray ray,
             double endpoint = INF_D) {
  auto medium = std::make_shared<LinearRampMedium>(kBox, a, b, scale);
  Fixture f{name, medium, ray, Interval{}, 0.0};
  medium->clip(ray, endpoint, f.segment);
  f.tau_exact = medium->exact_optical_depth(ray, f.segment);
  return f;
}

std::vector<Fixture> make_fixtures() {
  const Vector3D oblique = Vector3D(0.6, 0.0, 0.8);  // already unit length
  std::vector<Fixture> list;
  list.push_back(homogeneous("vacuum_density_zero", 0.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(homogeneous("zero_length_segment", 1.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1)),
                             INF_D, true));
  list.push_back(homogeneous("homog_tau_0.25", 0.25, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(homogeneous("homog_tau_1", 1.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(homogeneous("homog_tau_4", 4.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(homogeneous("homog_camera_inside", 2.0, Ray(Vector3D(.5, .5, .25), Vector3D(0, 0, 1))));
  list.push_back(homogeneous("homog_oblique_ray", 2.0, Ray(Vector3D(.1, .5, -.2), oblique)));
  list.push_back(homogeneous("homog_blocker_inside", 2.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1)), 1.4));
  list.push_back(ramp("ramp_up_a0.2_b2", 0.2, 2.0, 1.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(ramp("ramp_down_a0.2_b2", 0.2, 2.0, 1.0, Ray(Vector3D(.5, .5, 2), Vector3D(0, 0, -1))));
  list.push_back(ramp("ramp_oblique", 0.2, 2.0, 1.5, Ray(Vector3D(.1, .5, -.2), oblique)));
  list.push_back(ramp("ramp_steep_a0_b6", 0.0, 6.0, 1.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1))));
  list.push_back(ramp("ramp_blocker_inside", 0.2, 2.0, 1.0, Ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1)), 1.6));
  return list;
}

bool within(double mean, double analytic, double se) {
  if (se == 0.0) return std::abs(mean - analytic) <= 1e-12;
  return std::abs(mean - analytic) <= 5.0 * se;
}

int failures = 0;
void expect(bool good, const std::string& message) {
  std::cout << (good ? "  [ok]   " : "  [FAIL] ") << message << "\n";
  if (!good) ++failures;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: eval_transmittance <out_dir> [trials] [base_seed]\n";
    return 2;
  }
  const std::string out_dir = argv[1];
  const uint64_t trials = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200000;
  const uint64_t base_seed = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 184;
  if (trials < 2) { std::cerr << "trials must be >= 2\n"; return 2; }

  std::cout << std::setprecision(8);
  std::vector<Fixture> fixtures = make_fixtures();
  std::vector<Row> rows;

  // Sanity: the Simpson reference agrees with the closed form (independent code path).
  std::cout << "Reference checks (Simpson quadrature vs exact optical depth)\n";
  for (const Fixture& f : fixtures) {
    const double tau_q = optical_depth_quadrature(*f.medium, f.ray, f.segment);
    // Boundary samples can land a rounding error outside the box; allow a small slack.
    expect(std::abs(tau_q - f.tau_exact) <= 2e-3 * std::max(1.0, f.tau_exact),
           f.name + ": quadrature " + std::to_string(tau_q) + " vs exact " +
               std::to_string(f.tau_exact));
  }

  std::cout << "\nStochastic estimators, " << trials << " trials each, base seed " << base_seed << "\n";
  for (size_t i = 0; i < fixtures.size(); ++i) {
    const Fixture& f = fixtures[i];
    const double analytic = std::exp(-f.tau_exact);

    // --- ratio tracking: mean of estimates ---
    {
      RNG rng(base_seed * 1000 + 2 * i);
      TransmittanceStats stats;
      double sum = 0.0, sum_sq = 0.0, lo = 1e300, hi = -1e300;
      const auto start = std::chrono::steady_clock::now();
      for (uint64_t k = 0; k < trials; ++k) {
        const double v = ratio_tracking_transmittance(*f.medium, f.ray, f.segment, rng, &stats);
        sum += v; sum_sq += v * v; lo = std::min(lo, v); hi = std::max(hi, v);
      }
      const double ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count();
      const double mean = sum / trials;
      const double var = std::max(0.0, (sum_sq - trials * mean * mean) / (trials - 1));
      const double se = std::sqrt(var / trials);
      const bool bounded = lo >= 0.0 && hi <= 1.0;
      rows.push_back({f.name, "ratio_tracking", trials, mean, se, analytic, lo, hi, ms,
                      double(stats.candidates) / trials, within(mean, analytic, se) && bounded});
    }
    // --- delta tracking: escape fraction ---
    {
      RNG rng(base_seed * 1000 + 2 * i + 1);
      uint64_t escapes = 0;
      const auto start = std::chrono::steady_clock::now();
      for (uint64_t k = 0; k < trials; ++k)
        if (!f.medium->sample_collision(f.ray, f.segment, rng).occurred) ++escapes;
      const double ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count();
      const double p = double(escapes) / trials;
      const double se = std::sqrt(p * (1.0 - p) / trials);
      // Bernoulli SE is 0 only if every trial agreed; exact fixtures (tau=0, p=1) land here.
      rows.push_back({f.name, "delta_tracking_escape", trials, p, se, analytic, 0.0, 1.0, ms,
                      0.0, within(p, analytic, se)});
    }
  }

  std::cout << std::left << std::setw(24) << "fixture" << std::setw(23) << "estimator"
            << std::right << std::setw(13) << "mean" << std::setw(13) << "std_error"
            << std::setw(13) << "analytic" << std::setw(9) << "z" << "  result\n";
  for (const Row& r : rows) {
    const double z = r.standard_error > 0 ? (r.mean - r.analytic) / r.standard_error : 0.0;
    std::cout << std::left << std::setw(24) << r.fixture << std::setw(23) << r.estimator
              << std::right << std::setw(13) << r.mean << std::setw(13) << r.standard_error
              << std::setw(13) << r.analytic << std::setw(9) << std::setprecision(3) << z
              << std::setprecision(8) << (r.pass ? "  pass" : "  FAIL") << "\n";
    if (!r.pass) ++failures;
  }

  // Negative test: an invalid (too small) majorant must be reported, not clamped.
  std::cout << "\nMajorant violation is reported\n";
  {
    LinearRampMedium bad(kBox, 0.2, 2.0, 1.0, 0.5, /*majorant_override=*/0.5);
    Ray ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1));
    Interval seg; bad.clip(ray, INF_D, seg);
    RNG rng(7);
    bool threw = false;
    try { for (int k = 0; k < 1000; ++k) ratio_tracking_transmittance(bad, ray, seg, rng); }
    catch (const std::exception&) { threw = true; }
    expect(threw, "ratio tracking throws when sigma_t exceeds the majorant");
  }
  // Vacuum / empty edge cases return exactly 1 without consuming randomness.
  {
    HomogeneousMedium vac(kBox, 0.0, 1.0, 0.5);
    Ray ray(Vector3D(.5, .5, -1), Vector3D(0, 0, 1));
    Interval seg; vac.clip(ray, INF_D, seg);
    RNG rng(1);
    expect(ratio_tracking_transmittance(vac, ray, seg, rng) == 1.0, "vacuum returns exactly 1");
    expect(ratio_tracking_transmittance(vac, ray, Interval{}, rng) == 1.0, "empty segment returns exactly 1");
  }

  std::ofstream csv(out_dir + "/transmittance.csv");
  std::ofstream timing(out_dir + "/timing.csv");
  if (!csv || !timing) { std::cerr << "cannot write into " << out_dir << "\n"; return 2; }
  csv << std::setprecision(12)
      << "fixture,estimator,trials,mean,std_error,analytic,abs_error,z_score,min_estimate,max_estimate,pass\n";
  timing << std::setprecision(12)
         << "fixture,estimator,trials,total_ms,ns_per_estimate,mean_candidates_per_estimate\n";
  for (const Row& r : rows) {
    const double z = r.standard_error > 0 ? (r.mean - r.analytic) / r.standard_error : 0.0;
    csv << r.fixture << ',' << r.estimator << ',' << r.trials << ',' << r.mean << ','
        << r.standard_error << ',' << r.analytic << ',' << std::abs(r.mean - r.analytic) << ','
        << z << ',' << r.min_estimate << ',' << r.max_estimate << ',' << (r.pass ? 1 : 0) << '\n';
    timing << r.fixture << ',' << r.estimator << ',' << r.trials << ',' << r.total_ms << ','
           << (r.total_ms * 1e6 / r.trials) << ',' << r.mean_candidates << '\n';
  }

  std::cout << "\nWrote " << out_dir << "/transmittance.csv and timing.csv\n";
  std::cout << (failures ? "RESULT: FAIL (" + std::to_string(failures) + " failing checks)"
                         : std::string("RESULT: all checks passed")) << "\n";
  return failures ? 1 : 0;
}
