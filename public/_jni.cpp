//
// Created by wegam on 2022/5/8.
//

#include <dal/platform/platform.hpp>

#include <dal/platform/strict.hpp>
#include <public/_jni.hpp>

#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/matrixutils.hpp>

namespace Dal {

    double JNI::ToDouble(JNIEnv*, jdouble src) { return src; }
    std::optional<double> JNI::ToDouble(JNIEnv*, jdouble src, bool) {
        return src; // not really optional at this point
    }

    int JNI::ToInt(JNIEnv* jenv, jint src) { return src; }
    std::optional<int> JNI::ToInt(JNIEnv* jenv, jint src, bool optional) {
        return src; // not really optional at this point
    }

    bool JNI::ToBool(JNIEnv* jenv, jboolean src) { return !!src; }
    std::optional<bool> JNI::ToBool(JNIEnv* jenv, jboolean src, bool optional) {
        return !!src; // not really optional at this point
    }

    String_ JNI::ToString(JNIEnv* jenv, const jstring src, bool optional) {
        const char* cstr = jenv->GetStringUTFChars(src, nullptr);
        if (!cstr) {
            REQUIRE(optional, "Empty input string");
            return String_();
        }
        String_ retval(cstr);
        jenv->ReleaseStringUTFChars(src, cstr);
        REQUIRE(optional || !retval.empty(), "Missing input string");
        return retval;
    }

    DateTime_ JNI::ToDateTime(JNIEnv* jenv, const jobject src, bool optional) {
        static const jclass date = jenv->FindClass("java/util/Date");
        REQUIRE(date, "Can't find date class");
        static const jmethodID getTime = jenv->GetMethodID(date, "getTime", "()J");
        REQUIRE(getTime, "Can't find getTime method");
        jlong time = jenv->CallLongMethod(src, getTime);
        return DateTime_(time);
    }
    Date_ JNI::ToDate(JNIEnv* jenv, const jobject src, bool optional) { return ToDateTime(jenv, src, optional).Date(); }

    Handle_<Storable_> JNI::ToHandleBase(JNIEnv* jenv, const jobject src, bool optional) {
        static const jclass THE_CLASS = jenv->FindClass("DA/Handle");
        REQUIRE(THE_CLASS, "Can't find handle class");
        static const jfieldID THE_FIELD = jenv->GetFieldID(THE_CLASS, "address_", "J");
        REQUIRE(THE_FIELD, "Can't find address field in handle");
        jlong address = jenv->GetLongField(src, THE_FIELD);
        auto pointer = reinterpret_cast<Handle_<Storable_>*>(address);
        return *pointer;
    }

    Cell_ JNI::ToCell(JNIEnv* jenv, const jobject src, bool optional) {
        static const jclass THE_CLASS = jenv->FindClass("DA/util/Cell");
        REQUIRE(THE_CLASS, "Can't find cell class");
        static const jfieldID TYPE_FIELD = jenv->GetFieldID(THE_CLASS, "type_", "I");
        REQUIRE(TYPE_FIELD, "Can't find type field in cell");
        jint type = jenv->GetIntField(src, TYPE_FIELD);

        static const jfieldID NUMBER_FIELD = jenv->GetFieldID(THE_CLASS, "d_", "D");
        static const jfieldID BOOL_FIELD = jenv->GetFieldID(THE_CLASS, "b_", "B");
        static const jfieldID DATETIME_FIELD = jenv->GetFieldID(THE_CLASS, "dt_", "Ljava/util/Date;");
        static const jfieldID STRING_FIELD = jenv->GetFieldID(THE_CLASS, "s_", "Ljava/lang/String;");

        switch (type) // numerical values of this switch are hard-coded on the Java side:  		EMPTY,		BOOLEAN, NUMBER,		DATE,		DATETIME,		STRING
        {
        case 0:
            REQUIRE(optional, "Empty input cell");
            return Cell_();
        case 1:
            return Cell_(jenv->GetBooleanField(src, BOOL_FIELD) != 0);
        case 2:
            return Cell_(jenv->GetDoubleField(src, NUMBER_FIELD));
        case 3:
            return Cell_(ToDateTime(jenv, jenv->GetObjectField(src, DATETIME_FIELD)).Date());
        case 4:
            return Cell_(ToDateTime(jenv, jenv->GetObjectField(src, DATETIME_FIELD)));
        case 5:
            return Cell_(ToString(jenv, static_cast<jstring>(jenv->GetObjectField(src, STRING_FIELD)), true));
        }
        THROW("Unrecognized cell type");
    }

