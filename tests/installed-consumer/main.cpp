#include <dal/platform/platform.hpp>

#include <dal-public/src/interp.hpp>
#include <dal/time/date.hpp>

int main() {
    const Dal::Date_ start(2026, 1, 1);
    if (start.AddDays(1) - start != 1)
        return 1;

    const Dal::Vector_<> x{0.0, 1.0};
    const Dal::Vector_<> y{1.0, 3.0};
    const auto interp = Dal::Interp1NewLinear("installed-consumer", x, y);
    return interp.IsEmpty() ? 2 : 0;
}
