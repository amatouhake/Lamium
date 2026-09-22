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
    extern void settingsLayoutTests();
    settingsLayoutTests();
    extern void responseBarrierTests();
    responseBarrierTests();
    extern int runSortPlannerTests();
    check(runSortPlannerTests() == 0, "inventory planning and ordering suites");
    settingsStoreTests();
    check(runPreviewLayoutTests() + runDurabilityBarTests() + runBundlePreviewTests() == 0,
          "item inspection suites");
    lamium::ZoomState zoom;
    zoom.configure(3, .5f);
    check(zoom.fov(90) == 90, "inactive camera must pass through");
    zoom.press();
    check(zoom.fov(90) == 30, "held zoom scales projection");
    check(zoom.sensitivity() == 1.0f/3.0f, "sensitivity follows magnification");
    check(zoom.fov(.25f) == .25f, "tiny vanilla FOV must not use reversed clamp bounds");
    for (int i=0; i<100; ++i) zoom.wheel(1);
    check(zoom.level() == 10, "upper wheel bound");
    for (int i=0; i<100; ++i) zoom.wheel(-1);
    check(zoom.level() == 1, "lower wheel bound");
    zoom.reset();
    check(!zoom.held() && zoom.level() == 3 && zoom.sensitivity() == 1, "reset restores vanilla");
    zoom.wheel(1);
    check(zoom.level() == 3, "inactive wheel ignored");
    zoom.configure(std::numeric_limits<float>::infinity(), std::nanf(""));
    zoom.press();
    check(zoom.fov(90) == 30, "invalid configuration cannot poison projection");
    lamium::Settings settings;
    settings.camera.magnification = -9;
    settings.camera.wheelStep = std::numeric_limits<float>::infinity();
    settings.normalize();
    check(settings.camera.magnification == 1 && settings.camera.wheelStep == .5f, "normalize settings");
    std::cout << "Lamium: camera, settings storage, and item inspection checks passed\n";
} catch (std::exception const& error) {
    std::cerr << "Lamium test failure: " << error.what() << '\n';
    return 1;
}
