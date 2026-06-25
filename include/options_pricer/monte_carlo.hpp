#pragma once

#include "options_pricer/option.hpp"

namespace options_pricer {

MonteCarloResult price_european_monte_carlo(
    const EuropeanOption& option,
    const MonteCarloConfig& config
);

}  // namespace options_pricer

