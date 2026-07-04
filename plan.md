# 4-Phase Monte Carlo Options Pricer Plan

## Summary

Build a focused C++20 command-line Monte Carlo options pricer. Keep the scope tight: one executable, strong correctness checks, clean performance story, and reproducible benchmarks.

## Status

All four phases are implemented. Build instructions, CLI examples, measured
benchmark results, and project-summary bullets are in `README.md`.

## Key Changes

- Build a CLI-only C++20 project with CMake.
- Support:
  - European call/put options.
  - Arithmetic Asian call/put options.
  - Geometric Brownian motion under risk-neutral dynamics.
  - Black-Scholes closed-form validation for European options.
  - Standard error and 95% confidence interval output.
  - Fixed random seed for reproducible runs.
  - Antithetic variates.
  - Basic multithreading with configurable thread count.
- CLI examples:
  - `options-pricer price --kind european --type call --spot 100 --strike 100 --rate 0.05 --vol 0.2 --maturity 1 --paths 1000000`
  - `options-pricer price --kind asian --type call --steps 252 --antithetic --threads 8`
  - `options-pricer bench --paths 100000,1000000 --threads 1,2,4,8`

## Implementation Plan

- Phase 1:
  - Set up CMake project, CLI argument parsing, core pricing structs, payoff logic.
  - Implement Black-Scholes formula for European call/put.
  - Implement single-threaded Monte Carlo for European options.
  - Add unit tests for payoff logic and Black-Scholes reference values.

- Phase 2:
  - Add Asian option path simulation with configurable time steps.
  - Add standard error and 95% confidence interval reporting.
  - Add statistical tests showing European Monte Carlo converges near Black-Scholes.
  - Polish validation for required CLI arguments and invalid numeric inputs.

- Phase 3:
  - Add antithetic variates.
  - Add multithreaded simulation with deterministic per-thread seeding.
  - Add a reusable simulation accumulator for mean, variance, confidence intervals, and path counts.
  - Add reproducibility tests for fixed seeds.

- Phase 4:
  - Add benchmark command for paths/sec and thread scaling.
  - Compare plain Monte Carlo versus antithetic variates in benchmarks.
  - Polish CLI output, README examples, and benchmark table.
  - Add final project highlights explaining numerical methods, validation, and performance results.

## Test Plan

- Unit tests:
  - European call/put payoff correctness.
  - Asian average payoff correctness.
  - Black-Scholes values match known reference cases.
  - Running-statistics accumulator computes mean and variance correctly.
  - Invalid CLI inputs fail clearly.

- Statistical tests:
  - European Monte Carlo price lands near Black-Scholes within confidence interval.
  - Increasing path count reduces standard error.
  - Antithetic variates reduce standard error versus plain Monte Carlo on a fixed scenario.
  - Multithreaded and single-threaded results are reproducible for fixed seeds.

- CLI and benchmark tests:
  - Price command emits required fields.
  - Benchmark command reports paths/sec and elapsed time.
  - Invalid benchmark inputs fail clearly.

## Assumptions

- No reusable public API is required; CLI is the main product.
- Barrier, digital, and American options are out of scope to keep the project focused.
- CSV/JSON exports and parameter sweeps are deferred unless the core project finishes early.
- The priority is a polished, explainable options pricer rather than broad product coverage.
- Each phase should leave the project in a runnable state with tests passing.
