#include "features/camera/ViewFeet.h"
#include "ui/ShapeEditor.h"
#include <cmath>
#include <limits>
#include <variant>
void check(bool, char const*);

void viewFeetTests() {
    using lamium::camera::Position;
    using lamium::camera::viewFeet;
    using lamium::overlay::Snap;
    using lamium::ui::shape::Reference;
    auto near = [](Position const& a, Position const& b) {
        return std::abs(a.x - b.x) < 1e-9 && std::abs(a.y - b.y) < 1e-9 && std::abs(a.z - b.z) < 1e-9;
    };

    Position body{10.5, 64.0, -3.5}, eye{10.5, 65.62, -3.5}, far{100.25, 80.0, 200.75};
    // An ordinary view references the body's feet, exactly as before L-248.
    check(viewFeet(std::nullopt, body, eye) == body, "without a detached view the body's feet are the reference");
    // Freelook stands the camera at the body's eye, so its virtual feet are the body's.
    check(near(*viewFeet(eye, body, eye), body), "Freelook's virtual feet coincide with the body's feet");
    // FreeCamera references its own position minus the player's eye height.
    auto feet = viewFeet(far, body, eye);
    check(feet && feet->x == far.x && feet->z == far.z && std::abs(feet->y - (far.y - 1.62)) < 1e-9,
        "a detached camera references its own position minus the eye height");
    // The offset follows the real pose, so sneaking does not move the reference down.
    auto sneaking = Position{10.5, 64.0 + 1.27, -3.5};
    feet = viewFeet(far, body, sneaking);
    check(feet && std::abs(feet->y - (far.y - 1.27)) < 1e-9, "a lowered eye height follows the pose");
    // A pose read mid-teleport reports the eye at the feet: use the standing height.
    feet = viewFeet(far, body, body);
    check(feet && std::abs(feet->y - (far.y - lamium::camera::defaultEyeHeight)) < 1e-9,
        "an unusable eye height falls back to the standing height");
    // Unusable positions refuse instead of quietly using the body.
    double nan = std::numeric_limits<double>::quiet_NaN();
    check(!viewFeet(Position{nan, 80, 0}, body, eye), "a non-finite camera references nothing");
    check(!viewFeet(std::nullopt, Position{nan, 64, 0}, eye), "a non-finite body references nothing");
    check(!viewFeet(far, body, Position{nan, 65.62, -3.5}), "a non-finite eye references nothing");

    // A shape created from the view keeps the camera's precise point for the
    // exact reference, and snaps the standing block one to a block center.
    auto at = *viewFeet(far, body, eye);
    auto exact = lamium::ui::shape::withType({"E"}, 2, {at.x, at.y, at.z}, Reference::ExactPosition);
    auto spec = std::get<lamium::overlay::ShapeSpec>(exact.geometry);
    check(spec.snap == Snap::Off && spec.center == lamium::overlay::Point{at.x, at.y, at.z},
        "an exact-reference shape starts at the view's precise point");
    lamium::ui::shape::place(exact, {at.x, at.y, at.z}, Reference::StandingBlock);
    auto placed = std::get<lamium::overlay::ShapeSpec>(exact.geometry);
    check(placed.snap == Snap::BlockCenter, "a standing-block reference selects block-center snapping");
    // The view stands at y 78.38, x 100.25, z 200.75: block (100, 78, 200) centered.
    check(lamium::overlay::snapped(placed.center, placed.snap) == lamium::overlay::Point{100.5, 78.5, 200.5},
        "a standing-block shape centers on the block holding the view");
}
