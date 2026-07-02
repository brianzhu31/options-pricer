#include "options_pricer/black_scholes.hpp"
#include "options_pricer/monte_carlo.hpp"
#include "options_pricer/payoff.hpp"
#include "options_pricer/statistics.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using options_pricer::EuropeanOption;
using options_pricer::AsianOption;
using options_pricer::MonteCarloConfig;
using options_pricer::OptionType;

void require_close(
    const std::string& name,
    double actual,
    double expected,
    double tolerance
) {
    if (std::fabs(actual - expected) > tolerance) {
        std::cerr
            << name << " expected " << expected
            << " but got " << actual
            << " (tolerance " << tolerance << ")\n";
        std::exit(EXIT_FAILURE);
    }
}

void require_equal(const std::string& name, double actual, double expected) {
    if (actual != expected) {
        std::cerr << name << " expected " << expected << " but got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void test_european_payoffs() {
    require_equal(
        "in-the-money call payoff",
        options_pricer::european_payoff(OptionType::Call, 125.0, 100.0),
        25.0
    );
    require_equal(
        "out-of-the-money call payoff",
        options_pricer::european_payoff(OptionType::Call, 90.0, 100.0),
        0.0
    );
    require_equal(
        "in-the-money put payoff",
        options_pricer::european_payoff(OptionType::Put, 80.0, 100.0),
        20.0
    );
    require_equal(
        "out-of-the-money put payoff",
        options_pricer::european_payoff(OptionType::Put, 130.0, 100.0),
        0.0
    );
}

void test_black_scholes_reference_values() {
    const EuropeanOption call{
        .type = OptionType::Call,
        .spot = 100.0,
        .strike = 100.0,
        .rate = 0.05,
        .volatility = 0.2,
        .maturity = 1.0,
    };
    EuropeanOption put = call;
    put.type = OptionType::Put;

    require_close("Black-Scholes ATM call", options_pricer::black_scholes_price(call), 10.4506, 1e-4);
    require_close("Black-Scholes ATM put", options_pricer::black_scholes_price(put), 5.5735, 1e-4);
}

void test_monte_carlo_is_reproducible_for_fixed_seed() {
    const EuropeanOption option{
        .type = OptionType::Call,
        .spot = 100.0,
        .strike = 100.0,
        .rate = 0.05,
        .volatility = 0.2,
        .maturity = 1.0,
    };
    const MonteCarloConfig config{
        .paths = 10'000,
        .seed = 12345,
    };

    const auto first = options_pricer::price_european_monte_carlo(option, config);
    const auto second = options_pricer::price_european_monte_carlo(option, config);

    require_equal("fixed-seed Monte Carlo price", first.price, second.price);
    require_equal(
        "fixed-seed Monte Carlo paths",
        static_cast<double>(first.paths),
        static_cast<double>(config.paths)
    );
}

void test_asian_option_with_one_step_matches_european() {
    const EuropeanOption european{
        .type = OptionType::Put,
        .spot = 100.0,
        .strike = 105.0,
        .rate = 0.03,
        .volatility = 0.25,
        .maturity = 1.5,
    };
    const AsianOption asian{
        .type = european.type,
        .spot = european.spot,
        .strike = european.strike,
        .rate = european.rate,
        .volatility = european.volatility,
        .maturity = european.maturity,
        .steps = 1,
    };
    const MonteCarloConfig config{.paths = 20'000, .seed = 77};

    const auto european_result =
        options_pricer::price_european_monte_carlo(european, config);
    const auto asian_result =
        options_pricer::price_asian_monte_carlo(asian, config);

    require_equal("one-step Asian price", asian_result.price, european_result.price);
    require_equal(
        "one-step Asian standard error",
        asian_result.standard_error,
        european_result.standard_error
    );
}

void test_confidence_interval_and_european_convergence() {
    const EuropeanOption option{
        .type = OptionType::Call,
        .spot = 100.0,
        .strike = 100.0,
        .rate = 0.05,
        .volatility = 0.2,
        .maturity = 1.0,
    };
    const auto result = options_pricer::price_european_monte_carlo(
        option,
        MonteCarloConfig{.paths = 500'000, .seed = 42}
    );
    const double exact = options_pricer::black_scholes_price(option);

    if (result.standard_error <= 0.0 ||
        result.confidence_interval_low >= result.confidence_interval_high) {
        std::cerr << "Monte Carlo uncertainty statistics are invalid\n";
        std::exit(EXIT_FAILURE);
    }
    if (exact < result.confidence_interval_low ||
        exact > result.confidence_interval_high) {
        std::cerr << "Black-Scholes price is outside Monte Carlo 95% confidence interval\n";
        std::exit(EXIT_FAILURE);
    }
}

void test_more_paths_reduce_standard_error() {
    const EuropeanOption option{};
    const auto small = options_pricer::price_european_monte_carlo(
        option,
        MonteCarloConfig{.paths = 10'000, .seed = 123}
    );
    const auto large = options_pricer::price_european_monte_carlo(
        option,
        MonteCarloConfig{.paths = 100'000, .seed = 123}
    );
    if (large.standard_error >= small.standard_error) {
        std::cerr << "standard error did not decrease with more paths\n";
        std::exit(EXIT_FAILURE);
    }
}

void test_running_statistics_and_merge() {
    options_pricer::RunningStatistics first;
    first.add(1.0);
    first.add(2.0);
    options_pricer::RunningStatistics second;
    second.add(3.0);
    second.add(4.0);
    first.merge(second);

    require_equal("running statistics count", static_cast<double>(first.count()), 4.0);
    require_close("running statistics mean", first.mean(), 2.5, 1e-12);
    require_close(
        "running statistics sample variance",
        first.sample_variance(),
        5.0 / 3.0,
        1e-12
    );
}

void test_multithreaded_run_is_reproducible() {
    const EuropeanOption option{};
    const MonteCarloConfig config{
        .paths = 100'000,
        .seed = 9876,
        .threads = 4,
        .antithetic = true,
    };
    const auto first = options_pricer::price_european_monte_carlo(option, config);
    const auto second = options_pricer::price_european_monte_carlo(option, config);

    require_equal("multithreaded fixed-seed price", first.price, second.price);
    require_equal(
        "multithreaded fixed-seed standard error",
        first.standard_error,
        second.standard_error
    );
}

void test_antithetic_variates_reduce_standard_error() {
    const EuropeanOption option{
        .type = OptionType::Call,
        .spot = 100.0,
        .strike = 100.0,
        .rate = 0.05,
        .volatility = 0.2,
        .maturity = 1.0,
    };
    const auto plain = options_pricer::price_european_monte_carlo(
        option,
        MonteCarloConfig{
            .paths = 200'000,
            .seed = 314159,
            .threads = 1,
            .antithetic = false,
        }
    );
    const auto antithetic = options_pricer::price_european_monte_carlo(
        option,
        MonteCarloConfig{
            .paths = 200'000,
            .seed = 314159,
            .threads = 1,
            .antithetic = true,
        }
    );
    if (antithetic.standard_error >= plain.standard_error) {
        std::cerr << "antithetic variates did not reduce standard error\n";
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    test_european_payoffs();
    test_black_scholes_reference_values();
    test_monte_carlo_is_reproducible_for_fixed_seed();
    test_asian_option_with_one_step_matches_european();
    test_confidence_interval_and_european_convergence();
    test_more_paths_reduce_standard_error();
    test_running_statistics_and_merge();
    test_multithreaded_run_is_reproducible();
    test_antithetic_variates_reduce_standard_error();
    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}
