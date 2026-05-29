# Phase 0: Code Generation Pipeline

## Tool: Machinist

- **Location**: `externals/machinist/`
- **Binary**: `externals/machinist/bin/Machinist`
- **Template dir**: `externals/machinist/template/` (set via `MACHINIST_TEMPLATE_DIR` env var)
- **Built by**: `externals/machinist/build_linux.sh` (called from parent `build_linux.sh`)

## Build Commands

From `build_linux.sh` (lines 40-41):

```bash
export MACHINIST_TEMPLATE_DIR=$PWD/externals/machinist/template/
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./public
```

## Inputs

| File             | Purpose                                          |
|------------------|--------------------------------------------------|
| `config/dal.ifc`  | Interface definition file (enum/object/settings) |
| `config/dal.mgl`  | Machinist glossary/template config               |

## Outputs

### dal/auto/ (generated from `-d ./dal`)

Contains ~86 files organized into three categories:

**Enum types** (`.hpp` + `.inc` pairs):
```
MG_BizDayConvention_enum.hpp    MG_BizDayConvention_enum.inc
MG_Ccy_enum.hpp                 MG_Ccy_enum.inc
MG_Clearer_enum.hpp             MG_Clearer_enum.inc
MG_CollateralType_enum.hpp      MG_CollateralType_enum.inc
MG_CurveKnotPolicy_enum.hpp     MG_CurveKnotPolicy_enum.inc
MG_CurveParameterization_enum.hpp MG_CurveParameterization_enum.inc
MG_CurveSolveMode_enum.hpp      MG_CurveSolveMode_enum.inc
MG_DateGeneration_enum.hpp      MG_DateGeneration_enum.inc
MG_DateStepSize_enum.hpp        MG_DateStepSize_enum.inc
MG_DayBasis_enum.hpp            MG_DayBasis_enum.inc
MG_DomainCondProp_enum.hpp      MG_DomainCondProp_enum.inc
MG_NodeType_enum.hpp            MG_NodeType_enum.inc
MG_OptionType_enum.hpp          MG_OptionType_enum.inc
MG_PeriodLength_enum.hpp        MG_PeriodLength_enum.inc
MG_RNGType_enum.hpp             MG_RNGType_enum.inc
MG_RepositoryErase_enum.hpp     MG_RepositoryErase_enum.inc
MG_SpecialDay_enum.hpp          MG_SpecialDay_enum.inc
MG_TradedRate_enum.hpp          MG_TradedRate_enum.inc
```

**Object serialization** (`_object.hpp` + `_v1_Read.inc` + `_v1_Write.inc`):
```
MG_BSModelData_object.*
MG_Bag_object.*
MG_Box_object.*
MG_Cubic1_object.*
MG_DiscountPWLF_object.*
MG_DupireModelData_object.*
MG_Fixings_object.*
MG_Interp1Linear_object.*
MG_Interp2Linear_object.*
MG_LogLinear1_object.*
MG_PiecewiseConstant_object.*
MG_PseudoRSG_object.*
MG_Report_object.*
MG_ScriptProductData_object.*
MG_SobolRSG_object.*
MG_UnderdeterminedControls_object.*
```

**Java** (`java/` directory with `.java` files)

### public/auto/ (generated from `-d ./public`)

Contains Excel public function stubs (`MG_*_public.inc`). These are included directly by `public/excel/` source files at specific line numbers:
- `MG_EvaluationDate_Set_public.inc` / `_Get_public.inc` (included in `__global.cpp`)
- `MG_Interp1_New_Linear_public.inc` etc. (included in `__interp.cpp`)
- `MG_BSModelData_New_public.inc` etc. (included in `__models.cpp`)
- `MG_PseudoRSG_New_public.inc` etc. (included in `__random.cpp`)
- `MG_Repository_Erase_public.inc` etc. (included in `__repository.cpp`)
- `MG_Product_New_public.inc` etc. (included in `__script.cpp`)
- `MG_MonteCarlo_Value_public.inc` (included in `__value.cpp`)
- `MG_Format_public.inc`, `MG_PasteWithArgs_public.inc` (included in `_excel.cpp`)

## Source-of-Truth Problem

**ONE input file** (`config/dal.ifc`) generates outputs in **TWO directories** (`dal/auto/` and `public/auto/`). This means:

1. We cannot simply move `config/` into `dal-cpp/` -- dal-public would lose access to its auto-generated stubs.
2. First-round strategy (per the refactor plan): keep `config/` at the top level as the migration-period source of truth.
3. Second-round: split into `dal-cpp/config/dal.ifc` (core enums + objects) and `dal-public/config/dal_public.ifc` (public Excel stubs).
4. The Machinist invocation commands would need to be updated to route output to dal-cpp/ and dal-public/ respectively.
