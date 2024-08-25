//
// Created by wegam on 2023/2/18.
//

#include <dal/platform/platform.hpp>

#ifdef USE_AADET
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

    void Tape_::registerInput(Number_& input) {
        input.PutOnTape();
    }

    void Tape_::reset() {
        if (multi_)
            adjointsMulti_.Rewind();
        ders_.Rewind();
        argPtrs_.Rewind();
        nodes_.Rewind();
    }

    Tape_::Position_ Tape_::getPosition() {
        Position_ position;
        if (multi_)
            position.adjointsMultiPos_ = adjointsMulti_.GetPosition();
        position.dersPos_ = ders_.GetPosition();
        position.argPtrsPos_ = argPtrs_.GetPosition();
        position.nodesPos_ = nodes_.GetPosition();
        return position;
    }

    Tape_::Position_ Tape_::getZeroPosition() {
        Position_ position;
        if (multi_)
            position.adjointsMultiPos_ = adjointsMulti_.GetZeroPosition();
        position.dersPos_ = ders_.GetZeroPosition();
        position.argPtrsPos_ = argPtrs_.GetZeroPosition();
        position.nodesPos_ = nodes_.GetZeroPosition();
        return position;
    }

    void Tape_::resetTo(const Position_& pos, bool reset_adjoints) {
        // TODO: reset_adjoints is not used at all
        if (multi_)
            adjointsMulti_.RewindTo(pos.adjointsMultiPos_);
        ders_.RewindTo(pos.dersPos_);
        argPtrs_.RewindTo(pos.argPtrsPos_);
        nodes_.RewindTo(pos.nodesPos_);
    }

    void Tape_::evaluate(const Position_& from, const Position_& to) {
        Number_::evaluate(from, to);
    }

    void Tape_::evaluate() {
        Number_::evaluate(getPosition(), getZeroPosition());
    }

    void Tape_::Mark() {
        if (multi_)
            adjointsMulti_.SetMark();
        ders_.SetMark();
        argPtrs_.SetMark();
        nodes_.SetMark();
    }

    void Tape_::RewindToMark() {
        if (multi_)
            adjointsMulti_.RewindToMark();
        ders_.RewindToMark();
        argPtrs_.RewindToMark();
        nodes_.RewindToMark();
    }


} // namespace Dal::AAD
#endif