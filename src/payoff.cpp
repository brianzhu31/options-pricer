#include "options_pricer/payoff.hpp"

#include <algorithm>

namespace options_pricer {

double european_payoff(OptionType type, double spot_at_maturity, double strike) {
    if (type == OptionType::Call) {
        return std::max(spot_at_maturity - strike, 0.0);
    }

    return std::max(strike - spot_at_maturity, 0.0);
}

}  // namespace options_pricer

