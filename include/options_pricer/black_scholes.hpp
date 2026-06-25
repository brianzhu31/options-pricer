#pragma once

#include "options_pricer/option.hpp"

namespace options_pricer {

double normal_cdf(double x);
double black_scholes_price(const EuropeanOption& option);

}  // namespace options_pricer

