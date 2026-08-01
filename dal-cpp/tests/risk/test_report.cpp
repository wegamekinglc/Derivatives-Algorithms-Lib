//
// Created by wegam on 2024/2/19.
//

#include <gtest/gtest.h>

#include <string>

#include <dal/risk/report.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(RiskTest, TestReportSet) {
    const Vector_<Report::Axis_> axes = {{"header1", 1, {"a"}},
                                         {"header2", 2, {"b", "c"}}};
    Report_ report("test_report", axes);
    const auto axe_names = report.Axes();
    ASSERT_EQ(axe_names[0], String_("header1"));
    ASSERT_EQ(axe_names[1], String_("header2"));

    ASSERT_EQ(report.Size("header1"), 1);
    ASSERT_EQ(report.Size("header2"), 2);

    ASSERT_EQ(report.Header("header1").values_.Cols(), 1);
    ASSERT_EQ(report.Header("header2").values_.Cols(), 2);

    auto address = report.MakeAddress();
    address["header1"] = 0;
    address["header2"] = 0;
    report[address] = 2.0;
    ASSERT_NEAR(report[address], 2.0, 1e-8);

    address["header1"] = 0;
    address["header2"] = 1;
    report[address] = 3.0;
    ASSERT_NEAR(report[address], 3.0, 1e-8);
}

TEST(RiskTest, TestReportHeader) {
    const Vector_<Report::Axis_> axes = {{"header1", 1, {"a"}},
                                         {"header2", 2, {"b", "c"}}};
    Report_ report("test_report", axes);
    const auto axe_names = report.Axes();

    report.AddHeaderRow("header1", 0, {Cell_(1.0)});
    report.AddHeaderRow("header2", 0, {Cell_(1.0), Cell_("sample1")});
    report.AddHeaderRow("header2", 1, {Cell_(2.0), Cell_("sample2")});

    auto header = report.Header("header1");
    ASSERT_EQ(header.labels_, Vector_<String_>(1, String_("a")));

    header = report.Header("header2");
    ASSERT_EQ(header.labels_, Vector_<String_>({"b", "c"}));
}

TEST(RiskTest, TestReportRejectsEmptyAxes) {
    const Vector_<Report::Axis_> no_axes;
    ASSERT_THROW(Report_("test_report", no_axes), Dal::Exception_);
}

TEST(RiskTest, TestReportRejectsZeroSizeAfterFirstAxis) {
    const Vector_<Report::Axis_> axes = {{"header1", 1, {"a"}},
                                         {"header2", 0, {}}};
    ASSERT_THROW(Report_("test_report", axes), Dal::Exception_);
}

TEST(RiskTest, TestReportRejectsUnknownAxisLookup) {
    const Vector_<Report::Axis_> axes = {{"header1", 1, {"a"}}};
    Report_ report("test_report", axes);
    ASSERT_THROW((void)report.Size("bogus"), Dal::Exception_);
    ASSERT_THROW((void)report.Header("bogus"), Dal::Exception_);

    auto address = report.MakeAddress();
    ASSERT_THROW((void)address["bogus"], Dal::Exception_);
}

TEST(RiskTest, TestReportMultiAxisLayout) {
    const Vector_<Report::Axis_> axes = {{"header1", 2, {"a", "b"}},
                                         {"header2", 3, {"c", "d", "e"}}};
    Report_ report("test_report", axes);
    ASSERT_EQ(report.Size("header1"), 2);
    ASSERT_EQ(report.Size("header2"), 3);

    auto address = report.MakeAddress();
    double value = 0.0;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) {
            address["header1"] = i;
            address["header2"] = j;
            report[address] = value;
            value += 1.0;
        }

    value = 0.0;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) {
            address["header1"] = i;
            address["header2"] = j;
            ASSERT_NEAR(report[address], value, 1e-10);
            value += 1.0;
        }
}

TEST(RiskTest, TestReportConstAccessRejectsOutOfRangeAddress) {
    const Vector_<Report::Axis_> axes = {{"header1", 2, {"a", "b"}},
                                         {"header2", 3, {"c", "d", "e"}}};
    const Report_ report("test_report", axes);

    auto address = report.MakeAddress();
    address["header1"] = 1;
    address["header2"] = 5;
    ASSERT_THROW((void)report[address], Dal::Exception_);
}

