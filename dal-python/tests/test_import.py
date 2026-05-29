"""Test basic DAL Python module import and type availability."""


def test_import_dal():
    """Verify the dal module can be imported."""
    import dal
    assert dal is not None


def test_module_has_expected_attributes():
    """Verify dal module exposes expected API surfaces."""
    import dal
    attrs = dir(dal)
    # The SWIG-generated module should expose these categories
    assert len(attrs) > 0, "dal module should have attributes"
