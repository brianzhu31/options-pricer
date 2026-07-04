#include "options_pricer/black_scholes.hpp"
#include "options_pricer/monte_carlo.hpp"
#include "options_pricer/option.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using options_pricer::EuropeanOption;
using options_pricer::MonteCarloConfig;
using options_pricer::OptionKind;
using options_pricer::OptionType;

struct PriceCommand {
    OptionKind kind{OptionKind::European};
    EuropeanOption option;
    MonteCarloConfig monte_carlo;
    std::uint64_t steps{0};
};

struct BenchmarkCommand {
    std::vector<std::uint64_t> path_counts{100'000, 1'000'000};
    std::vector<std::uint32_t> thread_counts{1, 2, 4, 8};
    std::uint64_t seed{42};
};

void print_usage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  options-pricer price --kind european|asian --type call|put "
        << "--spot N --strike N --rate N --vol N --maturity N --paths N "
        << "[--steps N] [--seed N] [--threads N] [--antithetic]\n"
        << "  options-pricer bench [--paths N,N,...] [--threads N,N,...] [--seed N]\n"
        << "  --steps is required for Asian options and invalid for European options.\n\n"
        << "Examples:\n"
        << "  options-pricer price --kind european --type call --spot 100 --strike 100 "
        << "--rate 0.05 --vol 0.2 --maturity 1 --paths 100000\n"
        << "  options-pricer bench --paths 100000,1000000 --threads 1,2,4,8\n";
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
    if (result == 0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
    return static_cast<std::uint64_t>(result);
}

std::vector<std::uint64_t> parse_uint64_list(
    std::string_view name,
    const std::string& value
) {
    std::vector<std::uint64_t> values;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t comma = value.find(',', begin);
        const std::string item = value.substr(begin, comma - begin);
        if (item.empty()) {
            throw std::invalid_argument(
                std::string(name) + " must be a comma-separated integer list"
            );
        }
        values.push_back(parse_uint64(name, item));
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
    return values;
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
    bool saw_spot = false;
    bool saw_strike = false;
    bool saw_rate = false;
    bool saw_volatility = false;
    bool saw_maturity = false;
    bool saw_paths = false;
    bool saw_steps = false;

    for (std::size_t i = 2; i < args.size(); ++i) {
        if (auto value = value_after(args, i, "--kind")) {
            if (*value == "european") {
                command.kind = OptionKind::European;
            } else if (*value == "asian") {
                command.kind = OptionKind::Asian;
            } else {
                throw std::invalid_argument("--kind must be european or asian");
            }
            saw_kind = true;
        } else if (auto value = value_after(args, i, "--type")) {
            command.option.type = parse_option_type(*value);
            saw_type = true;
        } else if (auto value = value_after(args, i, "--spot")) {
            command.option.spot = parse_double("--spot", *value);
            saw_spot = true;
        } else if (auto value = value_after(args, i, "--strike")) {
            command.option.strike = parse_double("--strike", *value);
            saw_strike = true;
        } else if (auto value = value_after(args, i, "--rate")) {
            command.option.rate = parse_double("--rate", *value);
            saw_rate = true;
        } else if (auto value = value_after(args, i, "--vol")) {
            command.option.volatility = parse_double("--vol", *value);
            saw_volatility = true;
        } else if (auto value = value_after(args, i, "--maturity")) {
            command.option.maturity = parse_double("--maturity", *value);
            saw_maturity = true;
        } else if (auto value = value_after(args, i, "--paths")) {
            command.monte_carlo.paths = parse_uint64("--paths", *value);
            saw_paths = true;
        } else if (auto value = value_after(args, i, "--steps")) {
            command.steps = parse_uint64("--steps", *value);
            saw_steps = true;
        } else if (auto value = value_after(args, i, "--seed")) {
            command.monte_carlo.seed = parse_uint64("--seed", *value);
        } else if (auto value = value_after(args, i, "--threads")) {
            const std::uint64_t threads = parse_uint64("--threads", *value);
            if (threads > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("--threads is too large");
            }
            command.monte_carlo.threads = static_cast<std::uint32_t>(threads);
        } else if (args[i] == "--antithetic") {
            command.monte_carlo.antithetic = true;
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
    if (!saw_spot || !saw_strike || !saw_rate || !saw_volatility ||
        !saw_maturity || !saw_paths) {
        throw std::invalid_argument(
            "--spot, --strike, --rate, --vol, --maturity, and --paths are required"
        );
    }
    if (command.option.spot <= 0.0 || command.option.strike <= 0.0 ||
        command.option.volatility <= 0.0 || command.option.maturity <= 0.0) {
        throw std::invalid_argument(
            "--spot, --strike, --vol, and --maturity must be positive"
        );
    }
    if (command.kind == OptionKind::Asian && !saw_steps) {
        throw std::invalid_argument("--steps is required for Asian options");
    }
    if (command.kind == OptionKind::European && saw_steps) {
        throw std::invalid_argument("--steps is only valid for Asian options");
    }
    if (command.monte_carlo.antithetic && command.monte_carlo.paths % 2 != 0) {
        throw std::invalid_argument("--paths must be even with --antithetic");
    }

    return command;
}

BenchmarkCommand parse_benchmark_command(const std::vector<std::string>& args) {
    BenchmarkCommand command;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (auto value = value_after(args, i, "--paths")) {
            command.path_counts = parse_uint64_list("--paths", *value);
        } else if (auto value = value_after(args, i, "--threads")) {
            const auto values = parse_uint64_list("--threads", *value);
            command.thread_counts.clear();
            for (const std::uint64_t threads : values) {
                if (threads > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::invalid_argument("--threads value is too large");
                }
                command.thread_counts.push_back(static_cast<std::uint32_t>(threads));
            }
        } else if (auto value = value_after(args, i, "--seed")) {
            command.seed = parse_uint64("--seed", *value);
        } else {
            throw std::invalid_argument("unknown benchmark argument: " + args[i]);
        }
    }

    for (const std::uint64_t paths : command.path_counts) {
        if (paths % 2 != 0) {
            throw std::invalid_argument(
                "benchmark path counts must be even for antithetic comparison"
            );
        }
    }
    return command;
}

void run_benchmark(const BenchmarkCommand& command) {
    const EuropeanOption option{};

    std::cout
        << "Benchmark: European ATM call (spot=100, strike=100, rate=0.05, "
        << "vol=0.2, maturity=1)\n";
    std::cout
        << std::left
        << std::setw(12) << "mode"
        << std::right
        << std::setw(12) << "paths"
        << std::setw(10) << "threads"
        << std::setw(14) << "price"
        << std::setw(16) << "std_error"
        << std::setw(14) << "elapsed_ms"
        << std::setw(18) << "paths_per_sec"
        << '\n';

    for (const std::uint64_t paths : command.path_counts) {
        for (const std::uint32_t threads : command.thread_counts) {
            for (const bool antithetic : {false, true}) {
                const MonteCarloConfig config{
                    .paths = paths,
                    .seed = command.seed,
                    .threads = threads,
                    .antithetic = antithetic,
                };
                const auto start = std::chrono::steady_clock::now();
                const auto result =
                    options_pricer::price_european_monte_carlo(option, config);
                const auto stop = std::chrono::steady_clock::now();
                const double elapsed_seconds =
                    std::chrono::duration<double>(stop - start).count();
                const double elapsed_ms = elapsed_seconds * 1'000.0;
                const double paths_per_second =
                    static_cast<double>(paths) / elapsed_seconds;

                std::cout
                    << std::left << std::setw(12)
                    << (antithetic ? "antithetic" : "plain")
                    << std::right
                    << std::setw(12) << paths
                    << std::setw(10) << threads
                    << std::setw(14) << std::fixed << std::setprecision(6)
                    << result.price
                    << std::setw(16) << result.standard_error
                    << std::setw(14) << std::setprecision(3) << elapsed_ms
                    << std::setw(18) << std::setprecision(0) << paths_per_second
                    << '\n';
            }
        }
    }
}

int run(const std::vector<std::string>& args) {
    if (args.size() == 1 || args[1] == "--help" || args[1] == "-h") {
        print_usage(std::cout);
        return 0;
    }

    if (args[1] == "bench") {
        run_benchmark(parse_benchmark_command(args));
        return 0;
    }
    if (args[1] != "price") {
        throw std::invalid_argument("unknown command: " + args[1]);
    }

    const PriceCommand command = parse_price_command(args);
    options_pricer::MonteCarloResult monte_carlo;
    if (command.kind == OptionKind::European) {
        monte_carlo = options_pricer::price_european_monte_carlo(
            command.option,
            command.monte_carlo
        );
    } else {
        const options_pricer::AsianOption asian{
            .type = command.option.type,
            .spot = command.option.spot,
            .strike = command.option.strike,
            .rate = command.option.rate,
            .volatility = command.option.volatility,
            .maturity = command.option.maturity,
            .steps = command.steps,
        };
        monte_carlo = options_pricer::price_asian_monte_carlo(
            asian,
            command.monte_carlo
        );
    }

    std::cout << "kind: "
              << (command.kind == OptionKind::European ? "european" : "asian") << '\n';
    std::cout << "type: " << (command.option.type == OptionType::Call ? "call" : "put") << '\n';
    if (command.kind == OptionKind::European) {
        std::cout << "black_scholes: "
                  << options_pricer::black_scholes_price(command.option) << '\n';
    } else {
        std::cout << "steps: " << command.steps << '\n';
    }
    std::cout << "monte_carlo: " << monte_carlo.price << '\n';
    std::cout << "standard_error: " << monte_carlo.standard_error << '\n';
    std::cout << "confidence_interval_95: ["
              << monte_carlo.confidence_interval_low << ", "
              << monte_carlo.confidence_interval_high << "]\n";
    std::cout << "paths: " << monte_carlo.paths << '\n';
    std::cout << "seed: " << command.monte_carlo.seed << '\n';
    std::cout << "threads: " << command.monte_carlo.threads << '\n';
    std::cout << "antithetic: "
              << (command.monte_carlo.antithetic ? "true" : "false") << '\n';

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
