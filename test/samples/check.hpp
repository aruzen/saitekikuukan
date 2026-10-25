#pragma once

#include <cstdlib>
#include <iostream>
#include <source_location>

namespace sample {
    // Release ビルドでも結果の確認を省略しない。
    inline void check(bool condition,
                      std::source_location location = std::source_location::current()) {
        if (!condition) {
            std::cerr << location.file_name() << ':' << location.line()
                      << ": sample check failed\n";
            std::abort();
        }
    }
}
