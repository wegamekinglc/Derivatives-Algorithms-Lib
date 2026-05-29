//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    namespace Sparse {
        class Square_;

        class TriDiagonal_ : public Sparse::Square_ {
            Vector_<> diag_, above_, below_;

        public:
            explicit TriDiagonal_(int size) : diag_(size, 0.0), above_(size - 1, 0.0), below_(size - 1, 0.0) {}
            [[nodiscard]] int Size() const override { return diag_.size(); }
            [[nodiscard]] bool IsSymmetric() const override { return above_ == below_; }

            double* At(int i_row, int j_col);
            const double& operator()(int i_row, int j_col) const override;
            void Set(int i_row, int j_col, double val) override {
                double* dst = At(i_row, j_col);
                REQUIRE(dst, "out of band write to tri-diagonal");
                *dst = val;
            }

            void Add(int i_row, int j_col, double inc) override {
                double* dst = At(i_row, j_col);
                REQUIRE(dst, "out of band write to tri-diagonal");
                *dst += inc;
            }

            [[nodiscard]] const Vector_<>& Above() const { return above_; }
            [[nodiscard]] const Vector_<>& Diag() const { return diag_; }
            [[nodiscard]] const Vector_<>& Below() const { return below_; }


            void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const override;
            void MultiplyRight(const Vector_<>& x, Vector_<>* b) const override ;
            [[nodiscard]] SquareMatrixDecomposition_* Decompose() const override ;
        };

        Square_* NewBandDiagonal(int size, int n_above, int n_below);
    } // namespace Sparse

    class LowerBandAccumulator_ {
        Matrix_<> val_;

    public:
        LowerBandAccumulator_(int size, int n_below);
        void Add(const Vector_<>& v, int offset);

        void SolveLeft(const Vector_<>& b, Vector_<>* x) const;
        void SolveRight(const Vector_<>& b, Vector_<>* x) const;
    };
} // namespace Dal
