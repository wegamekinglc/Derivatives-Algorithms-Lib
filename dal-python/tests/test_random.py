"""Tests for PseudoRSG_ and SobolRSG_ random sequence generators."""

import dal


# ---- PseudoRSG ---------------------------------------------------------------


def test_pseudo_rsg_new():
    """PseudoRSG_New creates a valid generator."""
    rsg = dal.PseudoRSG_New(seed=42, ndim=3)
    assert rsg is not None  # nosec B101 - pytest assertions are intentional


def test_pseudo_rsg_default_ndim():
    """PseudoRSG_New defaults to ndim=1."""
    rsg = dal.PseudoRSG_New(seed=123)
    assert rsg is not None  # nosec B101 - pytest assertions are intentional


def test_pseudo_uniform_shape():
    """PseudoRSG_Get_Uniform returns a matrix of (num_path, ndim)."""
    ndim = 5
    num_path = 100
    rsg = dal.PseudoRSG_New(seed=42, ndim=ndim)
    m = dal.PseudoRSG_Get_Uniform(rsg, num_path)
    assert m is not None  # nosec B101 - pytest assertions are intentional
    # Check all elements are in [0, 1]
    for i in range(num_path):
        for j in range(ndim):
            val = m(i, j)
            assert 0.0 <= val <= 1.0, f"m({i},{j})={val} out of [0,1]"  # nosec B101 - pytest assertions are intentional


def test_pseudo_uniform_not_constant():
    """Pseudo uniform samples are not all the same value."""
    rsg = dal.PseudoRSG_New(seed=42, ndim=2)
    m = dal.PseudoRSG_Get_Uniform(rsg, 50)
    vals = {m(i, 0) for i in range(50)}
    assert len(vals) > 1, "All uniform samples are identical"  # nosec B101 - pytest assertions are intentional


def test_pseudo_normal_shape():
    """PseudoRSG_Get_Normal returns a matrix of (num_path, ndim)."""
    ndim = 3
    num_path = 200
    rsg = dal.PseudoRSG_New(seed=42, ndim=ndim)
    m = dal.PseudoRSG_Get_Normal(rsg, num_path)
    assert m is not None  # nosec B101 - pytest assertions are intentional
    # Normal samples should mostly be in [-4, 4]
    for i in range(num_path):
        for j in range(ndim):
            val = m(i, j)
            assert -10.0 < val < 10.0, f"m({i},{j})={val} extreme outlier"  # nosec B101 - pytest assertions are intentional


def test_pseudo_normal_mean_near_zero():
    """Pseudo normal samples have mean approximately zero."""
    ndim = 1
    num_path = 10000
    rsg = dal.PseudoRSG_New(seed=42, ndim=ndim)
    m = dal.PseudoRSG_Get_Normal(rsg, num_path)
    total = sum(m(i, 0) for i in range(num_path))
    mean = total / num_path
    assert abs(mean) < 0.1, f"Mean of normal samples: {mean}, expected ~0"  # nosec B101 - pytest assertions are intentional


def test_pseudo_reproducibility():
    """Same seed produces identical sequences."""
    rsg1 = dal.PseudoRSG_New(seed=42, ndim=2)
    m1 = dal.PseudoRSG_Get_Uniform(rsg1, 10)

    rsg2 = dal.PseudoRSG_New(seed=42, ndim=2)
    m2 = dal.PseudoRSG_Get_Uniform(rsg2, 10)

    for i in range(10):
        for j in range(2):
            assert m1(i, j) == m2(i, j)  # nosec B101 - pytest assertions are intentional


def test_pseudo_different_seeds_differ():
    """Different seeds produce different sequences."""
    rsg1 = dal.PseudoRSG_New(seed=42, ndim=1)
    m1 = dal.PseudoRSG_Get_Uniform(rsg1, 10)

    rsg2 = dal.PseudoRSG_New(seed=99, ndim=1)
    m2 = dal.PseudoRSG_Get_Uniform(rsg2, 10)

    any_different = any(m1(i, 0) != m2(i, 0) for i in range(10))
    assert any_different, "Different seeds produced identical sequences"  # nosec B101 - pytest assertions are intentional


# ---- SobolRSG ---------------------------------------------------------------


def test_sobol_rsg_new():
    """SobolRSG_New creates a valid generator."""
    rsg = dal.SobolRSG_New(i_path=0, ndim=3)
    assert rsg is not None  # nosec B101 - pytest assertions are intentional


