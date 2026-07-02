#include "options_pricer/monte_carlo.hpp"

#include "options_pricer/payoff.hpp"
#include "options_pricer/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

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
    if (config.threads == 0) {
        throw std::invalid_argument("threads must be positive");
    }
    if (config.antithetic && config.paths % 2 != 0) {
        throw std::invalid_argument("paths must be even when antithetic variates are enabled");
    }
}

std::uint64_t mix_seed(std::uint64_t seed, std::uint64_t stream) {
    std::uint64_t value = seed + 0x9e3779b97f4a7c15ULL * (stream + 1);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

MonteCarloResult make_result(
    const RunningStatistics& statistics,
    double discount,
    std::uint64_t paths
) {
    const double price = discount * statistics.mean();
    const double standard_error = discount * std::sqrt(
        statistics.sample_variance() / static_cast<double>(statistics.count())
    );
    constexpr double z_95 = 1.959963984540054;

    return MonteCarloResult{
        .price = price,
        .standard_error = standard_error,
        .confidence_interval_low = price - z_95 * standard_error,
        .confidence_interval_high = price + z_95 * standard_error,
        .paths = paths,
    };
}

template <typename Simulate>
RunningStatistics run_parallel(
    const MonteCarloConfig& config,
    Simulate simulate
) {
    const std::uint64_t samples = config.antithetic ? config.paths / 2 : config.paths;
    const std::uint32_t worker_count = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(config.threads, samples)
    );
    std::vector<RunningStatistics> partial(worker_count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    const std::uint64_t base_count = samples / worker_count;
    const std::uint64_t remainder = samples % worker_count;
    for (std::uint32_t worker = 0; worker < worker_count; ++worker) {
        const std::uint64_t count = base_count + (worker < remainder ? 1 : 0);
        workers.emplace_back([&, worker, count] {
            std::mt19937_64 rng(mix_seed(config.seed, worker));
            std::normal_distribution<double> normal(0.0, 1.0);
            for (std::uint64_t i = 0; i < count; ++i) {
                partial[worker].add(simulate(rng, normal, config.antithetic));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    RunningStatistics combined;
    for (const auto& statistics : partial) {
        combined.merge(statistics);
    }
    return combined;
}

}  // namespace

MonteCarloResult price_european_monte_carlo(
    const EuropeanOption& option,
    const MonteCarloConfig& config
) {
    validate_inputs(option, config);

    const double drift = (
        option.rate - 0.5 * option.volatility * option.volatility
    ) * option.maturity;
    const double diffusion_scale = option.volatility * std::sqrt(option.maturity);
    const double discount = std::exp(-option.rate * option.maturity);

    const auto statistics = run_parallel(
        config,
        [&](auto& rng, auto& normal, bool antithetic) {
            const double z = normal(rng);
            const auto payoff_for = [&](double normal_value) {
                const double terminal_spot =
                    option.spot * std::exp(drift + diffusion_scale * normal_value);
                return european_payoff(option.type, terminal_spot, option.strike);
            };
            const double payoff = payoff_for(z);
            return antithetic ? 0.5 * (payoff + payoff_for(-z)) : payoff;
        }
    );
    return make_result(statistics, discount, config.paths);
}

MonteCarloResult price_asian_monte_carlo(
    const AsianOption& option,
    const MonteCarloConfig& config
) {
    validate_inputs(option, config);
    if (option.steps == 0) {
        throw std::invalid_argument("steps must be positive");
    }

    const double dt = option.maturity / static_cast<double>(option.steps);
    const double drift = (
        option.rate - 0.5 * option.volatility * option.volatility
    ) * dt;
    const double diffusion_scale = option.volatility * std::sqrt(dt);
    const double discount = std::exp(-option.rate * option.maturity);

    const auto statistics = run_parallel(
        config,
        [&](auto& rng, auto& normal, bool antithetic) {
            double spot = option.spot;
            double antithetic_spot = option.spot;
            double spot_sum = 0.0;
            double antithetic_spot_sum = 0.0;
            for (std::uint64_t step = 0; step < option.steps; ++step) {
                const double z = normal(rng);
                spot *= std::exp(drift + diffusion_scale * z);
                spot_sum += spot;
                if (antithetic) {
                    antithetic_spot *= std::exp(drift - diffusion_scale * z);
                    antithetic_spot_sum += antithetic_spot;
                }
            }
            const double payoff = european_payoff(
                option.type,
                spot_sum / static_cast<double>(option.steps),
                option.strike
            );
            if (!antithetic) {
                return payoff;
            }
            const double antithetic_payoff = european_payoff(
                option.type,
                antithetic_spot_sum / static_cast<double>(option.steps),
                option.strike
            );
            return 0.5 * (payoff + antithetic_payoff);
        }
    );
    return make_result(statistics, discount, config.paths);
}

}  // namespace options_pricer
