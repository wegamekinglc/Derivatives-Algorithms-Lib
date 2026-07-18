//
// Created by wegam on 2020/12/26.
//

#pragma once

#include <dal/math/random/base.hpp>
#include <dal/utilities/noncopyable.hpp>

namespace Dal {
    class SequenceSet_ : public Random_, noncopyable {
    public:
        ~SequenceSet_() override = default;
        [[nodiscard]] size_t NDim() const override = 0;
        void FillUniform(Vector_<>* dst) override = 0 ;
        void FillNormal(Vector_<>* dst) override = 0;
        virtual std::unique_ptr<SequenceSet_> TakeAway(int subSize) = 0;
    };
} // namespace Dal
