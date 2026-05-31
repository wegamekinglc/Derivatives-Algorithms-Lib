"""Tests for the String_ class."""

import dal


def test_string_from_str():
    """String_ can be constructed from a Python string."""
    s = dal.String_("hello")
    assert str(s) == "hello"


def test_string_from_empty():
    """String_ from empty string works."""
    s = dal.String_("")
    assert str(s) == ""


def test_string_repr():
    """String_ repr returns the string content."""
    s = dal.String_("world")
    r = repr(s)
    assert "world" in r


def test_string_with_special_chars():
    """String_ handles special characters."""
    s = dal.String_("hello world 123 !@#")
    assert "hello world 123" in str(s)


def test_string_finance_context():
    """String_ works with typical finance-domain strings."""
    names = ["BSModelData_", "ScriptProductData_", "STRIKE", "call"]
    for name in names:
        s = dal.String_(name)
        assert name in str(s)


def test_strvector():
    """StrVector (std::vector<std::string>) works as a list-like container."""
    sv = dal.StrVector()
    assert len(sv) == 0

    sv.append("first")
    sv.append("second")
    sv.append("third")
    assert len(sv) == 3
    assert sv[0] == "first"
    assert sv[1] == "second"
    assert sv[2] == "third"


def test_strvector_iteration():
    """StrVector supports iteration."""
    sv = dal.StrVector()
    items = ["alpha", "beta", "gamma"]
    for item in items:
        sv.append(item)

    collected = [sv[i] for i in range(len(sv))]
    assert collected == items
