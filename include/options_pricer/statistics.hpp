#pragma once

#include <cstdint>

namespace options_pricer {

class RunningStatistics {
public:
    void add(double value);
    void merge(const RunningStatistics& other);

    [[nodiscard]] std::uint64_t count() const;
    [[nodiscard]] double mean() const;
    [[nodiscard]] double sample_variance() const;

private:
    std::uint64_t count_{0};
    double mean_{0.0};
    double squared_deviation_sum_{0.0};
};

}  // namespace options_pricer
