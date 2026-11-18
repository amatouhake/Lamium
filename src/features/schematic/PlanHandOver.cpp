#include "features/schematic/PlanHandOver.h"
#include "app/Runtime.h"
#include "features/recipes/RecipeState.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/Verification.h"

namespace lamium::schematic::planHandOver {
namespace {
// The last pass's counts of the checked placement: a pass that is running or outdated still says
// roughly what is left, and the next one corrects it.
std::vector<std::pair<std::string, recipes::Count>> targets() {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled()) return {};
    auto const settings = runtime.snapshot();
    if (!settings->schematic.enabled || !settings->schematic.planMaterials) return {};
    auto const checked = ghosts::verification();
    if (!checked || checked->placement < 0) return {};
    std::vector<std::pair<std::string, recipes::Count>> out;
    for (auto& [item, count] : planTargets(checked->materials)) out.emplace_back(std::move(item), count);
    return out;
}
}
void start() { recipes::state::provide("schematic", targets); }
void stop() { recipes::state::provide("schematic", nullptr); }
}
