#include "features/camera/CameraMovementInput.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/input/MoveInputState.h"
#include <algorithm>
#include <array>

namespace lamium::camera {
DetachedCameraMotion::Vector freecameraInputAxes(RawMoveInputComponent const& raw) {
    using Flag = MoveInputState::Flag;
    auto& flags = *raw.mRawInput->mFlagValues;
    auto held = [&](Flag flag) { return flags.test(static_cast<size_t>(flag)); };
    // Flags first: summing both sources could cancel when their signs differ.
    double x = 0, z = 0;
    if (held(Flag::Right)) x += 1;
    if (held(Flag::Left)) x -= 1;
    if (held(Flag::Up)) z += 1;
    if (held(Flag::Down)) z -= 1;
    if (x == 0 && z == 0) {
        x = raw.mRawMove->x;
        z = raw.mRawMove->z;
    }
    double y = static_cast<double>(held(Flag::JumpDown) || held(Flag::Ascend))
        - static_cast<double>(held(Flag::SneakDown) || held(Flag::Descend));
    return {std::clamp(x, -1.0, 1.0), y, std::clamp(z, -1.0, 1.0)};
}
DetachedCameraMotion::Vector consumeMovement(RawMoveInputComponent& raw) {
    using Flag = MoveInputState::Flag;
    auto& flags = *raw.mRawInput->mFlagValues;
    auto down = [&](Flag flag) { return flags.test(static_cast<size_t>(flag)); };
    // Horizontal axes have already passed through vanilla's device handling.
    // Vertical flight uses momentary jump/sneak, not the persisted sneak toggle.
    DetachedCameraMotion::Vector axes{
        raw.mRawMove->x,
        static_cast<double>(down(Flag::JumpDown) || down(Flag::Ascend))
            - static_cast<double>(down(Flag::SneakDown) || down(Flag::Descend)),
        raw.mRawMove->z,
    };
    *raw.mRawMove = Vec2{};
    *raw.mRawInput->mAnalogMoveVector = Vec2{};
    constexpr auto movementFlags = std::to_array<Flag>({
        Flag::SneakDown, Flag::SneakToggleDown, Flag::WantDownSlow, Flag::WantUpSlow,
        Flag::AscendBlock, Flag::DescendBlock, Flag::JumpDown, Flag::SprintDown,
        Flag::UpLeft, Flag::UpRight, Flag::DownLeft, Flag::DownRight,
        Flag::Up, Flag::Down, Flag::Left, Flag::Right,
        Flag::Ascend, Flag::Descend, Flag::ChangeHeight,
        Flag::SneakInputCurrentlyDown, Flag::SneakInputWasReleased, Flag::SneakInputWasPressed,
        Flag::JumpInputWasReleased, Flag::JumpInputWasPressed, Flag::JumpInputCurrentlyDown,
    });
    for (auto flag : movementFlags) flags.set(static_cast<size_t>(flag), false);
    return axes;
}
}
