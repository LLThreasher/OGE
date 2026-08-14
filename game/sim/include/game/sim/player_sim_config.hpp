#pragma once

#include <cstdint>

#include "oge/runtime/type_name.hpp"

namespace game::sim
{
// =========================================================================
// Shared simulation tick space (D1): one tick = one fixed frame at 20 Hz,
// executed as kSubStepsPerTick sub-steps of kSubStepDt.  The server's fixed
// pipeline and the client's authoritative-world parity sim run the same
// ordered (player -> creature -> physics) sub-step chain with the same dt —
// determinism by construction.
// =========================================================================

constexpr float kSubStepDt = 1.f / 60.f;
constexpr int kSubStepsPerTick = 3;
constexpr float kFixedFrameDuration = kSubStepDt * kSubStepsPerTick;  // 1/20

// World ctx: the tick the fixed stages are simulating, and which sub-step
// of that tick is currently executing (0 = decisions, 1..2 = re-application).
// Scenes write currentTick before Update; the scene-driven sub-step loop
// writes subStepIdx.
struct SimTickContext
{
    uint32_t currentTick = 0;
    uint8_t subStepIdx = 0;
};

}  // namespace game::sim

DECL_TYPE_NAME(game::sim::SimTickContext, "core::SimTickContext")
