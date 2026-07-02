#include "options_pricer/statistics.hpp"

#include <algorithm>

namespace options_pricer {

void RunningStatistics::add(double value) {
    ++count_;
    const double delta = value - mean_;
    mean_ += delta / static_cast<double>(count_);
    const double delta_after = value - mean_;
    squared_deviation_sum_ += delta * delta_after;
}

void RunningStatistics::merge(const RunningStatistics& other) {
    if (other.count_ == 0) {
        return;
    }
    if (count_ == 0) {
        *this = other;
        return;
    }

    const std::uint64_t combined_count = count_ + other.count_;
    const double delta = other.mean_ - mean_;
    squared_deviation_sum_ += other.squared_deviation_sum_
        + delta * delta
            * static_cast<double>(count_)
            * static_cast<double>(other.count_)
            / static_cast<double>(combined_count);
    mean_ += delta * static_cast<double>(other.count_)
        / static_cast<double>(combined_count);
    count_ = combined_count;
}

std::uint64_t RunningStatistics::count() const {
    return count_;
}

double RunningStatistics::mean() const {
    return mean_;
}

double RunningStatistics::sample_variance() const {
    if (count_ < 2) {
        return 0.0;
    }
    return std::max(
        squared_deviation_sum_ / static_cast<double>(count_ - 1),
        0.0
    );
}

}  // namespace options_pricer
