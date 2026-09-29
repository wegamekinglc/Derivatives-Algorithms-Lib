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
    py::class_<GSRCurveData_, Storable_, std::shared_ptr<GSRCurveData_>>(m, "GSRCurveData_");
    py::class_<GSRVolData_, Storable_, std::shared_ptr<GSRVolData_>>(m, "GSRVolData_");
    py::class_<LocalVolSurfaceData_, Storable_, std::shared_ptr<LocalVolSurfaceData_>>(m, "LocalVolSurfaceData_");

    const auto dates = [](const py::iterable& input) {
        Vector_<Date_> result;
        for (const auto item : input)
            result.push_back(py::cast<Date_>(item));
        return result;
    };
    const auto numbers = [](const py::iterable& input) {
        Vector_<> result;
        for (const auto item : input)
            result.push_back(py::cast<double>(item));
        return result;
    };
    const auto strings = [](const py::iterable& input) {
        Vector_<String_> result;
        for (const auto item : input)
            result.emplace_back(py::cast<std::string>(item));
        return result;
    };

    m.def("GSRCurveData_New",
          [=](const std::string& name, const Date_& evaluationDate, const std::string& currency, const py::iterable& nodeDates,
              const py::iterable& discountLogDF, const py::iterable& projectionTenors, const Matrix_<>& projectionLogDF) {
              return std::const_pointer_cast<GSRCurveData_>(NewGSRCurveData(String_(name), evaluationDate, String_(currency), dates(nodeDates),
                                                                          numbers(discountLogDF), strings(projectionTenors), projectionLogDF));
          },
          py::arg("name"), py::arg("evaluation_date"), py::arg("currency"), py::arg("node_dates"), py::arg("discount_log_df"),
          py::arg("projection_tenors"), py::arg("projection_log_df"));

    m.def("GSRCurveDataFromYieldCurve_New",
          [=](const std::string& name, const std::shared_ptr<YieldCurve_>& source, const Date_& evaluationDate,
              const py::iterable& nodeDates, const py::iterable& projectionTenors) {
              REQUIRE(source, "InvalidGSRCurve: source yield curve is required");
              return std::const_pointer_cast<GSRCurveData_>(NewGSRCurveDataFromYieldCurve(
                  String_(name), *source, evaluationDate, dates(nodeDates), strings(projectionTenors)));
          },
          py::arg("name"), py::arg("source"), py::arg("evaluation_date"), py::arg("node_dates"), py::arg("projection_tenors"));

    m.def("GSRVolData_New",
          [=](const std::string& name, const py::iterable& gKnotDates, const py::iterable& gValues, const py::iterable& hKnotDates,
              const py::iterable& hValues) {
              return std::const_pointer_cast<GSRVolData_>(NewGSRVolData(String_(name), dates(gKnotDates), numbers(gValues), dates(hKnotDates),
                                                                        numbers(hValues)));
          },
          py::arg("name"), py::arg("g_knot_dates"), py::arg("g_values"), py::arg("h_knot_dates"), py::arg("h_values"));

    m.def("GSRModelData_New",
          [](const std::string& name, const std::shared_ptr<GSRCurveData_>& curve,
             const std::shared_ptr<GSRVolData_>& vol) -> std::shared_ptr<ModelData_> {
              REQUIRE(curve && vol, "InvalidGSRModel: curve and volatility data are required");
              return std::const_pointer_cast<ModelData_>(NewGSRModelData(
                  String_(name), Handle_<GSRCurveData_>(std::shared_ptr<const GSRCurveData_>(curve)),
                  Handle_<GSRVolData_>(std::shared_ptr<const GSRVolData_>(vol))));
          },
          py::arg("name"), py::arg("curve"), py::arg("vol"));

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
        "LocalVolSurfaceData_New",
        [=](const std::string& name, const py::iterable& spots, const py::iterable& times,
            const Matrix_<>& vols) -> std::shared_ptr<LocalVolSurfaceData_> {
            return std::const_pointer_cast<LocalVolSurfaceData_>(NewLocalVolSurfaceData(String_(name), numbers(spots), numbers(times), vols));
        },
        py::arg("name"), py::arg("spots"), py::arg("times"), py::arg("vols"));

    m.def(
        "HybridLocalVolEquityData_New",
        [](const std::string& name, const std::string& index, const std::string& currency, const std::string& factor, double spot, double div,
           const std::shared_ptr<LocalVolSurfaceData_>& surface, double maxStep) -> std::shared_ptr<HybridComponentData_> {
            REQUIRE(surface, "InvalidHybridComponent: local-vol surface is required");
            return std::const_pointer_cast<HybridComponentData_>(
                NewHybridLocalVolEquityData(String_(name), String_(index), String_(currency), String_(factor), spot, div,
                                            Handle_<LocalVolSurfaceData_>(std::shared_ptr<const LocalVolSurfaceData_>(surface)), maxStep));
        },
        py::arg("name"), py::arg("index"), py::arg("currency"), py::arg("factor"), py::arg("spot"), py::arg("div"), py::arg("surface"),
        py::arg("max_step") = 1.0 / 12.0);

    m.def(
        "HybridGSRRateData_New",
        [](const std::string& name, const std::string& factor, const std::shared_ptr<GSRCurveData_>& curve,
           const std::shared_ptr<GSRVolData_>& vol) -> std::shared_ptr<HybridComponentData_> {
            REQUIRE(curve && vol, "InvalidHybridComponent: GSR curve and volatility are required");
            return std::const_pointer_cast<HybridComponentData_>(
                NewHybridGSRRateData(String_(name), String_(factor), Handle_<GSRCurveData_>(std::shared_ptr<const GSRCurveData_>(curve)),
                                     Handle_<GSRVolData_>(std::shared_ptr<const GSRVolData_>(vol))));
        },
        py::arg("name"), py::arg("factor"), py::arg("curve"), py::arg("vol"));

    m.def(
        "BSLocalVolModelData_New",
        [](const std::string& name, const std::string& index, const std::string& currency, const std::string& factor,
           const std::shared_ptr<ModelData_>& bs, const std::shared_ptr<LocalVolSurfaceData_>& surface,
           double maxStep) -> std::shared_ptr<ModelData_> {
            const auto* base = dynamic_cast<const BSModelData_*>(bs.get());
            REQUIRE(base && surface, "InvalidLocalVolModel: BS model and local-vol surface are required");
            return std::const_pointer_cast<ModelData_>(
                NewBSLocalVolModelData(String_(name), String_(index), String_(currency), String_(factor), *base,
                                       Handle_<LocalVolSurfaceData_>(std::shared_ptr<const LocalVolSurfaceData_>(surface)), maxStep));
        },
        py::arg("name"), py::arg("index"), py::arg("currency"), py::arg("factor"), py::arg("bs"), py::arg("surface"),
        py::arg("max_step") = 1.0 / 12.0);

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
}
