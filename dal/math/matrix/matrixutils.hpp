//
// Created by wegam on 2022/4/3.
//

#pragma once

#include <dal/string/strings.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    struct Cell_;
    namespace Matrix {
        Matrix_<Cell_> Format(const Vector_<const Matrix_<Cell_>*>& args, const String_& format);

        template <class T_> Matrix_<T_> MakeTranspose(const Matrix_<T_>& src) {
            Matrix_<T_> ret_val(src.Cols(), src.Rows());
            for (int ir = 0; ir < src.Rows(); ++ir)
                Copy(src.Row(ir), &ret_val.Col(ir));
            return ret_val;
        }

        template <class E_> void Append(Matrix_<E_>* above, const Matrix_<E_>& below) {
            REQUIRE(above != nullptr, "above matrix is null");
            const int offset = above->Rows();
            // efficient unless below is too wide
            above->Resize(offset + below.Rows(), Max(above->Cols(), below.Cols()));
            for (int ir = 0; ir < below.Rows(); ++ir)
                Copy(below.Row(ir), &above->Row(ir + offset));
        }

        // make a 1x1 matrix
        template <class E_> Matrix_<E_> M1x1(const E_& src) {
            Matrix_<E_> ret_val(1, 1);
            ret_val(0, 0) = src;
            return ret_val;
        }

        // copy/transform all matrix elements; these functions all take advantage of the known layout of matrix!
        template <class E_> typename Matrix_<E_>::Row_::iterator EndElements(Matrix_<E_>& m) {
            return m.Row(m.Empty() ? 0 : m.Rows() - 1).end();
        }
        template <class E_> typename Matrix_<E_>::ConstRow_::const_iterator EndElements(const Matrix_<E_>& m) {
            return m.Row(m.Empty() ? 0 : m.Rows() - 1).end();
        }

        template <class E_, class Op_> void Transform(Matrix_<E_>* container, Op_ op) {
            typename Matrix_<E_>::Row_::iterator b = container->Row(0).begin();
            transform(b, EndElements(*container), b, op);
        }
        template <class E_, class EE_, class Op_>
        void Transform(Matrix_<E_>* to_modify, const Matrix_<EE_>& other, Op_ op) {
            assert(other.Rows() == to_modify->Rows() && other.Cols() == to_modify->Cols());
            typename Matrix_<E_>::Row_::iterator b = to_modify->Row(0).begin();
            transform(b, EndElements(*to_modify), other.Row(0).begin(), b, op);
        }
        template <class EI_, class EO_, class Op_> void Transform(const Matrix_<EI_>& in, Op_ op, Matrix_<EO_>* out) {
            assert(in.Rows() == out->Rows() && in.Cols() == out->Cols());
            transform(in.Row(0).begin(), EndElements(in), out->Row(0).begin(), op);
        }
        template <class E1_, class E2_, class EO_, class Op_>
        void Transform(const Matrix_<E1_>& in1, const Matrix_<E2_>& in2, Op_ op, Matrix_<EO_>* out) {
            assert(in1.Rows() == out->Rows() && in1.Cols() == out->Cols());
            assert(in2.Rows() == out->Rows() && in2.Cols() == out->Cols());
            transform(in1.Row(0).begin(), EndElements(in1), in2.Row(0).begin(), out->Row(0).begin(), op);
        }

        template <class ES_, class ED_> void Copy(const Matrix_<ES_>& src, Matrix_<ED_>* dst) {
            assert(dst && src.Rows() == dst->Rows() && src.Cols() == dst->Cols());
            copy(src.Row(0).begin(), EndElements(src), dst->Row(0).begin());
        }
        template <class E_, class Op_> auto Apply(Op_ op, const Matrix_<E_>& src) -> Matrix_<decltype(op(src(0, 0)))> {
            Matrix_<decltype(op(src(0, 0)))> ret_val(src.Rows(), src.Cols());
            Transform(src, op, &ret_val);
            return ret_val;
        }

        template <class E_>
        Matrix_<E_>
        FromVectors(const Vector_<Vector_<E_>>& vv, bool by_cols = false, bool quiet = true, bool ragged = false) {
            if (vv.empty())
                return Matrix_<E_>();

            const int size2 =
                ragged ? *MaxElement(Apply([](const Vector_<E_>& v) { return v.size(); }, vv)) : vv[0].size();
            Matrix_<E_> ret_val(by_cols ? size2 : vv.size(), by_cols ? vv.size() : size2);
            for (int ii = 0; ii < vv.size(); ++ii) {
                REQUIRE(quiet || ragged || vv[ii].size() == size2, "Invalid array sizing");
                if (by_cols)
                    copy(vv[ii].begin(), vv[ii].end(), ret_val.Col(ii).begin());
                else
                    copy(vv[ii].begin(), vv[ii].end(), ret_val.Row(ii).begin());
            }
            return ret_val;
        }
        template <class E_> Vector_<Vector_<E_>> ToVectors(const Matrix_<E_>& m, bool by_cols = false) {
            Vector_<Vector_<E_>> ret_val(by_cols ? m.Cols() : m.Rows());
            for (int ii = 0; ii < ret_val.size(); ++ii) {
                ret_val[ii] = by_cols ? Copy(m.Col(ii)) : Copy(m.Row(ii));
            }
            return ret_val;
        }
    } // namespace Matrix
} // namespace Dal