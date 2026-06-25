#include "options_pricer/black_scholes.hpp"
#include "options_pricer/monte_carlo.hpp"
#include "options_pricer/option.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using options_pricer::EuropeanOption;
using options_pricer::MonteCarloConfig;
using options_pricer::OptionType;

struct PriceCommand {
    EuropeanOption option;
    MonteCarloConfig monte_carlo;
};

void print_usage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  options-pricer price --kind european --type call|put "
        << "--spot N --strike N --rate N --vol N --maturity N --paths N [--seed N]\n\n"
        << "Example:\n"
        << "  options-pricer price --kind european --type call --spot 100 --strike 100 "
        << "--rate 0.05 --vol 0.2 --maturity 1 --paths 100000\n";
}

double parse_double(std::string_view name, const std::string& value) {
    std::size_t parsed = 0;
    const double result = std::stod(value, &parsed);
    if (parsed != value.size()) {
        throw std::invalid_argument(std::string(name) + " must be a number");
    }
    if (!std::isfinite(result)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
    return result;
}

std::uint64_t parse_uint64(std::string_view name, const std::string& value) {
    if (value.empty() || value[0] == '-') {
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    }
    std::size_t parsed = 0;
    const unsigned long long result = std::stoull(value, &parsed);
    if (parsed != value.size()) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
    return static_cast<std::uint64_t>(result);
}

std::optional<std::string> value_after(
    const std::vector<std::string>& args,
    std::size_t& index,
    std::string_view flag
) {
    if (args[index] != flag) {
        return std::nullopt;
    }
    if (index + 1 >= args.size()) {
        throw std::invalid_argument(std::string(flag) + " requires a value");
    }
    ++index;
    return args[index];
}

OptionType parse_option_type(const std::string& value) {
    if (value == "call") {
        return OptionType::Call;
    }
    if (value == "put") {
        return OptionType::Put;
    }
    throw std::invalid_argument("--type must be call or put");
}

PriceCommand parse_price_command(const std::vector<std::string>& args) {
    PriceCommand command;
    bool saw_kind = false;
    bool saw_type = false;

    for (std::size_t i = 2; i < args.size(); ++i) {
        if (auto value = value_after(args, i, "--kind")) {
            if (*value != "european") {
                throw std::invalid_argument("--kind currently supports only european");
            }
            saw_kind = true;
        } else if (auto value = value_after(args, i, "--type")) {
            command.option.type = parse_option_type(*value);
            saw_type = true;
        } else if (auto value = value_after(args, i, "--spot")) {
            command.option.spot = parse_double("--spot", *value);
        } else if (auto value = value_after(args, i, "--strike")) {
            command.option.strike = parse_double("--strike", *value);
        } else if (auto value = value_after(args, i, "--rate")) {
            command.option.rate = parse_double("--rate", *value);
        } else if (auto value = value_after(args, i, "--vol")) {
            command.option.volatility = parse_double("--vol", *value);
        } else if (auto value = value_after(args, i, "--maturity")) {
            command.option.maturity = parse_double("--maturity", *value);
        } else if (auto value = value_after(args, i, "--paths")) {
            command.monte_carlo.paths = parse_uint64("--paths", *value);
        } else if (auto value = value_after(args, i, "--seed")) {
            command.monte_carlo.seed = parse_uint64("--seed", *value);
        } else {
            throw std::invalid_argument("unknown argument: " + args[i]);
        }
    }

    if (!saw_kind) {
        throw std::invalid_argument("--kind is required");
    }
    if (!saw_type) {
        throw std::invalid_argument("--type is required");
    }

    return command;
}

int run(const std::vector<std::string>& args) {
    if (args.size() == 1 || args[1] == "--help" || args[1] == "-h") {
        print_usage(std::cout);
        return 0;
    }

    if (args[1] != "price") {
        throw std::invalid_argument("unknown command: " + args[1]);
    }

    const PriceCommand command = parse_price_command(args);
    const double black_scholes = options_pricer::black_scholes_price(command.option);
    const auto monte_carlo = options_pricer::price_european_monte_carlo(
        command.option,
        command.monte_carlo
    );

    std::cout << "kind: european\n";
    std::cout << "type: " << (command.option.type == OptionType::Call ? "call" : "put") << '\n';
    std::cout << "black_scholes: " << black_scholes << '\n';
    std::cout << "monte_carlo: " << monte_carlo.price << '\n';
    std::cout << "paths: " << monte_carlo.paths << '\n';
    std::cout << "seed: " << command.monte_carlo.seed << '\n';

    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::vector<std::string> args(argv, argv + argc);
        return run(args);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n\n";
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }
}
