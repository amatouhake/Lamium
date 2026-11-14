#include "features/inventory/DeathLayout.h"
#include "features/inventory/DeathLayoutStore.h"
#include <map>
#include <random>
void check(bool, char const*);
namespace {
using namespace lamium::inventory::death;
Stack item(std::string kind, int count = 1, std::string variant = "", int maxStack = 64) {
    return {kind, kind + "|" + variant, count, maxStack};
}
// Runs the planner to the end, like the glue does one move per step.
Slots settle(Layout& layout, Slots now, int* moves = nullptr) {
    int n = 0;
    for (; n < 200; ++n) {
        markDone(layout, now);
        auto move = nextMove(layout, now);
        if (!move) break;
        now = applied(now, *move);
    }
    markDone(layout, now);
    if (moves) *moves = n;
    return now;
}
std::map<std::string, int> totals(Slots const& slots) {
    std::map<std::string, int> out;
    for (auto const& s : slots) if (!s.empty()) out[s.exact] += s.count;
    return out;
}
}
void deathLayoutTests() {
    {
        Layout layout;
        layout.slots[0] = item("sword", 1, "a", 1);
        layout.slots[1] = item("cobblestone", 64);
        layout.slots[5] = item("bread", 10);
        layout.slots[36] = item("helmet", 1, "", 1);
        Slots now{};
        now[0] = item("cobblestone", 64);
        now[1] = item("helmet", 1, "", 1);
        now[2] = item("bread", 10);
        now[3] = item("sword", 1, "a", 1);
        auto after = settle(layout, now);
        check(after[0] == layout.slots[0] && after[1] == layout.slots[1] && after[5] == layout.slots[5]
              && after[36] == layout.slots[36], "picked-up items go back to their death-time slots, armor put on");
        check(totals(after) == totals(now), "restoring never adds or loses an item");
        check(markDone(layout, after), "a fully restored layout is done");
    }
    {
        Layout layout;
        layout.slots[3] = item("cobblestone", 30);
        layout.slots[15] = item("cobblestone", 20);
        Slots now{};
        now[0] = item("cobblestone", 50);
        auto after = settle(layout, now);
        check(after[3].count == 30 && after[15].count == 20 && after[0].empty(), "merged stacks are split back");
    }
    {
        Layout layout;
        layout.slots[0] = item("sword", 1, "a", 1);
        Slots now{};
        now[0] = item("dirt", 5);
        now[4] = item("sword", 1, "a", 1);
        auto after = settle(layout, now);
        check(after[0] == layout.slots[0] && totals(after) == totals(now), "an item gained since death moves aside");
    }
    {
        Layout layout;
        layout.slots[0] = item("cobblestone", 10);
        Slots now{};
        now[0] = item("dirt", 5);
        for (int s = 1; s < armorFirst; ++s) now[s] = item("stone", 64);
        now[20] = item("cobblestone", 40);
        auto after = settle(layout, now);
        check(totals(after) == totals(now) && after[0].kind == "cobblestone",
              "with no free slot the item is swapped in whole, nothing dropped");
    }
    {
        Layout layout;
        layout.slots[2] = item("pickaxe", 1, "", 1);
        Slots now{};
        now[0] = item("dirt", 3);
        int moves = 0;
        auto after = settle(layout, now, &moves);
        check(moves == 0 && after == now && !markDone(layout, after), "a lost item leaves the inventory alone");
    }
    {
        Layout layout;
        layout.slots[36] = item("helmet", 1, "iron", 1);
        Slots now{};
        now[36] = item("helmet", 1, "gold", 1);
        now[7] = item("helmet", 1, "iron", 1);
        auto after = settle(layout, now);
        check(after == now, "an armor slot wearing something else is left alone");
    }
    {
        Layout layout;
        layout.slots[0] = item("bread", 10);
        layout.done[0] = true;
        layout.slots[1] = item("apple", 2);
        Slots now{};
        now[0] = item("dirt", 1);
        now[1] = item("bread", 10);
        now[2] = item("apple", 2);
        auto after = settle(layout, now);
        check(after[0] == now[0] && after[1] == item("apple", 2) && after[2] == item("bread", 10),
              "a slot once restored is never touched again");
    }
    {
        Layout layout;
        layout.slots[0] = item("sword", 1, "damage10", 1);
        Slots now{};
        now[6] = item("sword", 1, "damage30", 1);
        auto after = settle(layout, now);
        check(after[0].kind == "sword" && after[6].empty(), "the same item stands in when the exact one is not back");
        check(!markDone(layout, after), "a stand-in does not finish the layout");
    }
    {
        Layout layout;
        layout.slots[0] = item("bread", 10);
        Slots before{}, after{};
        after[3] = item("bread", 4);
        Slots unrelated{};
        unrelated[3] = item("dirt", 4);
        check(pickedUp(layout, before, after) && !pickedUp(layout, before, unrelated),
              "only picking up a layout item triggers a restore");
        check(!anyItem(Slots{}) && anyItem(after), "an empty snapshot is not a layout");
    }
    {
        // Random layouts and pickups: always ends, never changes the totals,
        // never touches equipment wearing something else.
        std::mt19937 random(109);
        char const* kinds[]{"a", "b", "c", "d"};
        bool ends = true, conserved = true, armorKept = true;
        for (int round = 0; round < 2000; ++round) {
            Layout layout;
            Slots now{};
            auto pick = [&](int n) { return static_cast<int>(random() % static_cast<unsigned>(n)); };
            for (int s = 0; s < slotCount; ++s) {
                if (pick(3)) continue;
                int maxStack = s >= armorFirst ? 1 : 64;
                layout.slots[s] = item(kinds[pick(4)], 1 + pick(maxStack), pick(2) ? "x" : "y", maxStack);
            }
            for (int s = 0; s < slotCount; ++s) {
                if (pick(3)) continue;
                int maxStack = s >= armorFirst ? 1 : 64;
                now[s] = item(kinds[pick(4)], 1 + pick(maxStack), pick(2) ? "x" : "y", maxStack);
            }
            int moves = 0;
            auto after = settle(layout, now, &moves);
            ends = ends && moves < 200;
            conserved = conserved && totals(after) == totals(now);
            for (int s = armorFirst; s < slotCount; ++s)
                if (!now[s].empty()) armorKept = armorKept && after[s] == now[s];
        }
        check(ends, "restoring always ends");
        check(conserved, "restoring never changes what the player has");
        check(armorKept, "worn equipment is never taken off");
    }
    {
        Layout layout;
        layout.x = -12; layout.y = 70; layout.z = 300; layout.dimension = 1;
        layout.slots[0] = item("sword", 1, "a", 1);
        layout.slots[40] = item("shield", 1, "", 1);
        layout.done[0] = true;
        check(decodeLayout(encodeLayout(layout)) == layout, "the death layout survives saving");
        layout.started = true;
        check(decodeLayout(encodeLayout(layout)).started, "that pickups began at the place is saved");
        check(atPlace(layout, 1, -11.5, 70, 330) && !atPlace(layout, 1, -11.5, 70, 340) && !atPlace(layout, 0, -11.5, 70, 300.5),
              "the place of the death is 32 blocks around it, in its dimension (L-332)");
        // L-338: the layout ends with the dropped items, five minutes of the place being loaded.
        Layout aging = layout;
        check(loadedAround(aging, 1, -11.5, 348) && !loadedAround(aging, 1, -11.5, 349) && !loadedAround(aging, 0, -11.5, 300.5)
              && loadedAround(aging, 1, -11.5, 300.5), "the place counts as loaded within 48 blocks across, at any height, in its dimension");
        bool early = false;
        for (int t = 0; t < despawnTicks - 1; ++t) early = early || age(aging, true);
        for (int t = 0; t < 100000; ++t) early = early || age(aging, false);
        check(!early && aging.loadedTicks == despawnTicks - 1, "time away from the place does not count");
        check(decodeLayout(encodeLayout(aging)).loadedTicks == despawnTicks - 1, "the count survives saving");
        check(age(aging, true) && age(aging, false), "after five minutes loaded the layout is over, and stays over");
        check(decodeLayout(R"({"version":1,"loadedTicks":-5})").loadedTicks == 0
              && decodeLayout(R"({"version":1})").loadedTicks == 0, "a missing or broken count starts from zero");
        bool rejected = false;
        try { (void)decodeLayout(R"({"version":2})"); } catch (...) { rejected = true; }
        check(rejected, "another version is rejected");
        auto tolerant = decodeLayout(R"({"version":1,"slots":[{"slot":99,"kind":"x","count":1},{"slot":3,"kind":"y","count":2}]})");
        check(tolerant.slots[3].count == 2 && tolerant.slots[3].exact == "y", "bad slots are skipped, missing fields default");
    }
    {
        Layout layout;
        layout.slots[0] = item("sword", 1, "a", 1);
        layout.slots[20] = item("bread", 10);
        layout.slots[36] = item("helmet", 1, "", 1);
        Slots now{};
        now[0] = item("bread", 10);
        now[5] = item("sword", 1, "a", 1);
        now[6] = item("helmet", 1, "", 1);
        Slots after = now;
        for (int n = 0; n < 50; ++n) {
            auto move = nextMove(layout, after, Scope::HotbarEquipment);
            if (!move) break;
            after = applied(after, *move);
        }
        check(after[0] == layout.slots[0] && after[36] == layout.slots[36] && after[20].empty(),
              "the default scope puts back the hotbar and equipment only");
        check(markDone(layout, after, Scope::HotbarEquipment) && !markDone(layout, after, Scope::All),
              "a scope is done when its own slots are");
        check(inScope(40, Scope::HotbarEquipment) && !inScope(9, Scope::HotbarEquipment) && inScope(9, Scope::All),
              "scopes name their slots");
    }
    {
        using E = LifeWatch::Event;
        LifeWatch joining;
        check(joining.update(false) == E::None && joining.update(true) == E::None,
              "reading as not alive while joining is not a death");
        check(joining.update(false) == E::Died && joining.update(false) == E::None && joining.update(true) == E::Respawned
              && joining.update(true) == E::None, "a death and its respawn are seen once each");
        LifeWatch rejoined;
        check(rejoined.update(true) == E::None, "joining alive is not a respawn");
    }
    {
        auto order = targetOrder();
        check(order[0] == 36 && order[4] == offhandSlot && order[5] == 0 && order[13] == 8 && order[40] == 35,
              "equipment first, then the hotbar, then the inventory");
    }
}
