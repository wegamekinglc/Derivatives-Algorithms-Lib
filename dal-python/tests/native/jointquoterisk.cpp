//
// Created by dal-implementer on 2026/9/11.
//

#include <pybind11/pybind11.h>

#include <tests/curve/jointquoteriskopaque.hpp>

PYBIND11_MODULE(_dal_quote_risk_test, m) {
    m.def("opaque_curve", [](const std::shared_ptr<Dal::DiscountCurve_>& inner, const std::string& currency) -> std::shared_ptr<Dal::DiscountCurve_> {
        return std::make_shared<JointXccyQuoteRiskFixtures::OpaqueCurve_>(Dal::Handle_<Dal::DiscountCurve_>(inner), Dal::String_(currency.c_str()));
    });
}
