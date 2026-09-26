#include "features/camera/ZoomState.h"
#include "settings/Settings.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}
void settingsStoreTests();
int runPreviewLayoutTests();
int runDurabilityBarTests();
int runBundlePreviewTests();
int main() try {
    extern void automationInputTests();
    automationInputTests();
    extern void autoClickTests();
    autoClickTests();
    extern void detachedLookTests();
    detachedLookTests();
    extern void detachedCameraMotionTests();
    detachedCameraMotionTests();
    extern void restrictionRegionTests();
    restrictionRegionTests();
    extern void frameRateTests();
    frameRateTests();
    extern void toolChoiceTests();
    toolChoiceTests();
    extern void restockPlanTests();
    restockPlanTests();
    extern void textFitTests();
    textFitTests();
    extern void numberInputTests();
    numberInputTests();
    extern void overlayGeometryTests();
    overlayGeometryTests();
    extern void shapeCollectionTests();
    shapeCollectionTests();
    extern void shapeDocumentTests();
    shapeDocumentTests();
    extern void shapeStoreTests();
    shapeStoreTests();
    extern void settingsRowsTests();
    settingsRowsTests();
    extern void bindingTests();
    bindingTests();
    extern void searchQueryTests();
    searchQueryTests();
    extern void sortPlannerPropertyTests();
    sortPlannerPropertyTests();
    extern void translationTests();
    translationTests();
    extern void lightOverlayTests();
    lightOverlayTests();
    extern void shapeEditorTests();
    shapeEditorTests();
    extern void settingsTableTests();
    settingsTableTests();
    extern void responseBarrierTests();
    responseBarrierTests();
    extern void toastTests();
    toastTests();
    extern void hudElementTests();
    hudElementTests();
    extern void hudEditorLayoutTests();
    hudEditorLayoutTests();
    extern void infoLinesTests();
    infoLinesTests();
    extern void targetDetailTests();
    targetDetailTests();
    extern void targetCardTests();
    targetCardTests();
    extern void shapeProfileTests();
    shapeProfileTests();
    extern int runSortPlannerTests();
    check(runSortPlannerTests() == 0, "inventory planning and ordering suites");
    settingsStoreTests();
    check(runPreviewLayoutTests() + runDurabilityBarTests() + runBundlePreviewTests() == 0,
          "item inspection suites");
    lamium::ZoomState zoom;
    zoom.configure(3);
    check(zoom.fov(90) == 90, "inactive camera must pass through");
    zoom.press();
    check(zoom.fov(90) == 30, "held zoom scales projection");
    check(zoom.sensitivity() == 1.0f/3.0f, "sensitivity follows magnification");
    check(zoom.fov(.25f) == .25f, "tiny vanilla FOV must not use reversed clamp bounds");
    zoom.wheel(1);
    check(std::abs(zoom.targetLevel() - 3 * lamium::ZoomState::notch) < 1e-4f && zoom.level() == 3,
        "a notch moves the target, not the shown level");
    zoom.advance(0);
    zoom.advance(.02);
    check(zoom.level() > 3 && zoom.level() < zoom.targetLevel(), "the shown level eases toward the target");
    float previous = zoom.level();
    for (int i=1; i<=40; ++i) {
        zoom.advance(.02 + i * .01);
        check(zoom.level() >= previous && zoom.level() <= zoom.targetLevel(), "easing never overshoots");
        previous = zoom.level();
    }
    check(zoom.level() == zoom.targetLevel(), "easing settles on the target");
    zoom.wheel(-1);
    check(std::abs(zoom.targetLevel() - 3) < 1e-4f, "notches are symmetric");
    int notches = 0;
    while (zoom.targetLevel() < lamium::ZoomState::maxLevel && notches < 100) { zoom.wheel(1); ++notches; }
    check(zoom.targetLevel() == 50 && notches <= 25, "50x is a reasonable number of notches from 3x");
    for (int i=0; i<100; ++i) zoom.wheel(-1);
    check(zoom.targetLevel() == 1, "lower wheel bound");
    zoom.release();
    check(zoom.level() == 1 && zoom.fov(90) == 90, "release settles on the target and restores the projection");
    zoom.reset();
    check(!zoom.held() && zoom.level() == 3 && zoom.sensitivity() == 1, "reset restores vanilla");
    zoom.wheel(1);
    check(zoom.targetLevel() == 3, "inactive wheel ignored");
    zoom.configure(std::numeric_limits<float>::infinity());
    zoom.press();
    check(zoom.fov(90) == 30, "invalid configuration cannot poison projection");
    zoom.configure(80);
    zoom.press();
    check(zoom.level() == 50, "configured magnification is clamped to 50x");
    lamium::Settings settings;
    settings.camera.magnification = -9;
    settings.normalize();
    check(settings.camera.magnification == 1, "normalize settings");
    settings.camera.magnification = 40;
    settings.normalize();
    check(settings.camera.magnification == 40, "magnification up to 50x is kept");
    std::cout << "Lamium: camera, settings storage, and item inspection checks passed\n";
} catch (std::exception const& error) {
    std::cerr << "Lamium test failure: " << error.what() << '\n';
    return 1;
}
