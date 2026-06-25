#include "options_pricer/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

namespace options_pricer {
namespace {

constexpr double kSqrtTwo = 1.41421356237309504880;

void validate_option(const EuropeanOption& option) {
    if (option.spot <= 0.0) {
        throw std::invalid_argument("spot must be positive");
    }
    if (option.strike <= 0.0) {
        throw std::invalid_argument("strike must be positive");
    }
    if (option.volatility <= 0.0) {
        throw std::invalid_argument("volatility must be positive");
    }
    if (option.maturity <= 0.0) {
        throw std::invalid_argument("maturity must be positive");
    }
}

}  // namespace

double normal_cdf(double x) {
    return 0.5 * std::erfc(-x / kSqrtTwo);
}

double black_scholes_price(const EuropeanOption& option) {
    validate_option(option);

    const double sqrt_t = std::sqrt(option.maturity);
    const double sigma_sqrt_t = option.volatility * sqrt_t;
    const double d1 = (
        std::log(option.spot / option.strike) +
        (option.rate + 0.5 * option.volatility * option.volatility) * option.maturity
    ) / sigma_sqrt_t;
    const double d2 = d1 - sigma_sqrt_t;
    const double discounted_strike = option.strike * std::exp(-option.rate * option.maturity);

    if (option.type == OptionType::Call) {
        return option.spot * normal_cdf(d1) - discounted_strike * normal_cdf(d2);
    }

    return discounted_strike * normal_cdf(-d2) - option.spot * normal_cdf(-d1);
}

}  // namespace options_pricer

