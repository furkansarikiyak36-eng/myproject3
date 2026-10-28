# Phase 13 Acceptance Checklist

## Source foundation
- [x] Environment and survival data contracts.
- [x] Deterministic seeded weather selection and seasonal state.
- [x] Bounded clock advancement and duplicate-operation rejection.
- [x] Weather-aware survival step with clothing and shelter modifiers.
- [x] Tunable rates and explicit integration limitations.

## Required before closing this phase
- [ ] UE 5.8 UHT and C++ build.
- [ ] Deterministic replay test: same seed, time and step sequence gives same weather sequence.
- [ ] Season boundary test and large-delta rejection test.
- [ ] Invalid and duplicate operation IDs do not mutate state.
- [ ] Shelter/clothing comparison test confirms protection reduces exposure.
- [ ] Survival state never leaves documented bounds.
- [ ] One authoritative world clock and one survival tick path selected.
- [ ] Phase 12 snapshot adapter for environment and survival state.
- [ ] Food/water inventory transactions and cooking/fuel integration.
- [ ] Visual weather/audio feedback and region/terrain integration.

## Suggested scenarios
1. Exposed character in winter blizzard with no insulation.
2. Same character with insulated clothing and a high-quality shelter.
3. Summer heatwave during high activity with low water reserve.
4. Resting under shelter versus resting in heavy rain.
5. Save/reload at a season boundary and replay the next weather period.

The code is a prototype foundation only. Do not mark M110–M118 verified from source presence.
