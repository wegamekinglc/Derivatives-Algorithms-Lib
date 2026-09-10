//
// Created by dal-implementer on 2026/9/11.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/storage/archive.hpp>

namespace JointXccyQuoteRiskFixtures {
    using namespace Dal;

    class OpaqueCurve_ : public DiscountCurve_ {
        Handle_<DiscountCurve_> inner_;

    public:
        explicit OpaqueCurve_(const Handle_<DiscountCurve_>& inner = {}, const String_& currency = "USD")
            : DiscountCurve_("opaque", currency), inner_(inner) {}
        double operator()(const Date_& from, const Date_& to) const override { return inner_ ? (*inner_)(from, to) : 1.0; }
        void Poll(Vector_<const YCComponent_*>* all) const override {
            all->push_back(this);
            if (inner_)
                inner_->Poll(all);
        }
        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>* all) const override {
            if (inner_)
                inner_->Poll(all);
        }
        [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_&, const YCComponent_::substitutions_t& changes) const override {
            const auto found = changes.find(inner_.get());
            return std::make_unique<OpaqueCurve_>(found == changes.end() ? inner_ : handle_cast<DiscountCurve_>(found->second), ccy_.String());
        }
        void Write(Archive::Store_& dst) const override {
            dst.SetType("OpaqueCurve_JointQuoteRiskTestOnly");
            if (inner_)
                Archive::Utils::Set(dst, "inner", inner_);
            dst.Done();
        }
    };
} // namespace JointXccyQuoteRiskFixtures
