#include <dal-public/src/calendar.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/interp.hpp>
#include <dal-public/src/storage.hpp>

int main() {
    Dal::InitGlobalData(1);
    const Dal::Date_ start(2026, 1, 1);
    if (start.AddDays(1) - start != 1)
        return 1;

    const Dal::Vector_<> x{0.0, 1.0};
    const Dal::Vector_<> y{1.0, 3.0};
    const auto interp = Dal::Interp1NewLinear("installed-consumer", x, y);
    if (interp.IsEmpty() || Dal::Interp1Get(interp, {0.5})[0] != 2.0)
        return 2;

    const Dal::Holidays_ holidays("");
    if (Dal::NextBusinessDay(holidays, Dal::Date_(2026, 1, 31)) != Dal::Date_(2026, 2, 2))
        return 3;

    const auto payload = Dal::WriteObjectJson(*interp);
    const auto restored = Dal::handle_cast<Dal::Interp1_>(Dal::ReadObjectJson(payload.data(), payload.size()));
    return restored.IsEmpty() || Dal::Interp1Get(restored, {0.5})[0] != 2.0 ? 4 : 0;
}