    namespace {
        template <class J_, class E_, class T_>
        auto JNI__ToVector(JNIEnv* jenv, const J_ src, const E_ GET_ATlement, const T_ translate, bool optional)
            -> Vector_<decltype(translate(nullptr, nullptr))> {
            typedef Vector_<decltype(translate(jenv, nullptr))> ret_t;
            auto size = jenv->GetArrayLength(src);
            REQUIRE(size > 0 || optional, "Missing input vector");
            ret_t retval;
            retval.Resize(size); // Clang doesn't like the construct-from-size
            for (int ii = 0; ii < size; ++ii) {
                auto obj = GET_ATlement(jenv, src, ii);
                retval[ii] = translate(jenv, obj);
            }
            return retval;
        }
        static const auto GET_AT = [](JNIEnv* jenv, jobjectArray src, int ii) {
            return jenv->GetObjectArrayElement(src, ii);
        };

        JNI::HDict_ ToHDictionary(JNIEnv* jenv, const jobject src) { return JNI::HDict_(); }
    } // namespace

    Dictionary_ JNI::ToDictionary(JNIEnv* jenv, const jobject src, bool optional) {
        auto temp = ToHDictionary(jenv, src);
        REQUIRE(temp.objects_.empty(), "Dictionary can't contain handle types");
        return temp.atoms_;
    }

    Vector_<int> JNI::ToIntVector(JNIEnv* jenv, const jintArray src, bool optional) {
        auto size = jenv->GetArrayLength(src);
        REQUIRE(size > 0 || optional, "Missing input vector");
        auto vals = jenv->GetIntArrayElements(src, nullptr);
        Vector_<int> retval(vals, vals + size);
        jenv->ReleaseIntArrayElements(src, vals, 0);
        return retval;
    }

    Vector_<> JNI::ToDoubleVector(JNIEnv* jenv, const jdoubleArray src, bool optional) {
        auto size = jenv->GetArrayLength(src);
        REQUIRE(size > 0 || optional, "Missing input vector");
        jboolean isCopy;
        auto vals = jenv->GetDoubleArrayElements(src, &isCopy);
        Vector_<> retval(vals, vals + size);
        jenv->ReleaseDoubleArrayElements(src, vals, isCopy);
        return retval;
    }

