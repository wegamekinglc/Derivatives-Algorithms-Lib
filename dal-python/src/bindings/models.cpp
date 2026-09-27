//
// models.cpp - model data bindings for the script valuation models
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/matrix/matrixs.hpp>

#include <dal-public/src/models.hpp>

using namespace Dal;

void init_bindings_models(py::module_& m) {
    py::class_<HybridComponentData_, Storable_, std::shared_ptr<HybridComponentData_>>(m, "HybridComponentData_");
    py::class_<HybridCorrelationData_, Storable_, std::shared_ptr<HybridCorrelationData_>>(m, "HybridCorrelationData_");

    m.def(
        "CorrelatedBSModelData_New",
        [](const py::iterable& indices, const py::iterable& spots, const py::iterable& vols, const py::iterable& divs, double rate,
           const Matrix_<>& correlations) -> std::shared_ptr<ModelData_> {
            Vector_<String_> names;
            Vector_<> spotValues, volValues, divValues;
            for (const auto item : indices)
                names.emplace_back(py::cast<std::string>(item));
            for (const auto item : spots)
                spotValues.push_back(py::cast<double>(item));
            for (const auto item : vols)
                volValues.push_back(py::cast<double>(item));
            for (const auto item : divs)
                divValues.push_back(py::cast<double>(item));
            return std::const_pointer_cast<ModelData_>(Handle_<ModelData_>(
                new CorrelatedBSModelData_("CorrelatedBSModelData_", names, spotValues, volValues, divValues, rate, correlations)));
        },
        py::arg("indices"), py::arg("spots"), py::arg("vols"), py::arg("divs"), py::arg("rate"), py::arg("correlations"));

    m.def(
        "HybridBSEquityData_New",
        [](const std::string& name, const std::string& index, const std::string& currency, const std::string& factor, double spot, double vol,
           double div) -> std::shared_ptr<HybridComponentData_> {
            return std::make_shared<HybridBSEquityData_>(String_(name), String_(index), String_(currency), String_(factor), spot, vol, div);
        },
        py::arg("name"), py::arg("index"), py::arg("currency"), py::arg("factor"), py::arg("spot"), py::arg("vol"), py::arg("div"));

    m.def(
        "HybridDeterministicRateData_New",
        [](const std::string& name, const std::string& currency, double rate) -> std::shared_ptr<HybridComponentData_> {
            return std::make_shared<HybridDeterministicRateData_>(String_(name), String_(currency), rate);
        },
        py::arg("name"), py::arg("currency"), py::arg("rate"));

    m.def(
        "HybridLogDfRateData_New",
        [](const std::string& name, const std::string& currency, const py::iterable& times, const py::iterable& logDF,
           const std::string& scheme) -> std::shared_ptr<HybridComponentData_> {
            Vector_<> modelTimes, nodeLogDF;
            for (const auto item : times)
                modelTimes.push_back(py::cast<double>(item));
            for (const auto item : logDF)
                nodeLogDF.push_back(py::cast<double>(item));
            return std::make_shared<HybridLogDfRateData_>(String_(name), String_(currency), modelTimes, nodeLogDF, String_(scheme));
        },
        py::arg("name"), py::arg("currency"), py::arg("times"), py::arg("log_df"), py::arg("scheme") = "LOG_LINEAR");

    m.def(
        "HybridLogDfRateDataFromCurve_New",
        [](const std::string& name, const std::shared_ptr<DiscountCurve_>& curve, const Date_& evaluationDate, const py::iterable& nodeDates,
           const std::string& scheme) -> std::shared_ptr<HybridComponentData_> {
            REQUIRE(curve, "InvalidHybridCurve: source discount curve is required");
            Vector_<Date_> dates;
            for (const auto item : nodeDates)
                dates.push_back(py::cast<Date_>(item));
            return std::const_pointer_cast<HybridComponentData_>(
                NewHybridLogDfRateDataFromCurve(String_(name), *curve, evaluationDate, dates, String_(scheme)));
        },
        py::arg("name"), py::arg("curve"), py::arg("evaluation_date"), py::arg("node_dates"), py::arg("scheme") = "LOG_LINEAR");

    m.def(
        "HybridConstantCorrelationData_New",
        [](const std::string& name, const py::iterable& factors, const Matrix_<>& correlations) -> std::shared_ptr<HybridCorrelationData_> {
            Vector_<String_> names;
            for (const auto item : factors)
                names.emplace_back(py::cast<std::string>(item));
            return std::make_shared<HybridConstantCorrelationData_>(String_(name), names, correlations);
        },
        py::arg("name"), py::arg("factors"), py::arg("correlations"));

    m.def(
        "HybridModelData_New",
        [](const std::string& name, const std::string& domesticCurrency, const py::iterable& components,
           const std::shared_ptr<HybridCorrelationData_>& correlation) -> std::shared_ptr<ModelData_> {
            Vector_<Handle_<HybridComponentData_>> handles;
            for (const auto item : components)
                handles.emplace_back(std::shared_ptr<const HybridComponentData_>(py::cast<std::shared_ptr<HybridComponentData_>>(item)));
            const Handle_<HybridCorrelationData_> correlationHandle{std::shared_ptr<const HybridCorrelationData_>(correlation)};
            return std::const_pointer_cast<ModelData_>(
                NewHybridModelData(String_(name), HybridSettings_{String_(domesticCurrency), handles, correlationHandle}));
        },
        py::arg("name"), py::arg("domestic_currency"), py::arg("components"), py::arg("correlation"));

    m.def(
        "BSModelData_New",
        [](double spot, double vol, double rate, double div) -> std::shared_ptr<ModelData_> {
            return std::const_pointer_cast<ModelData_>(NewBSModelData(String_("BSModelData_"), spot, vol, rate, div));
        },
        py::arg("spot"), py::arg("vol"), py::arg("rate"), py::arg("div"));

    m.def(
        "DupireModelData_New",
        [](double spot, double rate, double repo, const py::iterable& spots, const py::iterable& times,
           const Matrix_<>& vols) -> std::shared_ptr<ModelData_> {
            // Convert Python iterables to Vector_<> for the factory function
            Vector_<> new_spots;
            for (auto item : spots)
                new_spots.push_back(py::cast<double>(item));

            Vector_<> new_times;
            for (auto item : times)
                new_times.push_back(py::cast<double>(item));

            return std::const_pointer_cast<ModelData_>(NewDupireModelData(String_("DupireModelData_"), spot, rate, repo, new_spots, new_times, vols));
        },
        py::arg("spot"), py::arg("rate"), py::arg("repo"), py::arg("spots"), py::arg("times"), py::arg("vols"));
}
