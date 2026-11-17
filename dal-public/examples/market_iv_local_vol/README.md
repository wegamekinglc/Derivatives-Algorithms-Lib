# Public market F/D workflow

```bash
cmake --preset Release-linux -S . -B build/market-public -DDAL_BUILD_PUBLIC=ON -DDAL_PUBLIC_BUILD_EXAMPLES=ON
cmake --build build/market-public --target market_iv_local_vol_public -j 4
ctest --test-dir build/market-public -R '^market_iv_local_vol_public$' --output-on-failure
```

The program uses `NewMarketIvLocalVolModelData`, `ValueByMonteCarlo` and public
inspection. Its fixture, checks and output match the independent
[core program](../../../dal-cpp/examples/models/market_iv_local_vol/README.md).
It installs as `bin/market_iv_local_vol_public`.
