#include "types.hpp"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>

struct configuration {
    std::size_t rows = 262144;
    int iterations = 24;
    int samples = 7;
};

template<typename Prepare, typename Operation>
double measure(const configuration& config, Prepare prepare, Operation operation) {
    std::vector<double> samples(config.samples);
    for (auto& sample : samples) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < config.iterations; ++i) { prepare(); operation(); }
        sample = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count()
            / config.iterations / config.rows;
    }
    std::ranges::sort(samples);
    return samples[samples.size() / 2];
}

template<typename Values, typename Expected>
double verify(const Values& values, std::size_t count, Expected expected) {
    if (values.size() != count) std::abort();
    const auto x = values.template column<0>();
    const auto y = values.template column<1>();
    double checksum = 0;
    for (std::size_t i = 0; i < count; ++i) {
        auto [a, b] = expected(i);
        if (x[i] != a || y[i] != b) std::abort();
        checksum += x[i] + y[i];
    }
    return checksum;
}

template<typename Values>
void run_case(std::string_view name, const configuration& config,
    void (*resize)(Values&, std::size_t), void (*push)(Values&, std::size_t),
    void (*columns)(Values&, float, float), void (*rows)(Values&, float, float) = nullptr,
    void (*read_write)(Values&, float, float) = nullptr) {
    Values values;
    values.reserve(config.rows); // 計測区間から容量の初回確保を除く。
    auto report = [&](const char* operation, double time, double checksum) {
        std::printf("%s,%.*s,%zu,%.6f,%.0f\n", operation, static_cast<int>(name.size()), name.data(),
            config.rows, time, checksum);
    };
    auto time = measure(config, [&] { values.clear(); }, [&] { resize(values, config.rows); });
    report("clear_resize", time, verify(values, config.rows, [](auto) { return std::pair{0.0f, 0.0f}; }));
    time = measure(config, [&] { values.clear(); }, [&] { push(values, config.rows); });
    report("clear_push", time, verify(values, config.rows, [](auto i) {
        return std::pair{static_cast<float>(i), static_cast<float>(i + 1)};
    }));
    auto update = [&](const char* operation, auto function) {
        if (!function) return;
        values.clear();
        resize(values, config.rows);
        auto elapsed = measure(config, [] {}, [&] { function(values, 1, 2); });
        float calls = static_cast<float>(config.iterations * config.samples);
        report(operation, elapsed, verify(values, config.rows, [calls](auto) { return std::pair{calls, calls * 2}; }));
    };
    update("columns", columns);
    update("rows", rows);
    update("read_write", read_write);
}

int main(int argc, char** argv) {
    configuration config;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) return 2;
        auto option = std::string_view(argv[i]);
        auto text = std::string_view(argv[i + 1]);
        unsigned value = 0;
        auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0) return 2;
        if (option == "--rows" && value <= (1u << 24)) config.rows = value;
        else if (option == "--iterations" && value <= 1000) config.iterations = static_cast<int>(value);
        else if (option == "--samples" && value <= 1000) config.samples = static_cast<int>(value);
        else return 2;
    }
    std::puts("operation,variant,rows,median_ns_per_row,checksum");
    run_case("vectors", config, vectors_resize, vectors_push, vectors_columns);
    run_case("tuple", config, tuple_resize, tuple_push, tuple_columns, tuple_rows, tuple_values);
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
    run_case("record", config, record_resize, record_push, record_columns, record_rows, record_values);
    run_case("record_initializers", config, initialized_resize, initialized_push,
        initialized_columns, initialized_rows, initialized_values);
#endif
}
