//
// Created by dal-implementer on 2026/7/28.
//

#include <dal-public/src/curvepricing.hpp>

namespace Dal {
    const Vector_<RateInstrumentType_>& CurvePricingFamilyRegistry() {
        static const Vector_<RateInstrumentType_> result = RateInstrumentTypeListAll();
        return result;
    }
} // namespace Dal