    Vector_<String_> JNI::ToStringVector(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) {
            return ToString(jenv, reinterpret_cast<jstring>(obj), false);
        };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
    }
    Vector_<Date_> JNI::ToDateVector(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) { return ToDate(jenv, obj, false); };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
    }
    Vector_<Cell_> JNI::ToCellVector(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) { return ToCell(jenv, obj, true); };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
    }
    Vector_<Handle_<Storable_>> JNI::ToHandleBaseVector(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) { return ToHandleBase(jenv, obj, false); };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
    }
    Vector_<Dictionary_> JNI::ToDictionaryVector(JNIEnv* jenv, const jobjectArray src) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) { return ToDictionary(jenv, obj, false); };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, true);
    }

    Vector_<JNI::HDict_> JNI::ToHDictionaryVector(JNIEnv* jenv, jobjectArray src) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) { return ToHDictionary(jenv, obj); };
        return JNI__ToVector(jenv, src, GET_AT, TRANSLATE, true);
    }

    Matrix_<> JNI::ToDoubleMatrix(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) {
            return ToDoubleVector(jenv, static_cast<jdoubleArray>(obj), false);
        };
        auto vv = JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
        return Matrix::FromVectors(vv, false, false);
    }
    Matrix_<String_> JNI::ToStringMatrix(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) {
            return ToStringVector(jenv, static_cast<jobjectArray>(obj), false);
        };
        auto vv = JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
        return Matrix::FromVectors(vv, false, false);
    }
    Matrix_<Cell_> JNI::ToCellMatrix(JNIEnv* jenv, const jobjectArray src, bool optional) {
        static const auto TRANSLATE = [](JNIEnv* jenv, jobject obj) {
            return ToCellVector(jenv, static_cast<jobjectArray>(obj), false);
        };
        auto vv = JNI__ToVector(jenv, src, GET_AT, TRANSLATE, optional);
        return Matrix::FromVectors(vv, false, false);
    }

    //----------------------------------------------------------------------------
    // time for some outputs

    jstring JNI::CopyOut(JNIEnv* jenv, const String_& src) { return jenv->NewStringUTF(src.c_str()); }

    jobject JNI::CopyOut(JNIEnv* jenv, const Date_& src) { return CopyOut(jenv, DateTime_(src)); }

    jobject JNI::CopyOut(JNIEnv* jenv, const DateTime_& src) {
        static const jclass THE_CLASS = jenv->FindClass("java/util/Date");
        REQUIRE(THE_CLASS, "Can't find date class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(J)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find date initializer");
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, DateTime::MSec(src));
    }

    jobject JNI::CopyOutBase(JNIEnv* jenv, const Handle_<Storable_>& src) {
        static const jclass THE_CLASS = jenv->FindClass("DA/Handle");
        REQUIRE(THE_CLASS, "Can't find handle class");
        static const jmethodID THE_CONSTRUCTOR = jenv->GetMethodID(THE_CLASS, "<init>", "(I)V");
        REQUIRE(THE_CONSTRUCTOR, "Can't find handle initializer");
        auto passMem = new Handle_<Storable_>(src); // appear to leak this memory -- we get it back when Java finalizes
        return jenv->NewObject(THE_CLASS, THE_CONSTRUCTOR, reinterpret_cast<jlong>(passMem));
    }

    jobject JNI::CopyOut(JNIEnv* jenv, const Cell_& src) {
        static const jclass THE_CLASS = jenv->FindClass("DA/util/Cell");
        REQUIRE(THE_CLASS, "Can't find cell class");
        static const jmethodID INIT_EMPTY = jenv->GetMethodID(THE_CLASS, "<init>", "()V");
        static const jmethodID INIT_NUMBER = jenv->GetMethodID(THE_CLASS, "<init>", "(D)V");
        static const jmethodID INIT_BOOL = jenv->GetMethodID(THE_CLASS, "<init>", "(B)V");
        static const jmethodID INIT_DATETIME = jenv->GetMethodID(THE_CLASS, "<init>", "(Ljava/util/Date)V");
        static const jmethodID INIT_STRING = jenv->GetMethodID(THE_CLASS, "<init>", "(Ljava/lang/String)V");
        REQUIRE(INIT_EMPTY && INIT_NUMBER && INIT_BOOL && INIT_DATETIME && INIT_STRING, "Can't find cell initializers");

        if (Cell::IsBool(src))
            return jenv->NewObject(THE_CLASS, INIT_BOOL, Cell::ToBool(src));
        else if (Cell::IsDate(src))
            return jenv->NewObject(THE_CLASS, INIT_DATETIME, CopyOut(jenv, Cell::ToDate(src)));
        else if (Cell::IsDateTime(src))
            return jenv->NewObject(THE_CLASS, INIT_DATETIME, CopyOut(jenv, Cell::ToDateTime(src).Date()));
        else if (Cell::IsEmpty(src))
            return jenv->NewObject(THE_CLASS, INIT_EMPTY);
        else if (Cell::IsDouble(src))
            return jenv->NewObject(THE_CLASS, INIT_NUMBER, Cell::ToDouble(src));
        else if (Cell::IsInt(src))
            return jenv->NewObject(THE_CLASS, INIT_NUMBER, Cell::ToInt(src));
        else if (Cell::IsString(src))
            return jenv->NewObject(THE_CLASS, INIT_STRING, CopyOut(jenv, Cell::ToString(src)));
        else
            THROW("Unrecognized cell type");
    }

    jdoubleArray JNI::CopyOut(JNIEnv* jenv, const Vector_<>& src) {
        jdoubleArray retval = jenv->NewDoubleArray(src.size());
        jenv->SetDoubleArrayRegion(retval, 0, src.size(), &src[0]);
        return retval;
    }

    jintArray JNI::CopyOut(JNIEnv* jenv, const Vector_<int>& src) {
        auto temp = Apply(ConstructCast_<int, jint>(), src);
        jintArray retval = jenv->NewIntArray(src.size());
        jenv->SetIntArrayRegion(retval, 0, src.size(), &temp[0]);
        return retval;
    }

    namespace {
        template <class T_> jobjectArray CopyOutVector(JNIEnv* jenv, const Vector_<T_>& src) {
            jobjectArray retval = jenv->NewObjectArray(src.size(), nullptr, nullptr);
            for (int ii = 0; ii < src.size(); ++ii)
                jenv->SetObjectArrayElement(retval, ii, JNI::CopyOut(jenv, src[ii]));
            return retval;
        }

        template <class T_> jobjectArray CopyOutMatrix(JNIEnv* jenv, const Matrix_<T_>& src) {
            jobjectArray retval = jenv->NewObjectArray(src.Rows(), nullptr, nullptr);
            for (int ii = 0; ii < src.Rows(); ++ii)
                jenv->SetObjectArrayElement(retval, ii, JNI::CopyOut(jenv, Copy(src.Row(ii))));
            return retval;
        }
    } // namespace

    jobjectArray JNI::CopyOut(JNIEnv* jenv, const Vector_<String_>& src) { return CopyOutVector(jenv, src); }
    jobjectArray JNI::CopyOut(JNIEnv* jenv, const Vector_<Cell_>& src) { return CopyOutVector(jenv, src); }
    jobjectArray JNI::CopyOutBase(JNIEnv* jenv, const Vector_<Handle_<Storable_>>& src) {
        return CopyOutVector(jenv, src);
    }

    jobjectArray JNI::CopyOut(JNIEnv* jenv, const Matrix_<>& src) { return CopyOutMatrix(jenv, src); }
    jobjectArray JNI::CopyOut(JNIEnv* jenv, const Matrix_<String_>& src) { return CopyOutMatrix(jenv, src); }
    jobjectArray JNI::CopyOut(JNIEnv* jenv, const Matrix_<Cell_>& src) { return CopyOutMatrix(jenv, src); }

    jobject JNI::Error(JNIEnv* jenv, const char* msg, const char* arg_name) {
        static const jclass J_EX = jenv->FindClass("java/lang/Exception");
        jenv->ThrowNew(J_EX, msg);
        return nullptr;
    }
}
