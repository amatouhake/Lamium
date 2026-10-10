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
    extern void freeCameraCullingTests();
    freeCameraCullingTests();
    extern void restrictionRegionTests();
    restrictionRegionTests();
    extern void deathLayoutTests();
    deathLayoutTests();
    extern void frameRateTests();
    frameRateTests();
    extern void debugLinesTests();
    debugLinesTests();
    extern void lockedTradesTests();
    lockedTradesTests();
    extern void englishSearchTests();
    englishSearchTests();
    extern void playerListTests();
    playerListTests();
    extern void toolChoiceTests();
    toolChoiceTests();
    extern void weaponChoiceTests();
    weaponChoiceTests();
    extern void edgeGuardTests();
    edgeGuardTests();
    extern void restockPlanTests();
    restockPlanTests();
    extern void equipmentPlanTests();
    equipmentPlanTests();
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
    extern void durabilityHudTests();
    durabilityHudTests();
    extern void hudLinesTests();
    hudLinesTests();
    extern void offhandSwapTests();
    offhandSwapTests();
    extern void fakeOffhandTests();
    fakeOffhandTests();
    extern void fetchSlotTests();
    fetchSlotTests();
    extern void offhandSlotTests();
    offhandSlotTests();
    extern void saturationTests();
    saturationTests();
    extern void tooltipGlyphsTests();
    tooltipGlyphsTests();
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
    extern void schematicTests();
    schematicTests();
    extern void lightOverlayTests();
    lightOverlayTests();
    extern void shapeEditorTests();
    shapeEditorTests();
    extern void settingsTableTests();
    settingsTableTests();
    extern void responseBarrierTests();
    responseBarrierTests();
    extern void transferGestureTests();
    transferGestureTests();
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
    extern void mapTests();
    mapTests();
    extern void waypointTests();
    waypointTests();
    extern void worldMapTests();
    worldMapTests();
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
    // The look hook applies this same factor to its single turn call, so
    // Freelook/FreeCamera + Zoom gets the correction the detached branch
    // used to skip.
    check(zoom.sensitivity() * 30.0f == 10.0f, "turn input is scaled by the zoom correction on every camera path");
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
    // L-99: the wheel goes below 1x, stopping once on exactly 1x.
    zoom.fov(70);
    bool stoppedAtOne = false;
    for (int i=0; i<100; ++i) { zoom.wheel(-1); stoppedAtOne = stoppedAtOne || zoom.targetLevel() == 1.0f; }
    check(stoppedAtOne, "a notch crossing 1x stops on exactly 1x");
    check(zoom.targetLevel() == lamium::ZoomState::wheelMinLevel, "the wheel stops at 0.5x");
    auto settle = [&](double from) { for (int i=0; i<20; ++i) zoom.advance(from + i * .3); };
    settle(10);
    check(std::abs(zoom.fov(70) - 140) < .01f, "below 1x the view widens");
    check(zoom.sensitivity() == 1.0f, "below 1x turning keeps the normal speed");
    stoppedAtOne = false;
    for (int i=0; i<10 && zoom.targetLevel() < 1.5f; ++i) { zoom.wheel(1); stoppedAtOne = stoppedAtOne || zoom.targetLevel() == 1.0f; }
    check(stoppedAtOne, "going up also stops on 1x");
    zoom.fov(110);
    for (int i=0; i<100; ++i) zoom.wheel(-1);
    settle(20);
    check(std::abs(zoom.targetLevel() - 110.0f / lamium::ZoomState::maxFov) < 1e-4f
        && std::abs(zoom.fov(110) - lamium::ZoomState::maxFov) < .01f,
        "with a wide vanilla FOV the wheel stops where the view reaches 160 degrees");
    for (int i=0; i<5; ++i) zoom.wheel(1);
    float kept = zoom.targetLevel();
    zoom.configure(3);
    check(zoom.held() && zoom.targetLevel() == kept,
          "saving unrelated camera settings keeps the held zoom and its wheel level");
    zoom.release();
    check(zoom.level() == kept && zoom.fov(90) == 90, "release settles on the target and restores the projection");
    zoom.press();
    check(zoom.targetLevel() == kept, "the next press reopens the kept wheel level, below 2x too");
    zoom.release();
    zoom.press();
    while (zoom.targetLevel() != 1.0f && zoom.targetLevel() > .6f) zoom.wheel(-1);
    check(zoom.targetLevel() == 1.0f, "the wheel can rest on 1x");
    zoom.release();
    zoom.press();
    check(zoom.targetLevel() == 3 && zoom.level() == 3, "a zoom left at 1x reopens at the configured level");
    zoom.release();
    zoom.reset();
    check(!zoom.held() && zoom.level() == 3 && zoom.sensitivity() == 1
        && zoom.sensitivity() * 30.0f == 30.0f, "reset restores vanilla look sensitivity");
    zoom.wheel(1);
    check(zoom.targetLevel() == 3, "inactive wheel ignored");
    zoom.configure(std::numeric_limits<float>::infinity());
    zoom.press();
    check(zoom.fov(90) == 30, "invalid configuration cannot poison projection");
    zoom.configure(80);
    zoom.press();
    check(zoom.level() == 50, "configured magnification is clamped to 50x");
    zoom.configure(1.5f);
    zoom.press();
    check(zoom.level() == 2, "a configured level below 2x is raised to 2x");
    zoom.wheel(-1);
    check(zoom.targetLevel() < 2, "the wheel may go below the setting's 2x floor");
    {
        // L-99 follow-up: the wheel adjusts Zoom only while its key is down in toggle mode.
        lamium::ZoomKey key;
        check(key.acceptsWheel(false), "hold mode: the held key is the zoom, the wheel always adjusts");
        bool wanted = key.press(true, false);
        check(wanted && key.acceptsWheel(true), "toggle: a press switches on and the held key takes the wheel");
        wanted = key.release(true, wanted);
        check(wanted && !key.acceptsWheel(true), "toggle: the release keeps Zoom on and gives the wheel back");
        wanted = key.press(true, wanted);
        check(wanted, "toggle: pressing while on does not switch off yet");
        wanted = key.release(true, wanted);
        check(!wanted, "toggle: a tap while on switches off on the release");
        wanted = key.release(true, key.press(true, false));
        wanted = key.press(true, wanted);
        key.wheel();
        wanted = key.release(true, wanted);
        check(wanted, "toggle: using the wheel while holding the key keeps Zoom on");
        check(!key.release(false, true), "hold mode: the release ends Zoom");
    }
    lamium::Settings settings;
    settings.camera.magnification = -9;
    settings.normalize();
    check(settings.camera.magnification == 2, "normalize settings");
    settings.camera.magnification = 1.5f;
    settings.normalize();
    check(settings.camera.magnification == 2, "a saved magnification below 2x loads as 2x");
    settings.camera.magnification = 40;
    settings.normalize();
    check(settings.camera.magnification == 40, "magnification up to 50x is kept");
    std::cout << "Lamium: camera, settings storage, and item inspection checks passed\n";
} catch (std::exception const& error) {
    std::cerr << "Lamium test failure: " << error.what() << '\n';
    return 1;
}
