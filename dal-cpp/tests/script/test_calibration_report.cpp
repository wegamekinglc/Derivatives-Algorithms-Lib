#include <gtest/gtest.h>
#include <limits>

#include <dal/calibration/protocol.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>

TEST(CalibrationReportTest, TestPartialReportRemainsUnavailableAndRejectsArchive) {
    const Dal::CalibrationReport_ report(Dal::CalibrationStage_("Input"));
    ASSERT_TRUE(report.Data().observations_.empty());
    ASSERT_FALSE(report.Data().quoteJacobian_);
    ASSERT_FALSE(report.Data().summary_.evaluations_);
    ASSERT_FALSE(report.QuoteRiskAvailable());
    ASSERT_THROW(Dal::JSON::WriteString(report), Dal::CalibrationError_);
}

TEST(CalibrationReportTest, TestSnapshotCopiesCallerContainers) {
    Dal::CalibrationReportSnapshot_ data;
    data.objective_.theta0_ = {.02};
    data.objective_.weightMatrix_ = Dal::Matrix_<>(1, 1);
    (*data.objective_.weightMatrix_)(0, 0) = 3.;
    const Dal::CalibrationReport_ report(Dal::CalibrationStage_("Input"), {}, {}, data);
    data.objective_.theta0_[0] = 100.;
    (*data.objective_.weightMatrix_)(0, 0) = 999.;
    ASSERT_DOUBLE_EQ(report.Data().objective_.theta0_[0], .02);
    ASSERT_DOUBLE_EQ((*report.Data().objective_.weightMatrix_)(0, 0), 3.);
}

TEST(CalibrationReportTest, TestNonfiniteObjectiveAndSimulationAreRejected) {
    const double infinity = std::numeric_limits<double>::infinity();
    Dal::CalibrationReportSnapshot_ data;
    data.objective_.theta0_ = {infinity};
    ASSERT_THROW(Dal::CalibrationReport_(Dal::CalibrationStage_("Input"), {}, {}, data), Dal::CalibrationError_);
    data.objective_.theta0_.clear();
    data.execution_.simulation_ = Dal::CalibrationReportSimulation_{};
    data.execution_.simulation_->smooth_ = infinity;
    ASSERT_THROW(Dal::CalibrationReport_(Dal::CalibrationStage_("Input"), {}, {}, data), Dal::CalibrationError_);
}
