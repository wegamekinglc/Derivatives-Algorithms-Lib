//
// Created by wegamekinglc on 2020/11/24.
//

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <dal/platform/platform.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/cellutils.hpp>
#include <dal/math/interp/interp2d.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/bag.hpp>
#include <dal/storage/box.hpp>
#include <dal/storage/splat.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

namespace {
    std::filesystem::path UniqueTempDir(const char* test_name) {
        const auto dir = std::filesystem::temp_directory_path() / (std::string("dal_splat_test_") + test_name);
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    void WriteTextFile(const std::filesystem::path& file, const Vector_<String_>& lines) {
        std::ofstream dst(file);
        for (const auto& line : lines)
            dst << line << '\n';
    }
} // namespace

TEST(StorageTest, TestSplatAndUnSplat) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(StorageTest, TestSplatFileAndUnSplatFile) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    SplatFile("src.csv", *src);
    Handle_<Storable_> rtn = UnSplatFile("src.csv", true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
    File::Remove("src.csv");
}

TEST(StorageTest, TestSplatBoxRoundTripMixedCells) {
    Matrix_<Cell_> mat(2, 3);
    mat(0, 0) = 1.5;
    mat(0, 1) = String_("hello");
    mat(0, 2) = true;
    mat(1, 0) = Date_(2023, 1, 22);
    mat(1, 1) = DateTime_(Date_(2023, 1, 22), 0.5);
    // mat(1, 2) intentionally left empty

    Box_ src("mybox", mat);
    auto dst = Splat(src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Box_> val(std::dynamic_pointer_cast<const Box_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_EQ(val->Name(), String_("mybox"));
    Matrix_<Cell_> contents = val->contents_;
    ASSERT_EQ(contents.Rows(), 2);
    ASSERT_EQ(contents.Cols(), 3);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(contents(0, 0)), 1.5);
    ASSERT_EQ(Cell::ToString(contents(0, 1)), String_("hello"));
    ASSERT_TRUE(Cell::ToBool(contents(0, 2)));
    ASSERT_EQ(Cell::ToDate(contents(1, 0)), Date_(2023, 1, 22));
    ASSERT_EQ(Cell::ToDateTime(contents(1, 1)), DateTime_(Date_(2023, 1, 22), 0.5));
    ASSERT_TRUE(Cell::IsEmpty(contents(1, 2)));
}

TEST(StorageTest, TestSplatInterp2RoundTripMatrix) {
    Vector_<> x = {1., 2.};
    Vector_<> y = {1., 2., 3.};
    Matrix_<> f(2, 3);
    f(0, 0) = 1.1;
    f(0, 1) = 1.2;
    f(0, 2) = 1.3;
    f(1, 0) = 2.1;
    f(1, 1) = 2.2;
    f(1, 2) = 2.3;

    Handle_<Interp2_> src(Interp::NewLinear2("interp2", x, y, f));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp2_> val(std::dynamic_pointer_cast<const Interp2_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(1.5, 2.5), (*val)(1.5, 2.5));
    ASSERT_DOUBLE_EQ((*src)(1.0, 1.0), 1.1);
    ASSERT_DOUBLE_EQ((*val)(2.0, 3.0), 2.3);
}

TEST(StorageTest, TestSplatDiscountCurveNestedBaseRoundTrip) {
    const Vector_<Date_> knots{Date_(2026, 2, 15), Date_(2026, 7, 15), Date_(2027, 1, 15)};
    const Handle_<DiscountCurve_> base(
        NewDiscountPWC("base", "USD", PiecewiseConstant_(knots, Vector_<>{0.01, 0.012, 0.014})));
    const Handle_<DiscountCurve_> spread(
        NewDiscountPWC("spread", "USD", PiecewiseConstant_(knots, Vector_<>{0.001, 0.002, 0.003}), base));

    auto dst = Splat(*spread);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    const auto curve = std::dynamic_pointer_cast<const Tape::DiscountPWC_<double>>(rtn);

    ASSERT_TRUE(curve);
    ASSERT_TRUE(curve->Base());
    ASSERT_EQ(curve->KnotDates(), knots);
    ASSERT_EQ(curve->FRight(), (Vector_<>{0.001, 0.002, 0.003}));
    ASSERT_DOUBLE_EQ((*curve)(Date_(2026, 1, 15), Date_(2026, 10, 15)),
                     (*spread)(Date_(2026, 1, 15), Date_(2026, 10, 15)));
}

TEST(StorageTest, TestSplatBagSharesRepeatedObjects) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    Handle_<Storable_> shared(Interp::NewLinear("interp", x, f));

    std::multimap<String_, Handle_<Storable_>> map;
    map.insert(std::make_pair(String_("first"), shared));
    map.insert(std::make_pair(String_("second"), shared));

    Bag_ bag("bag", map);
    auto dst = Splat(bag);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Bag_> val(std::dynamic_pointer_cast<const Bag_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    auto first = val->contents_.find("first");
    auto second = val->contents_.find("second");
    ASSERT_TRUE(first != val->contents_.end());
    ASSERT_TRUE(second != val->contents_.end());
    // the repeated object is stored once and referenced by tag, so both
    // entries resolve to the same restored instance
    ASSERT_EQ(first->second.get(), second->second.get());
    auto interp = std::dynamic_pointer_cast<const Interp1_>(first->second);
    ASSERT_TRUE(interp != nullptr);
    ASSERT_DOUBLE_EQ((*interp)(2.5), 2.6);
}

TEST(StorageTest, TestSplatInterpEmptyNameOmitsOptionalChild) {
    Vector_<> x = {1., 2., 3.};
    Vector_<> f = {2.5, 3.5, 1.7};

    Handle_<Interp1_> src(Interp::NewLinear("", x, f));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_EQ(val->Name(), String_(""));
    ASSERT_DOUBLE_EQ((*src)(1.5), (*val)(1.5));
}

TEST(StorageTest, TestUnSplatUnknownTypeThrows) {
    Matrix_<Cell_> blob(1, 1);
    blob(0, 0) = String_("~NoSuchType_v1");

    ASSERT_THROW(UnSplat(blob, true), Dal::Exception_);
}

TEST(StorageTest, TestUnSplatMissingTypeMarkerThrows) {
    Matrix_<Cell_> blob(1, 1);
    blob(0, 0) = String_("no type marker here");

    ASSERT_THROW(UnSplat(blob, true), Dal::Exception_);
}

TEST(StorageTest, TestUnSplatMissingRequiredChildThrows) {
    // an Interp1Linear_v1 blob with the required "f" child removed
    Matrix_<Cell_> blob(1, 4);
    blob(0, 0) = String_("~Interp1Linear_v1");
    blob(0, 1) = String_("x");
    blob(0, 2) = 1.0;
    blob(0, 3) = 2.0;

    ASSERT_THROW(UnSplat(blob, true), Dal::Exception_);
}

TEST(StorageTest, TestUnSplatNonNumericValueThrows) {
    // a boolean cell where the "f" vector expects numbers
    Matrix_<Cell_> blob(2, 4);
    blob(0, 0) = String_("~Interp1Linear_v1");
    blob(0, 1) = String_("f");
    blob(0, 2) = true;
    blob(0, 3) = 3.5;
    blob(1, 1) = String_("x");
    blob(1, 2) = 1.0;
    blob(1, 3) = 2.0;

    ASSERT_THROW(UnSplat(blob, true), Dal::Exception_);
}

TEST(StorageTest, TestUnSplatMultiLineVectorThrows) {
    // a stray value under the "x" row makes "x" a multi-line entry
    Matrix_<Cell_> blob(4, 4);
    blob(0, 0) = String_("~Interp1Linear_v1");
    blob(0, 1) = String_("f");
    blob(0, 2) = 2.5;
    blob(0, 3) = 3.5;
    blob(1, 1) = String_("name");
    blob(1, 2) = String_("interp");
    blob(2, 1) = String_("x");
    blob(2, 2) = 1.0;
    blob(2, 3) = 2.0;
    blob(3, 2) = 3.0;

    ASSERT_THROW(UnSplat(blob, true), Dal::Exception_);
}

TEST(StorageTest, TestUnSplatDateVectorAcceptsIsoStrings) {
    // date cells may also be supplied as ISO strings
    Matrix_<Cell_> blob(3, 3);
    blob(0, 0) = String_("~DiscountPWC_v1");
    blob(0, 1) = String_("ccy");
    blob(0, 2) = String_("USD");
    blob(1, 1) = String_("knotDates");
    blob(1, 2) = String_("2026-02-15");
    blob(2, 1) = String_("rightVals");
    blob(2, 2) = 0.01;

    Handle_<Storable_> rtn = UnSplat(blob, true);
    const auto curve = std::dynamic_pointer_cast<const Tape::DiscountPWC_<double>>(rtn);

    ASSERT_TRUE(curve);
    ASSERT_EQ(curve->KnotDates(), (Vector_<Date_>{Date_(2026, 2, 15)}));
    ASSERT_EQ(curve->FRight(), (Vector_<>{0.01}));
}

TEST(StorageTest, TestSplatFileRoundTripThroughTempDir) {
    const Vector_<Date_> knots{Date_(2026, 2, 15), Date_(2026, 7, 15)};
    const Handle_<DiscountCurve_> src(
        NewDiscountPWC("curve", "USD", PiecewiseConstant_(knots, Vector_<>{0.01, 0.012})));

    const auto dir = UniqueTempDir("file_round_trip");
    const auto file = dir / "curve.csv";
    const String_ fileName(file.string());

    SplatFile(fileName, *src);
    Handle_<Storable_> rtn = UnSplatFile(fileName, true);
    const auto curve = std::dynamic_pointer_cast<const Tape::DiscountPWC_<double>>(rtn);

    ASSERT_TRUE(curve);
    ASSERT_EQ(curve->Name(), String_("curve"));
    ASSERT_EQ(curve->KnotDates(), knots);
    ASSERT_DOUBLE_EQ((*curve)(Date_(2026, 1, 15), Date_(2026, 10, 15)),
                     (*src)(Date_(2026, 1, 15), Date_(2026, 10, 15)));

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}

TEST(StorageTest, TestUnSplatFileCorruptContentThrows) {
    const auto dir = UniqueTempDir("file_corrupt");

    {
        // unknown type name
        const auto file = dir / "unknown_type.csv";
        WriteTextFile(file, {String_("~NoSuchType_v1,x,1.0")});
        ASSERT_THROW(UnSplatFile(String_(file.string()), true), Dal::Exception_);
    }
    {
        // truncated blob: required "f" child is missing
        const auto file = dir / "truncated.csv";
        WriteTextFile(file, {String_("~Interp1Linear_v1,x,1.0,2.0")});
        ASSERT_THROW(UnSplatFile(String_(file.string()), true), Dal::Exception_);
    }

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}

TEST(StorageTest, TestUnSplatFileMissingFileThrows) {
    const auto dir = UniqueTempDir("file_missing");
    const auto file = dir / "no_such_file.csv";
    ASSERT_FALSE(std::filesystem::exists(file));

    ASSERT_THROW(UnSplatFile(String_(file.string()), true), Dal::Exception_);

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}

TEST(StorageTest, TestUnSplatFileEmptyFileThrows) {
    const auto dir = UniqueTempDir("file_empty");
    const auto file = dir / "empty.csv";
    WriteTextFile(file, {});
    ASSERT_TRUE(std::filesystem::exists(file));

    ASSERT_THROW(UnSplatFile(String_(file.string()), true), Dal::Exception_);

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}

TEST(StorageTest, TestSplatFileRoundTripPreservesBoolAndDoublePrecision) {
    const double precise = 0.1 + 0.2;
    Matrix_<Cell_> mat(1, 3);
    mat(0, 0) = true;
    mat(0, 1) = false;
    mat(0, 2) = precise;

    Box_ src("precise", mat);

    const auto dir = UniqueTempDir("file_typed_round_trip");
    const auto file = dir / "box.csv";
    const String_ fileName(file.string());

    SplatFile(fileName, src);
    Handle_<Storable_> rtn = UnSplatFile(fileName, true);
    Handle_<Box_> val(std::dynamic_pointer_cast<const Box_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    const Matrix_<Cell_> contents = val->contents_;
    ASSERT_EQ(contents.Rows(), 1);
    ASSERT_EQ(contents.Cols(), 3);
    ASSERT_TRUE(Cell::IsBool(contents(0, 0)));
    ASSERT_TRUE(Cell::ToBool(contents(0, 0)));
    ASSERT_TRUE(Cell::IsBool(contents(0, 1)));
    ASSERT_FALSE(Cell::ToBool(contents(0, 1)));
    ASSERT_TRUE(Cell::IsDouble(contents(0, 2)));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(contents(0, 2)), precise);

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}

TEST(StorageTest, TestUnSplatFileRecognizesUppercaseBool) {
    // pre-existing splat files spell booleans TRUE/FALSE in uppercase
    const auto dir = UniqueTempDir("file_upper_bool");
    const auto file = dir / "box.csv";
    WriteTextFile(file, {String_("~Box,contents,TRUE,FALSE"), String_(",name,mybox")});

    Handle_<Storable_> rtn = UnSplatFile(String_(file.string()), true);
    Handle_<Box_> val(std::dynamic_pointer_cast<const Box_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_EQ(val->Name(), String_("mybox"));
    ASSERT_TRUE(Cell::IsBool(val->contents_(0, 0)));
    ASSERT_TRUE(Cell::ToBool(val->contents_(0, 0)));
    ASSERT_TRUE(Cell::IsBool(val->contents_(0, 1)));
    ASSERT_FALSE(Cell::ToBool(val->contents_(0, 1)));

    std::filesystem::remove_all(dir);
    ASSERT_FALSE(std::filesystem::exists(dir));
}
