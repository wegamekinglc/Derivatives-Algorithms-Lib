//
// Created by wegam on 2020/12/26.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/utilities/noncopyable.hpp>

namespace Dal::QuasiRandom {
    class SequenceSet_: noncopyable {
    public:
        virtual ~SequenceSet_() {}
        virtual int Size() const = 0;
        virtual void Next(Vector_<>* dst) = 0;
        virtual void NextNormal(Vector_<>* dst) = 0;
        virtual SequenceSet_* TakeAway(int sub_size) = 0;
    };
}
