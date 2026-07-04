# Options Pricer

A focused C++20 command-line Monte Carlo pricer for European and arithmetic
Asian call and put options. It includes Black-Scholes validation, confidence
intervals, deterministic random seeds, antithetic variates, multithreading,
and a built-in performance benchmark.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Price an option

```sh
./build/options-pricer price \
  --kind european --type call \
  --spot 100 --strike 100 --rate 0.05 --vol 0.2 --maturity 1 \
  --paths 1000000 --seed 42 --threads 4 --antithetic
```

Asian options require `--steps`:

```sh
./build/options-pricer price \
  --kind asian --type call \
  --spot 100 --strike 100 --rate 0.05 --vol 0.2 --maturity 1 \
  --steps 252 --paths 1000000 --seed 42 --threads 4 --antithetic
```

The price command reports the Monte Carlo estimate, standard error, 95%
confidence interval, and run configuration. European results also include the
Black-Scholes closed-form price.

## Benchmark

```sh
./build/options-pricer bench \
  --paths 100000,1000000 \
  --threads 1,2,4,8 \
  --seed 42
```

Each path/thread combination runs both plain and antithetic Monte Carlo. The
table reports price, standard error, elapsed milliseconds, and paths per
second. Antithetic throughput counts both members of each path pair.

Benchmark timings depend on hardware, compiler, build type, and other system
load. Use a Release build and repeat runs when making performance comparisons.

### Sample results

One run on Darwin arm64, compiled with Apple Clang 17 and `-O3`:

| Mode | Paths | Threads | Standard error | Elapsed (ms) | Paths/sec |
|---|---:|---:|---:|---:|---:|
| Plain | 1,000,000 | 1 | 0.014701 | 32.825 | 30,465,010 |
| Plain | 1,000,000 | 2 | 0.014705 | 16.884 | 59,227,819 |
| Plain | 1,000,000 | 4 | 0.014702 | 11.967 | 83,560,220 |
| Plain | 1,000,000 | 8 | 0.014730 | 7.505 | 133,245,232 |
| Antithetic | 1,000,000 | 1 | 0.010396 | 18.984 | 52,675,591 |
| Antithetic | 1,000,000 | 8 | 0.010397 | 5.505 | 181,650,304 |

In this run, eight-thread plain simulation delivered about 4.4x the
single-thread throughput. Antithetic sampling reduced standard error by about
29% while using half as many normal draws for the same reported path count.

## Numerical method

The simulator evolves geometric Brownian motion under risk-neutral dynamics:

```text
S(t + dt) = S(t) * exp((r - 0.5 * sigma^2) * dt + sigma * sqrt(dt) * Z)
```

European options use the terminal price. Arithmetic Asian options average the
simulated prices at each monitoring step. Payoffs are discounted at the
risk-free rate. Online mean and variance accumulation avoids storing every
payoff and allows thread-local results to be merged efficiently.

## Project highlights

- Built a reproducible C++20 Monte Carlo engine for European and arithmetic
  Asian options with online error estimates and 95% confidence intervals.
- Validated European estimates against Black-Scholes and tested convergence,
  payoff correctness, fixed-seed reproducibility, and variance reduction.
- Added antithetic variates and deterministic multithreaded simulation, plus a
  benchmark harness that measured up to 133 million plain paths/sec and 4.4x
  thread scaling on an 8-thread arm64 test run.
