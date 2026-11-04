#include "app/Capabilities.h"
#include "app/CapabilityNotice.h"
#include "app/CapabilityHooks.h"
#include "app/PausedFeatures.h"
#include <thread>
void check(bool, char const*);

void capabilitiesTests() {
    using namespace lamium::capabilities;
    check(loaderSupported(26,60,0) && loaderSupported(26,60,1)
        && loaderSupported(26,60,1000), "patch updates in the supported loader series need no allowlist edit");
    check(!loaderSupported(26,51,5) && !loaderSupported(26,61,0) && !loaderSupported(27,60,0)
        && !loaderSupported(-1,-1,-1), "older, unknown and other loader series remain outside the policy");
    Registry r{{true,26,60,1}};
    {
        Registry cleanup{{true,26,60,1}};
        bool installed = false;
        int removals = 0, failures = 0;
        auto failed = [&] {
            ++failures;
            cleanup.fail(Id::FreeCameraHud,Reason::RemovalFailed);
            cleanup.fail(Id::FreeCameraHotbar,Reason::RemovalFailed);
        };
        removeHook(installed,[&] { ++removals; return false; },failed);
        check(removals == 0 && failures == 0, "never remove an unattached hook or report a false cleanup failure");
        installed = true;
        cleanup.ready(Id::FreeCameraHud); cleanup.ready(Id::FreeCameraHotbar);
        cleanup.stop(Id::FreeCameraHud); cleanup.stop(Id::FreeCameraHotbar);
        removeHook(installed,[&] { ++removals; return false; },failed);
        cleanup.reset();
        check(installed && removals == 1 && failures == 1
            && !cleanup.ready(Id::FreeCameraHud) && !cleanup.ready(Id::FreeCameraHotbar)
            && cleanup.status(Id::FreeCameraHud).reason() == Reason::RemovalFailed
            && cleanup.status(Id::FreeCameraHotbar).reason() == Reason::RemovalFailed,
            "a failed shared hook removal blocks all dependent capabilities across restarts");
        removeHook(installed,[]() -> bool { throw 1; },failed);
        bool otherInstalled = true;
        removeHook(otherInstalled,[] { return true; },failed);
        check(installed && !otherInstalled && failures == 2,
            "a throwing removal retains the hook and permits subsequent hooks to clean up");
        removeHook(installed,[] { return true; },failed);
        cleanup.reset();
        check(!installed && cleanup.status(Id::FreeCameraHud).reason() == Reason::RemovalFailed,
            "later cleanup success cannot clear the process-restart requirement");
        int attachments = 0;
        auto attach = [&] { ++attachments; return 0; };
        auto readCleanup = [&](Id id) { return cleanup.status(id); };
        check(!attachHook(installed,{Id::FreeCameraHud,Id::FreeCameraHotbar},attach,readCleanup)
            && !installed && attachments == 0,
            "a cleaned-up hook still cannot be reattached after a latched removal failure");
        installed = true;
        check(!attachHook(installed,{Id::FreeCameraHud,Id::FreeCameraHotbar},attach,readCleanup) && attachments == 0,
            "a retained installed flag cannot make a failed hook reusable");
        installed = false;
        cleanup.fail(Id::HideParticles,Reason::RemovalFailed);
        check(attachHook(installed,{Id::HideWeather,Id::HideParticles},attach,readCleanup)
            && installed && attachments == 1 && !cleanup.status(Id::HideParticles).available(),
            "a shared hook can serve its unaffected consumer without reviving the failed capability");
    }
    auto read = [&](Id id) { return r.status(id); };
    auto terrain = Id::FreeCameraTerrain;
    check(r.status(terrain).waiting() && r.status(terrain).canProbe() && !r.status(terrain).available()
        && !r.status(terrain).paused(), "waiting for the first owned renderer permits checking, not overriding or a failure warning");
    check(featureImpact("freecamera",read) == Impact::None, "pending camera context does not mark the camera paused");
    unsigned revision = r.revision();
    check(r.ready(terrain) && r.status(terrain).available() && r.revision() > revision,
        "a successful native check publishes availability");
    revision = r.revision();
    check(!r.ready(terrain) && r.revision() == revision, "repeated successful frames do not publish or log transitions");
    check(r.fail(terrain,Reason::NativeContract) && r.status(terrain).reason() == Reason::NativeContract
        && !r.status(terrain).canProbe(), "a native contract failure stops only its adapter");
    check(featureImpact("freecamera",read) == Impact::Partly && featureImpact("zoom",read) == Impact::None
        && r.status(Id::GameLanguage).available(), "failed camera terrain preserves its independent and unrelated paths");
    revision = r.revision();
    check(!r.ready(terrain) && !r.fail(terrain,Reason::HookUnavailable) && !r.stop(terrain)
        && r.revision() == revision && r.status(terrain).reason() == Reason::NativeContract,
        "later frames and cleanup cannot erase the first failure or spam its reason");
    check(r.begin(terrain) && r.status(terrain).waiting() && r.ready(terrain),
        "only an explicit lifecycle restart retries a failed lazy adapter");
    check(r.stop(terrain) && !r.status(terrain).available() && !r.ready(terrain),
        "stopped adapters cannot become available from a late callback");
    r.begin(terrain); r.ready(terrain);
    r.ready(Id::FreeCameraHud);
    r.fail(Id::FreeCameraHotbar,Reason::HookUnavailable);
    check(r.status(Id::FreeCameraHud).available() && r.status(terrain).available()
        && featureImpact("freecamera",read) == Impact::Partly,
        "a missing hotbar hook leaves whole-HUD hiding and terrain independent");
    r.fail(Id::NightVision,Reason::HookUnavailable);
    check(featureImpact("nightVision",read) == Impact::Fully && featureImpact("freecamera",read) == Impact::Partly,
        "feature aggregation distinguishes essential failures from partial loss");
    check(r.status(Id::SchematicPiston).available(), "direct SDK adapters have no invented first-render prerequisite");

    Registry unknown{{false,26,60,1}}, oldLoader{{true,26,51,4}};
    for (auto const& d : definitions) {
        unknown.ready(d.id); oldLoader.ready(d.id);
        if (d.gameSensitive) {
            check(unknown.status(d.id).reason() == Reason::UnverifiedGame && !unknown.status(d.id).available()
                && !unknown.status(d.id).canProbe(), "every version-sensitive path remains vanilla on an unverified game");
            check(oldLoader.status(d.id).reason() == Reason::UnsupportedLoader,
                "every native adapter uses the common loader policy");
        } else check(unknown.status(d.id).available() && oldLoader.status(d.id).available(),
            "version-independent paths do not inherit unnecessary version restrictions");
    }
    unknown.fail(terrain,Reason::TargetChanged);
    check(unknown.status(terrain).reason() == Reason::UnverifiedGame
        && unknown.status(terrain).runtime.reason == Reason::TargetChanged,
        "compatibility and the native result remain separate without accidentally widening either");
    auto reviewed = definition(terrain);
    reviewed.verifiedBuild = {1,26,52,1};
    Environment nextGame{false,26,60,1,{1,26,52,1}};
    check(compatibility(reviewed,nextGame) == Reason::None && compatibility(terrain,nextGame) == Reason::UnverifiedGame,
        "a separately reviewed game build is scoped to one capability, leaving the shared baseline closed");
    nextGame.forceUnverified = true;
    check(compatibility(reviewed,nextGame) == Reason::UnverifiedGame,
        "the forced-unverified build cannot be bypassed by a per-capability review");
    r.fail(Id::HideParticles,Reason::NativeContract);
    check(r.fail(Id::HideParticles,Reason::RemovalFailed),
        "cleanup failure escalates an earlier contract failure so stale hooks cannot be retried");
    r.reset();
    check(!r.begin(Id::HideParticles) && !r.ready(Id::HideParticles)
        && r.status(Id::HideParticles).reason() == Reason::RemovalFailed,
        "a failed native cleanup requires a process restart instead of reusing stale hooks");
    Registry fresh{{true,26,60,1}};
    for (auto const& failed : definitions) {
        Registry isolated{{true,26,60,1}};
        for (auto const& d : definitions) isolated.ready(d.id);
        isolated.fail(failed.id,Reason::HookUnavailable);
        for (auto const& d : definitions)
            check(isolated.status(d.id).available() == (d.id != failed.id),
                "every catalog capability fails independently without disabling another adapter");
    }
    lamium::Settings settings;
    settings.inventory.repeatTrades = false; settings.inventory.transfer = false;
    settings.lighting.nightVision = true;
    check(lamium::paused::affected(settings,[&](Id id) { return fresh.status(id); }).empty(),
        "a saved switch alone neither grants availability nor creates a failure");
    fresh.fail(Id::NightVision,Reason::HookUnavailable);
    auto selected = lamium::paused::affected(settings,[&](Id id) { return fresh.status(id); });
    check(selected.size() == 1 && selected[0] == "nightVision", "notices project requested features from actual failed subpaths");
    settings.lighting.nightVision = false;
    check(lamium::paused::affected(settings,[&](Id id) { return fresh.status(id); }).empty()
        && fresh.status(Id::NightVision).reason() == Reason::HookUnavailable,
        "turning a setting off hides its notice without erasing the adapter's failure");
    settings.visuals.hideEffects = true; settings.visuals.hideParticles = true;
    fresh.fail(Id::HideParticles,Reason::HookUnavailable);
    selected = lamium::paused::affected(settings,[&](Id id) { return fresh.status(id); });
    check(selected.size() == 1 && selected[0] == "hideEffects", "non-version hook failures also reach the shared notices");
    Notice notice;
    check(notice.poll(1,1) && !notice.changed(""), "pending initialization stays quiet on join");
    check(notice.poll(1,2) && notice.changed("terrain failed"), "a meaningful failure after joining notifies");
    check(!notice.poll(1,2) && notice.poll(1,3) && !notice.changed("terrain failed"),
        "unchanged frames and unrelated ready transitions do not repeat notices");
    check(notice.poll(2,3) && notice.changed("terrain failed"), "the same persistent failure can be explained in a new world");
    check(!notice.poll(0,4) && notice.poll(2,4) && notice.changed("terrain failed"), "world exit clears notice context");
    Registry concurrent{{true,26,60,1}};
    std::thread writer([&] { for (int i=0; i<1000; ++i) concurrent.ready(terrain); concurrent.fail(terrain,Reason::NativeContract); });
    for (int i=0; i<1000; ++i) {
        auto status = concurrent.status(terrain);
        check(status.runtime.stage != Stage::Failed || status.runtime.reason == Reason::NativeContract,
            "readers observe one coherent owned stage/reason value");
    }
    writer.join();
    check(!concurrent.ready(terrain) && concurrent.status(terrain).runtime.stage == Stage::Failed,
        "concurrent ready reports cannot resurrect a failed capability");
}
