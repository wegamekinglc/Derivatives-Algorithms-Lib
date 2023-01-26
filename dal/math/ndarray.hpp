//
// Created by wegam on 2023/1/25.
//

#pragma once

#include <iterator>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal {
    namespace ArrayN {
        BASE_EXPORT Vector_<int> Strides(const Vector_<int>& sizes);
        BASE_EXPORT Vector_<pair<int, int>> Moves(const Vector_<int>& old_sizes, const Vector_<int>& new_sizes);
    } // namespace ArrayN

    template <class E_>
    class ArrayN_ {
    private:
        Vector_<int> sizes_;
        Vector_<int> strides_;
        Vector_<E_> vals_;

    public:
        explicit ArrayN_(const Vector_<int>& sizes, const E_& fill = 0)
            :sizes_(sizes), strides_(ArrayN::Strides(sizes)), vals_(strides_[0] * sizes[0], fill) {}

        const E_& operator[](const Vector_<int>& where) const { return vals_[InnerProduct(where, strides_)]; }
        E_& operator[](const Vector_<int>& where) { return vals_[InnerProduct(where, strides_)]; }

        [[nodiscard]] const Vector_<int>& Sizes() const { return sizes_; }
        [[nodiscard]] bool IsEmpty() const { return vals_.empty(); }
        void Fill(double val) { vals_.Fill(val); }
        void operator*=(double scale) { vals_ *= scale; }

        void Swap(ArrayN_<E_>* other) {
            sizes_.Swap(&other->sizes_);
            strides_.Swap(&other->strides_);
            vals_.Swap(&other->vals_);
        }

        // also allow Swap with a vector, iff we are effectively one-dimensional
        void Swap(Vector_<E_>* other) {
            auto pm = MaxElement(sizes_);
            REQUIRE(*pm == vals_.size(), "Can't swap a vector with a multi-dimensional array");
            vals_.Swap(other);
            int index = std::distance(sizes_.cbegin(), pm);
            *(sizes_.begin() + index) = vals_.size();
            strides_ = ArrayN::Strides(sizes_);
        }

        // Resize() requires computation of element mapping; take it out-of-line
        void Resize(const Vector_<int>& new_sizes) {
            const Vector_<pair<int, int>>& moves = ArrayN::Moves(sizes_, new_sizes);
            Vector_<E_> new_values(sizes_[0] * strides_[0], E_());
            sizes_ = new_sizes;
            strides_ = ArrayN::Strides(sizes_);
            for (const auto& move : moves)
                new_values[move.second] = vals_[move.first];
            vals_.Swap(&new_values);
        }
        // allow enough access for Cube_
    protected:
        struct XLoc_ {
            int offset_;
            int so_far_;
            const Vector_<int>& strides_;
            explicit XLoc_(const Vector_<int>& strides) : offset_(0), so_far_(0), strides_(strides) {}
            XLoc_& operator()(int i_x) {
                so_far_ += i_x * strides_[offset_++];
                return *this;
            }
            [[nodiscard]] int Offset() const {
                REQUIRE(offset_ == strides_.size(), "offset size should be same as strides");
                return so_far_;
            }
        };
        XLoc_ Goto() const { return XLoc_(strides_); }
        const E_& At(const XLoc_& loc) const { return vals_[loc.Offset()]; }
        E_& At(const XLoc_& loc) { return vals_[loc.Offset()]; }
    };

    template <class E_>
    class Cube_ : public ArrayN_<E_> {
    public:
        Cube_() :ArrayN_<E_>(Vector_<int>({ 0, 0, 0 }), E_()) {}
        Cube_(int size_i, int size_j, int size_k): ArrayN_<E_>(Vector_<int>({ size_i, size_j, size_k })) {}

        // support lookups without constructing a temporary vector
        const double& operator()(int ii, int jj, int kk) const {
            return ArrayN_<E_>::At(ArrayN_<E_>::Goto()(ii)(jj)(kk));
        }
        double& operator()(int ii, int jj, int kk) {
            return ArrayN_<E_>::At(ArrayN_<E_>::Goto()(ii)(jj)(kk));
        }
        // allow access to slices (last dimension)
        [[nodiscard]] inline double* SliceBegin(int ii, int jj) {
            return &operator()(ii, jj, 0);
        }

        [[nodiscard]] inline const double* SliceBegin(int ii, int jj) const {
            return &operator()(ii, jj, 0);
        }

        [[nodiscard]] inline const double* SliceEnd(int ii, int jj) const {
            return SliceBegin(ii, jj) + ArrayN_<E_>::Goto()(0)(1)(0).Offset();
        }

        [[nodiscard]] inline int SizeI() const { return ArrayN_<E_>::Sizes()[0]; }
        [[nodiscard]] inline int SizeJ() const { return ArrayN_<E_>::Sizes()[1]; }
        [[nodiscard]] inline int SizeK() const { return ArrayN_<E_>::Sizes()[2]; }

        void Resize(int size_i, int size_j, int size_k) {
            ArrayN_<E_>::Resize(Vector_<int>({ size_i, size_j, size_k }));
        }

    };

} // namespace Dal
