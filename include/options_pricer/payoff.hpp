#pragma once

#include "options_pricer/option.hpp"

namespace options_pricer {

double european_payoff(OptionType type, double spot_at_maturity, double strike);

}  // namespace options_pricer

