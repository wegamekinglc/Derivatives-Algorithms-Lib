import copy

import pytest

import dal


def test_checkpoint_settings_roundtrip():
    settings = dal.MonteCarloSettings_(
        compiled=True,
        lsmc_checkpoint_block_events=8,
        lsmc_training_memory_mb=64,
    )
    for clone in (settings, copy.copy(settings), copy.deepcopy(settings)):
        assert clone.lsmc_checkpoint_block_events == 8
        assert clone.lsmc_training_memory_mb == 64
    settings.lsmc_checkpoint_block_events = None
    assert settings.lsmc_checkpoint_block_events is None
    assert dal.MonteCarloSettings_().lsmc_training_memory_mb is None


@pytest.mark.parametrize("field", ["lsmc_checkpoint_block_events", "lsmc_training_memory_mb"])
@pytest.mark.parametrize("value", [True, 0, -1, 1.5, "8", 2**40])
def test_checkpoint_settings_reject_invalid_counts(field, value):
    with pytest.raises((TypeError, RuntimeError), match="InvalidLsmcCheckpointSetting"):
        dal.MonteCarloSettings_(**{field: value})
