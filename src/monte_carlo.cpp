#include "options_pricer/monte_carlo.hpp"

#include "options_pricer/payoff.hpp"

#include <cmath>
#include <random>
#include <stdexcept>

namespace options_pricer {
namespace {

void validate_inputs(const EuropeanOption& option, const MonteCarloConfig& config) {
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
    if (config.paths == 0) {
        throw std::invalid_argument("paths must be positive");
    }
}

}  // namespace

MonteCarloResult price_european_monte_carlo(
    const EuropeanOption& option,
    const MonteCarloConfig& config
) {
    validate_inputs(option, config);

    std::mt19937_64 rng(config.seed);
    std::normal_distribution<double> standard_normal(0.0, 1.0);

    const double drift = (
        option.rate - 0.5 * option.volatility * option.volatility
    ) * option.maturity;
    const double diffusion_scale = option.volatility * std::sqrt(option.maturity);
    const double discount = std::exp(-option.rate * option.maturity);

    double payoff_sum = 0.0;
    for (std::uint64_t i = 0; i < config.paths; ++i) {
        const double z = standard_normal(rng);
        const double terminal_spot = option.spot * std::exp(drift + diffusion_scale * z);
        payoff_sum += european_payoff(option.type, terminal_spot, option.strike);
    }

    return MonteCarloResult{
        .price = discount * payoff_sum / static_cast<double>(config.paths),
        .paths = config.paths,
    };
}

}  // namespace options_pricer

