#include "floatformat.hpp"

#include <limits>
#include <stdexcept>
#include <string>

void Check(double value, const std::string& expected, int minDecimals = 2) {
    const std::string actual = ExampleFloat(value, minDecimals);
    if (actual != expected)
        throw std::runtime_error("Expected " + expected + ", got " + actual);
}

int main() {
    Check(0.0, "0.00");
    Check(12.0, "12.00");
    Check(1.2345, "1.234");
    Check(0.00001234, "0.00001234");
    Check(-0.0000000001234, "-0.0000000001234");
    Check(0.01, "0.010000", 6);
    Check(std::numeric_limits<double>::infinity(), "inf");
    Check(-std::numeric_limits<double>::infinity(), "-inf");
    Check(std::numeric_limits<double>::quiet_NaN(), "nan");
}
