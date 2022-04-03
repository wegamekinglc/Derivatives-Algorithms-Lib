//
// Created by wegam on 2022/4/3.
//

#pragma once

#ifdef _WIN32

// Framework for Excel interface

// From the Microsoft Excel Developer's Kit, Version 14
// Copyright (c) 1997 - 2010 Microsoft Corporation.

#define rwMaxO8 (65536)
#define colMaxO8 (256)
#define cchMaxStz (255)
#define MAXSHORTINT 0x7fff

#include <dal/utilities/environment.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/optionals.hpp>
#include <dal/storage/storable.hpp>
#include <dal/public/_reader.hpp>

#define XL_CALLBACK Excel12v

struct xloper12;

namespace Dal {
    struct Cell_;
    class Date_;
    class DateTime_;
    class Dictionary_;

    typedef xloper12 OPER_;
    OPER_* TempStr(const String_& src); // takes NON-BYTE-COUNTED input

    namespace Logging {
        inline void Write(const char* msg) {} // logging is not supported
    }                                         // namespace Log

    namespace Excel {
        inline void InitializeSessionIfNeeded() {} // no runtime initialization required
        double ToDouble(const OPER_* src);
        boost::optional<double> ToDouble(const OPER_* src, bool); // use overloading to know the return type
        int ToInt(const OPER_* src);
        boost::optional<int> ToInt(const OPER_* src, bool);
        bool ToBool(const OPER_* src);
        boost::optional<bool> ToBool(const OPER_* src, bool);
        String_ ToString(const OPER_* src, bool optional = false);
        Date_ ToDate(const OPER_* src);
        boost::optional<Date_> ToDate(const OPER_* src, bool optional);
        DateTime_ ToDateTime(const OPER_* src, bool optional = false);
        Cell_ ToCell(const OPER_* src, bool optional = false);
        Dictionary_ ToDictionary(const OPER_* src, bool optional = false);
        Handle_<Storable_> ToHandleBase(_ENV, const OPER_* src, bool optional = false);
        Vector_<> ToDoubleVector(const OPER_* src, bool optional = false);
        Vector_<int> ToIntVector(const OPER_* src, bool optional = false);
        Vector_<bool> ToBoolVector(const OPER_* src, bool optional = false);
        Vector_<String_> ToStringVector(const OPER_* src, bool optional = false);
        Vector_<Date_> ToDateVector(const OPER_* src, bool optional = false);
        Vector_<Cell_> ToCellVector(const OPER_* src, bool optional = false);
        Vector_<Handle_<Storable_>> ToHandleBaseVector(_ENV, const OPER_* src, bool optional = false);
        Matrix_<double> ToDoubleMatrix(const OPER_* src, bool optional = false);
        Matrix_<int> ToIntMatrix(const OPER_* src, bool optional = false);
        // Matrix of bool is presently not supported
        Matrix_<String_> ToStringMatrix(const OPER_* src, bool optional = false);
        Matrix_<Cell_> ToCellMatrix(const OPER_* src, bool optional = false, bool greedy = false);

        template <class T_> T_ ToEnum(const OPER_* src) { return T_(ToString(src)); }
        template <class T_> boost::optional<T_> ToEnum(const OPER_* src, bool optional) {
            boost::optional<T_> retval;
            const String_ s = ToString(src, optional);
            if (!s.empty())
                retval = T_(s);
            return retval;
        }

        template <class T_> Handle_<T_> ToHandle(_ENV, const OPER_* src, bool optional = false) {
            const Handle_<Storable_> base = ToHandleBase(_env, src, optional);
            if (base.IsEmpty() && optional)
                return Handle_<T_>();

            const Handle_<T_> retval = handle_cast<T_>(base);
            REQUIRE(!retval.Empty(), "Input object has wrong type");
            return retval;
        }
        template <class T_> Vector_<Handle_<T_>> ToHandleVector(_ENV, const OPER_* src, bool optional = false) {
            const Vector_<Handle_<Storable_>> base = ToHandleBaseVector(_env, src, optional);
            Vector_<Handle_<T_>> retval;
            for (const auto& b : base) {
                retval.push_back(handle_cast<T_>(b));
                REQUIRE(!retval.back().Empty(), "Input object has wrong type");
            }
            return retval;
        }

