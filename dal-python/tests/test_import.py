"""Test basic DAL Python module import and type availability."""


def test_import_dal():
    """Verify the dal module can be imported."""
    import dal
    assert dal is not None


def test_module_has_expected_attributes():
    """Verify dal module exposes expected API surfaces."""
    import dal
    attrs = dir(dal)
    assert len(attrs) > 0, "dal module should have attributes"


def test_core_types_available():
    """Verify all core SWIG-wrapped types are importable."""
    import dal
    for type_name in [
        "Date_", "String_", "Cell_",
        "DoubleVector", "StrVector", "CellVector", "DateVector",
        "DoubleMatrix_", "Dictionary",
    ]:
        assert hasattr(dal, type_name), f"Missing type: {type_name}"


def test_factory_functions_available():
    """Verify all factory functions are importable."""
    import dal
    for fn_name in [
        "BSModelData_New",
        "DupireModelData_New",
        "Product_New",
        "Product_Debug",
        "PseudoRSG_New",
        "SobolRSG_New",
        "PseudoRSG_Get_Uniform",
        "PseudoRSG_Get_Normal",
        "SobolRSG_Get_Uniform",
        "SobolRSG_Get_Normal",
        "MonteCarlo_Value",
        "EvaluationDate_Get",
        "EvaluationDate_Set",
    ]:
        assert hasattr(dal, fn_name), f"Missing function: {fn_name}"
        assert callable(getattr(dal, fn_name)), f"Not callable: {fn_name}"


def test_date_functions_available():
    """Verify Date accessor functions are available at module level."""
    import dal
    for fn_name in ["Year", "Month", "Day"]:
        assert hasattr(dal, fn_name), f"Missing function: {fn_name}"
        assert callable(getattr(dal, fn_name)), f"Not callable: {fn_name}"


def test_dal_submodule_importable():
    """The dal.dal SWIG-generated submodule is directly importable."""
    import dal.dal
    assert dal.dal is not None


def test_api_submodule_importable():
    """The dal.api submodule is directly importable."""
    import dal.api
    assert dal.api is not None
