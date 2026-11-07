#pragma once

#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curvescript_internal.hpp>
#include <dal/indice/index/ir.hpp>

namespace Dal::CompoundedOisInternal {
    struct SourceLocation_ {
        String_ table_;
        size_t row_ = 0, sourceRow_ = 0, statement_ = 0, offset_ = 0;
    };
    struct Source_ {
        Handle_<CalibrationInstrument_> instrument_;
        Vector_<SourceLocation_> locations_;
    };
    Source_ BuildSource(const Handle_<PreparedCompoundedOis_>& tables);
    Index::Overnight_ Index(const CompoundedOisScriptSpec_& spec, size_t observation);
    bool ContainsOvernight(const Script::ScriptProduct_& product);
    bool ContainsOvernight(const CalibrationInstrument_& instrument);
    CalibrationCurveFamily_ Lower(const CalibrationInstrument_& instrument,
                                  const CalibrationPreparationContext_& context,
                                  const Vector_<const Script::NodePays_*>& payments,
                                  const Vector_<CalibrationDiagnostic_>& sources);
} // namespace Dal::CompoundedOisInternal
