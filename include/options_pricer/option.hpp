#pragma once

#include <cstdint>

namespace options_pricer {

enum class OptionKind {
    European,
    Asian,
};

enum class OptionType {
    Call,
    Put,
};

struct EuropeanOption {
    OptionType type{OptionType::Call};
    double spot{100.0};
    double strike{100.0};
    double rate{0.05};
    double volatility{0.2};
    double maturity{1.0};
};

struct AsianOption {
    OptionType type{OptionType::Call};
    double spot{100.0};
    double strike{100.0};
    double rate{0.05};
    double volatility{0.2};
    double maturity{1.0};
    std::uint64_t steps{252};
};

struct MonteCarloConfig {
    std::uint64_t paths{100'000};
    std::uint64_t seed{42};
};

struct MonteCarloResult {
    double price{0.0};
    double standard_error{0.0};
    double confidence_interval_low{0.0};
    double confidence_interval_high{0.0};
    std::uint64_t paths{0};
};

}  // namespace options_pricer
