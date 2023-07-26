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
            adjoints_multi_.Memset(0);
        else {
            for (auto it = nodes_.Begin(); it != nodes_.End(); ++it)
                it->adjoint_ = 0.;
        }
    }

    void Tape_::Clear() {
        adjoints_multi_.Clear();
        ders_.Clear();
        arg_ptrs_.Clear();
        nodes_.Clear();
    }

    void Tape_::setActive() {}
    void Tape_::registerInput(Number_& input) {
        input.PutOnTape();
    }

    void Tape_::reset() {
        if (multi_)
            adjoints_multi_.Rewind();
        ders_.Rewind();
        arg_ptrs_.Rewind();
        nodes_.Rewind();
    }

    Tape_::Position_ Tape_::getPosition() {
        Position_ position;
        if (multi_)
            position.adjoints_multi_pos_ = adjoints_multi_.GetPosition();
        position.ders_pos_ = ders_.GetPosition();
        position.arg_ptrs_pos_ = arg_ptrs_.GetPosition();
        position.nodes_pos_ = nodes_.GetPosition();
        return position;
    }

    Tape_::Position_ Tape_::getZeroPosition() {
        Position_ position;
        if (multi_)
            position.adjoints_multi_pos_ = adjoints_multi_.GetZeroPosition();
        position.ders_pos_ = ders_.GetZeroPosition();
        position.arg_ptrs_pos_ = arg_ptrs_.GetZeroPosition();
        position.nodes_pos_ = nodes_.GetZeroPosition();
        return position;
    }

    void Tape_::resetTo(const Position_& pos, bool resetAdjoints) {
        if (multi_)
            adjoints_multi_.RewindTo(pos.adjoints_multi_pos_);
        ders_.RewindTo(pos.ders_pos_);
        arg_ptrs_.RewindTo(pos.arg_ptrs_pos_);
        nodes_.RewindTo(pos.nodes_pos_);
    }

    void Tape_::evaluate(const Position_& from, const Position_& to) {
        Number_::evaluate(from, to);
    }

    void Tape_::evaluate() {
        Number_::evaluate(getPosition(), getZeroPosition());
    }

} // namespace Dal::AAD
#endif