TEST(RiskTest, TestReportFirstAxisGrowsOnWrite) {
    const Vector_<Report::Axis_> axes = {{"trade", 0, {}},
                                         {"view", 2, {"x", "y"}}};
    Report_ report("test_report", axes);
    ASSERT_EQ(report.Size("trade"), 0);
    ASSERT_EQ(report.Size("view"), 2);

    auto address = report.MakeAddress();
    address["trade"] = 1;
    address["view"] = 1;
    report[address] = 5.0;

    ASSERT_EQ(report.Size("trade"), 2);
    ASSERT_NEAR(report[address], 5.0, 1e-10);

    address["trade"] = 0;
    address["view"] = 0;
    ASSERT_NEAR(report[address], 0.0, 1e-10);
}

TEST(RiskTest, TestReportSetAll) {
    const Vector_<Report::Axis_> axes = {{"header1", 2, {"a", "b"}},
                                         {"header2", 3, {"c", "d", "e"}}};
    Report_ report("test_report", axes);

    {
        // 5 values is not a multiple of the first stride (3)
        ASSERT_THROW(report.SetAll(Vector_<>(5, 1.0)), Dal::Exception_);
    }
    {
        // 3 values would silently drop existing storage
        ASSERT_THROW(report.SetAll(Vector_<>(3, 1.0)), Dal::Exception_);
    }
    {
        Vector_<> all(6);
        for (int i = 0; i < 6; ++i)
            all[i] = 10.0 + i;
        report.SetAll(all);

        auto address = report.MakeAddress();
        address["header1"] = 1;
        address["header2"] = 2;
        ASSERT_NEAR(report[address], 15.0, 1e-10);
    }
}

TEST(RiskTest, TestReportAddHeaderRowRejectsWrongWidth) {
    const Vector_<Report::Axis_> axes = {{"header1", 2, {"a", "b"}}};
    Report_ report("test_report", axes);
    try {
        report.AddHeaderRow("header1", 0, {Cell_(1.0)});
        FAIL() << "Expected AddHeaderRow to reject the wrong width";
    } catch (const Dal::Exception_& e) {
        const std::string message(e.what());
        ASSERT_NE(message.find("Wrong number (= 1) of labels (= 2)"), std::string::npos);
        ASSERT_NE(message.find("nLabels = 2"), std::string::npos);
    }
}

TEST(RiskTest, TestReportAddHeaderRowExtendsValues) {
    const Vector_<Report::Axis_> axes = {{"header1", 1, {"a"}}};
    Report_ report("test_report", axes);

    report.AddHeaderRow("header1", 2, {Cell_(42.0)});
    const auto& header = report.Header("header1");
    ASSERT_EQ(header.values_.Rows(), 3);
    ASSERT_NEAR(Cell::ToDouble(header.values_(2, 0)), 42.0, 1e-10);
}

TEST(RiskTest, TestReportRoundTripsThroughJSON) {
    const Vector_<Report::Axis_> axes = {{"header1", 2, {"a", "b"}},
                                         {"header2", 2, {"c", "d"}}};
    Report_ report("test_report", axes);
    report.AddHeaderRow("header1", 0, {Cell_(1.0), Cell_(10.0)});
    report.AddHeaderRow("header1", 1, {Cell_(2.0), Cell_(20.0)});
    report.AddHeaderRow("header2", 0, {Cell_("sample1"), Cell_("sample2")});
    report.AddHeaderRow("header2", 1, {Cell_("sample3"), Cell_("sample4")});

    auto address = report.MakeAddress();
    address["header1"] = 0;
    address["header2"] = 1;
    report[address] = 3.5;
    address["header1"] = 1;
    address["header2"] = 0;
    report[address] = -2.5;

    const String_ dst = JSON::WriteString(report);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    auto restored = std::dynamic_pointer_cast<const Report_>(rtn);
    ASSERT_TRUE(restored != nullptr);
    ASSERT_EQ(restored->Name(), String_("test_report"));
    ASSERT_EQ(restored->Axes(), report.Axes());
    ASSERT_EQ(restored->Size("header1"), 2);
    ASSERT_EQ(restored->Size("header2"), 2);
    ASSERT_EQ(restored->Header("header2").labels_, Vector_<String_>({"c", "d"}));

    address["header1"] = 0;
    address["header2"] = 1;
    ASSERT_NEAR((*restored)[address], 3.5, 1e-10);
    address["header1"] = 1;
    address["header2"] = 0;
    ASSERT_NEAR((*restored)[address], -2.5, 1e-10);
}
