#include "options_pricer/black_scholes.hpp"
#include "options_pricer/monte_carlo.hpp"
#include "options_pricer/payoff.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using options_pricer::EuropeanOption;
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

}  // namespace

int main() {
    test_european_payoffs();
    test_black_scholes_reference_values();
    test_monte_carlo_is_reproducible_for_fixed_seed();
    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}

