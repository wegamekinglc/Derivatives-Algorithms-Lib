//
// Created by wegam on 24-2-19.
//

#include <gtest/gtest.h>
#include <dal/risk/report.hpp>

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