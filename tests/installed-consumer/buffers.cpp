//
// Created by Codex on 2026/10/06.
//

#include <cstdint>
#include <iostream>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/random/sobol.hpp>

int main() {
    const auto payload = 2 * 33 * sizeof(uint_least32_t);
    Dal::BufferCapacityBudget_ budget(payload);
    {
        Dal::BufferCapacityScope_ scope(&budget);
        auto sequence = Dal::NewSobol(2, 0);
        if (budget.CapacityBytes() != payload || sequence->NDim() != 2)
            return 1;
        sequence.reset();
        if (budget.CapacityBytes() != 0)
            return 2;
    }
    Dal::BufferCapacityBudget_ insufficient(payload - 1);
    {
        Dal::BufferCapacityScope_ scope(&insufficient);
        try {
            static_cast<void>(Dal::NewSobol(2, 0));
            return 3;
        } catch (const Dal::Exception_&) {
            if (insufficient.CapacityBytes() != 0)
                return 4;
        }
    }
    std::cout << "Installed consumer/library buffer capacity admission: PASS\n";
}