def test_sobol_rsg_default_ndim():
    """SobolRSG_New defaults to ndim=1."""
    rsg = dal.SobolRSG_New(i_path=0)
    assert rsg is not None  # nosec B101 - pytest assertions are intentional


def test_sobol_normal_precision_policy():
    """Sobol normals default to fast mode and allow full precision explicitly."""
    i_path = (1 << 20) - 2
    default_rsg = dal.SobolRSG_New(i_path=i_path)
    fast_rsg = dal.SobolRSG_New(i_path=i_path, precise=False, polish=False)
    precise_rsg = dal.SobolRSG_New(i_path=i_path, precise=True, polish=True)

    default_value = dal.SobolRSG_Get_Normal(default_rsg, 1)(0, 0)
    fast_value = dal.SobolRSG_Get_Normal(fast_rsg, 1)(0, 0)
    precise_value = dal.SobolRSG_Get_Normal(precise_rsg, 1)(0, 0)

    assert default_value == fast_value  # nosec B101 - pytest assertions are intentional
    assert precise_value != fast_value  # nosec B101 - pytest assertions are intentional


def test_sobol_uniform_shape():
    """SobolRSG_Get_Uniform returns values in [0, 1]."""
    ndim = 4
    num_path = 100
    rsg = dal.SobolRSG_New(i_path=0, ndim=ndim)
    m = dal.SobolRSG_Get_Uniform(rsg, num_path)
    assert m is not None  # nosec B101 - pytest assertions are intentional
    for i in range(num_path):
        for j in range(ndim):
            val = m(i, j)
            assert 0.0 <= val <= 1.0, f"m({i},{j})={val} out of [0,1]"  # nosec B101 - pytest assertions are intentional


def test_sobol_uniform_not_constant():
    """Sobol uniform samples vary across paths."""
    rsg = dal.SobolRSG_New(i_path=0, ndim=2)
    m = dal.SobolRSG_Get_Uniform(rsg, 50)
    vals = {m(i, 0) for i in range(50)}
    assert len(vals) > 1, "All Sobol uniform samples are identical"  # nosec B101 - pytest assertions are intentional


def test_sobol_normal_shape():
    """SobolRSG_Get_Normal returns reasonable normal samples."""
    ndim = 3
    num_path = 200
    rsg = dal.SobolRSG_New(i_path=0, ndim=ndim)
    m = dal.SobolRSG_Get_Normal(rsg, num_path)
    assert m is not None  # nosec B101 - pytest assertions are intentional
    for i in range(num_path):
        for j in range(ndim):
            val = m(i, j)
            assert -10.0 < val < 10.0, f"m({i},{j})={val} extreme outlier"  # nosec B101 - pytest assertions are intentional


def test_sobol_normal_mean_near_zero():
    """Sobol normal samples have mean approximately zero."""
    ndim = 1
    num_path = 10000
    rsg = dal.SobolRSG_New(i_path=0, ndim=ndim)
    m = dal.SobolRSG_Get_Normal(rsg, num_path)
    total = sum(m(i, 0) for i in range(num_path))
    mean = total / num_path
    assert abs(mean) < 0.1, f"Mean of Sobol normal samples: {mean}, expected ~0"  # nosec B101 - pytest assertions are intentional


def test_sobol_reproducibility():
    """Same i_path produces identical Sobol sequences."""
    rsg1 = dal.SobolRSG_New(i_path=0, ndim=2)
    m1 = dal.SobolRSG_Get_Uniform(rsg1, 10)

    rsg2 = dal.SobolRSG_New(i_path=0, ndim=2)
    m2 = dal.SobolRSG_Get_Uniform(rsg2, 10)

    for i in range(10):
        for j in range(2):
            assert m1(i, j) == m2(i, j)  # nosec B101 - pytest assertions are intentional


def test_sobol_different_starting_points_differ():
    """Different i_path values produce different Sobol sequences."""
    rsg1 = dal.SobolRSG_New(i_path=0, ndim=1)
    m1 = dal.SobolRSG_Get_Uniform(rsg1, 10)

    rsg2 = dal.SobolRSG_New(i_path=100, ndim=1)
    m2 = dal.SobolRSG_Get_Uniform(rsg2, 10)

    any_different = any(m1(i, 0) != m2(i, 0) for i in range(10))
    assert any_different, "Different i_path produced identical Sobol sequences"  # nosec B101 - pytest assertions are intentional
