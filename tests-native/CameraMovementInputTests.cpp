#include "features/camera/CameraMovementInput.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include <iostream>
#include <stdexcept>

void check(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}
int main() try {
    using Flag = MoveInputState::Flag;
    RawMoveInputComponent raw{};
    auto& flags = *raw.mRawInput->mFlagValues;
    for (size_t i = 0; i < static_cast<size_t>(Flag::Count); ++i) flags.set(i);
    *raw.mRawMove = Vec2{.25f, -.75f};
    *raw.mRawInput->mAnalogMoveVector = Vec2{.5f, -.5f};
    raw.mRawInput->mLookSlightDirField = 1;
    raw.mRawInput->mLookNormalDirField = 2;
    raw.mRawInput->mLookSmoothDirField = 3;
    auto original = raw;
    auto axes = lamium::camera::consumeMovement(raw);
    check(axes == lamium::DetachedCameraMotion::Vector{.25,0,-.75}, "native horizontal axes / opposing vertical inputs");
    check(raw.mRawMove->x == 0 && raw.mRawMove->z == 0
        && raw.mRawInput->mAnalogMoveVector->x == 0 && raw.mRawInput->mAnalogMoveVector->z == 0,
        "both extracted movement vectors must be suppressed");
    for (size_t i = 0; i < static_cast<size_t>(Flag::Count); ++i) {
        bool preserved = i == static_cast<size_t>(Flag::BlockSelectDown) || i == static_cast<size_t>(Flag::LookCenter);
        check(flags.test(i) == preserved, "movement flags cleared while unrelated flags remain");
        check(original.mRawInput->mFlagValues->test(i), "copy must not share physical input flag storage");
    }
    check(raw.mRawInput->mLookSlightDirField == 1 && raw.mRawInput->mLookNormalDirField == 2
        && raw.mRawInput->mLookSmoothDirField == 3, "keyboard look fields preserved");
    check(original.mRawMove->x == .25f && original.mRawInput->mAnalogMoveVector->x == .5f,
          "consumption must not mutate other input snapshots");
    flags.set(static_cast<size_t>(Flag::JumpDown));
    check(lamium::camera::consumeMovement(raw)[1] == 1, "jump ascends the camera");
    flags.set(static_cast<size_t>(Flag::SneakToggleDown));
    check(lamium::camera::consumeMovement(raw)[1] == 0, "persistent sneak must not move the camera");
    flags.set(static_cast<size_t>(Flag::SneakDown));
    check(lamium::camera::consumeMovement(raw)[1] == -1, "held sneak descends the camera");
    RawMoveInputComponent keys{};
    auto& keyFlags = *keys.mRawInput->mFlagValues;
    keyFlags.set(static_cast<size_t>(Flag::Up));
    keyFlags.set(static_cast<size_t>(Flag::Right));
    check(lamium::camera::freecameraInputAxes(keys) == lamium::DetachedCameraMotion::Vector{1, 0, 1},
          "keyboard direction flags drive WASD axes");
    keyFlags.set(static_cast<size_t>(Flag::Down));
    check(lamium::camera::freecameraInputAxes(keys) == lamium::DetachedCameraMotion::Vector{1, 0, 0},
          "opposing direction flags cancel");
    keyFlags.set(static_cast<size_t>(Flag::JumpDown));
    check(lamium::camera::freecameraInputAxes(keys)[1] == 1, "jump flag ascends the stash");
    RawMoveInputComponent pad{};
    *pad.mRawMove = Vec2{.5f, -.25f};
    check(lamium::camera::freecameraInputAxes(pad) == lamium::DetachedCameraMotion::Vector{.5, 0, -.25},
          "analog vector is the fallback without flags");
    std::cout << "Native camera movement extraction checks passed\n";
    return 0;
} catch (std::exception const& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
