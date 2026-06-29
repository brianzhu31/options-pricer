#include "options_pricer/monte_carlo.hpp"

#include "options_pricer/payoff.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace options_pricer {
namespace {

template <typename Option>
void validate_inputs(const Option& option, const MonteCarloConfig& config) {
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

MonteCarloResult make_result(
    double payoff_sum,
    double payoff_square_sum,
    double discount,
    std::uint64_t paths
) {
    const double path_count = static_cast<double>(paths);
    const double mean = payoff_sum / path_count;
    double sample_variance = 0.0;
    if (paths > 1) {
        sample_variance = (
            payoff_square_sum - path_count * mean * mean
        ) / static_cast<double>(paths - 1);
        // Roundoff can make the computed variance very slightly negative.
        sample_variance = std::max(sample_variance, 0.0);
    }

    const double price = discount * mean;
    const double standard_error = discount * std::sqrt(sample_variance / path_count);
    constexpr double z_95 = 1.959963984540054;

    return MonteCarloResult{
        .price = price,
        .standard_error = standard_error,
        .confidence_interval_low = price - z_95 * standard_error,
        .confidence_interval_high = price + z_95 * standard_error,
        .paths = paths,
    };
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
    double payoff_square_sum = 0.0;
    for (std::uint64_t i = 0; i < config.paths; ++i) {
        const double z = standard_normal(rng);
        const double terminal_spot = option.spot * std::exp(drift + diffusion_scale * z);
        const double payoff = european_payoff(option.type, terminal_spot, option.strike);
        payoff_sum += payoff;
        payoff_square_sum += payoff * payoff;
    }

    return make_result(payoff_sum, payoff_square_sum, discount, config.paths);
}

MonteCarloResult price_asian_monte_carlo(
    const AsianOption& option,
    const MonteCarloConfig& config
) {
    validate_inputs(option, config);
    if (option.steps == 0) {
        throw std::invalid_argument("steps must be positive");
    }

    std::mt19937_64 rng(config.seed);
    std::normal_distribution<double> standard_normal(0.0, 1.0);

    const double dt = option.maturity / static_cast<double>(option.steps);
    const double drift = (
        option.rate - 0.5 * option.volatility * option.volatility
    ) * dt;
    const double diffusion_scale = option.volatility * std::sqrt(dt);
    const double discount = std::exp(-option.rate * option.maturity);

    double payoff_sum = 0.0;
    double payoff_square_sum = 0.0;
    for (std::uint64_t i = 0; i < config.paths; ++i) {
        double spot = option.spot;
        double spot_sum = 0.0;
        for (std::uint64_t step = 0; step < option.steps; ++step) {
            spot *= std::exp(drift + diffusion_scale * standard_normal(rng));
            spot_sum += spot;
        }
        const double average_spot = spot_sum / static_cast<double>(option.steps);
        const double payoff = european_payoff(option.type, average_spot, option.strike);
        payoff_sum += payoff;
        payoff_square_sum += payoff * payoff;
    }

    return make_result(payoff_sum, payoff_square_sum, discount, config.paths);
}

}  // namespace options_pricer