        class RowReader_ : public UIRow_ {
            const OPER_* src_;

        public:
            int iRow_;
            RowReader_(const OPER_* src) : src_(src), iRow_(0) {}
            int Rows() const;

            bool Exists(int i_col) const override;
            double ExtractDouble(int i_col) const override;
            double ExtractDouble(int i_col, double def_val) const override;
            int ExtractInt(int i_col) const override;
            int ExtractInt(int i_col, int def_val) const override;
            bool ExtractBool(int i_col) const override;
            String_ ExtractString(int i_col) const override;
            Date_ ExtractDate(int i_col) const override;
            Handle_<Storable_> ExtractHandleBase(_ENV, int i_col) const override;
        };

        // armed with Row Reader, we can process records
        template <class T_> Vector_<T_> ToRecordVector(_ENV, const OPER_* src, bool optional = false) {
            Vector_<T_> retval;
            for (RowReader_ reader(src); reader.iRow_ < reader.Rows(); ++reader.iRow_)
                retval.emplace_back(_env, reader);
            REQUIRE(optional || !retval.empty(), "No records found");
            return retval;
        }
        // also use the reader for greedy handle inputs -- note this can silently return NULL handles
        template <class T_> Vector_<Handle_<T_>> ToHandleVector(_ENV, const OPER_* src, bool optional, bool greedy) {
            if (!greedy)
                return ToHandleVector<T_>(_env, src, optional);
            Vector_<Handle_<T_>> retval;
            for (RowReader_ reader(src); reader.iRow_ < reader.Rows(); ++reader.iRow_)
                retval.emplace_back(new T_(_env, reader));
            REQUIRE(optional || !retval.empty(), "No handles found");
            return retval;
        }

        // matrix of handles is presently not supported

        template <class T_> T_ ToSettings(const OPER_* src, bool optional = false) {
            return T_(ToDictionary(src, optional));
        }

        struct Retval_ {
            Vector_<Matrix_<Cell_>> values_;
            Vector_<int> scalars_; // locations of the scalars
            Vector_<int> vectors_;
            Vector_<int> matrices_;

            void Load(double s);
            void Load(const String_& s);
            void Load(const Cell_& s);
            void LoadBase(_ENV, const Handle_<Storable_>& s);
            template <class T_> void Load(_ENV, const Handle_<T_>& h) { LoadBase(_env, handle_cast<Storable_>(h)); }
            void Load(const Vector_<>& v);
            void Load(const Vector_<int>& v);
            void Load(const Vector_<String_>& v);
            void Load(const Vector_<Cell_>& v);
            void LoadBase(_ENV, const Vector_<Handle_<Storable_>>& v);
            template <class T_> void Load(_ENV, const Vector_<Handle_<T_>>& v) {
                LoadBase(_env, Apply([](const Handle_<T_>& h) { return handle_cast<Storable_>(h); }, v));
            }
            void Load(const Matrix_<double>& m);
            void Load(const Matrix_<int>& m);
            void Load(const Matrix_<String_>& m);
            void Load(const Matrix_<Cell_>& m);
            OPER_* ToXloper() const;
            OPER_* ToXloper(const String_& format) const;
        };

        OPER_* Error(const String_& what, const char* arg_name);

        void Register(const String_& dll_name,
                      const String_& c_name,
                      const String_& xl_name,
                      const String_& help,
                      const String_& arg_types,
                      const String_& arg_names,
                      const Vector_<String_>& arg_help,
                      bool volatile);
    } // namespace Excel

    extern "C" {
    char* GetTempMemory(size_t cBytes);
    void FreeAllTempMemory();
    OPER_* TempNum(double d);
    }

    namespace Cell {
        String_ ToString(const Cell_& c); // sets policy at interface, by forwarding to OwnString or CoerceToString
    }
}

#endif