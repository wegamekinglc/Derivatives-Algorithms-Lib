//
// Created by wegam on 2023/2/18.
//

#include <dal/platform/platform.hpp>

#include <dal/math/aad/expr.hpp>
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {

    void Tape_::ResetAdjoints() {
        if (multi_)
            adjointsMulti_.Memset(0);
        else {
            for (auto it = nodes_.Begin(); it != nodes_.End(); ++it)
                it->adjoint_ = 0.;
        }
    }

    void Tape_::Clear() {
        adjointsMulti_.Clear();
        ders_.Clear();
        argPtrs_.Clear();
        nodes_.Clear();
    }

    void Tape_::Mark() {
        if (multi_)
            adjointsMulti_.SetMark();
        ders_.SetMark();
        argPtrs_.SetMark();
        nodes_.SetMark();
    }

    void Tape_::Rewind()  {
        if (multi_)
            adjointsMulti_.Rewind();
        ders_.Rewind();
        argPtrs_.Rewind();
        nodes_.Rewind();
    }

    void Tape_::RewindToMark() {
        if (multi_)
            adjointsMulti_.RewindToMark();
        ders_.RewindToMark();
        argPtrs_.RewindToMark();
        nodes_.RewindToMark();
    }

    Tape_::Iterator_ Tape_::MarkIt() {
        return nodes_.Mark();
    }


} // namespace Dal::AAD