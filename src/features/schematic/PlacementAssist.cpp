#include "features/schematic/PlacementAssist.h"
#include "features/schematic/AssistPlan.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostPick.h"
#include "features/inventory/BringToHand.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ui/Toast.h"
#include "ui/Localization.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/InventoryTransactionPacket.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/inventory/transaction/ItemUseInventoryTransaction.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/phys/HitResult.h"
#include <chrono>
#include <mutex>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace lamium::schematic::assist {
void placeNow();
void repeatPress();
namespace {
// Client thread only: the build calls, the use call, the auth input send and
// the queued placement all run there.
struct Pending {
    BlockPos cell, from;    // the cell placed into, and the neighbor clicked
    unsigned char face = 1; // the neighbor's face clicked
    Vec3 click;             // relative to the neighbor
    Look look;
    std::string item;
    short aux = 0;
    int dimension = 0;
    std::string wantName; // the ghost's block, read back later
    States wantStates;
    int authsFaked = 0;
    bool queued = false;
    // Auth inputs to let pass first: the block came into the hand by an
    // inventory transaction, and a placement sent at once was refused and
    // the slot put back (2026-11-28 run, to confirm by its log).
    int waitAuths = 0;
    bool swapped = false;
};
std::optional<Pending> pending;
inline constexpr int swapWait = 4; // auth inputs after a swap or give
// Placements read back a few auth inputs later (a held run makes several).
struct Sent {
    BlockPos cell;
    std::string wantName;
    States wantStates;
    int dimension = 0, wait = 0;
    BlockPos from;
    double reachToFrom = 0; // eye to the clicked neighbor's centre
    bool swapped = false;
};
std::vector<Sent> sent;
// Two seconds: an inventory-only block was seen reverted after the half
// second this used to wait, with no line logged (2026-11-28).
inline constexpr int readBackAfter = 40;
// The ghost and item the assist last could not place: a held button asks
// again every tick, and the answer stays the same until one changes.
struct Refused {
    BlockPos cell;
    std::string item;
    std::chrono::steady_clock::time_point at;
};
std::optional<Refused> refused;
// Asked again after this long: a block placed beside the ghost meanwhile may
// give it a face to be clicked from.
inline constexpr auto refusalKept = std::chrono::seconds(1);
// The look the server last got: it places with the auth input sent before the
// placement, not the client's newer rotation (L-265 first trace).
std::optional<Vec2> lastSentLook;
// A press the assist took: while the button stays held, vanilla's repeated
// build calls place nothing of their own (a held run put blocks the
// schematic does not have where no ghost was aimed). Ends with the press, or
// when no build call came for a moment (a press in the air ends unseen).
std::optional<std::chrono::steady_clock::time_point> pressTaken;
inline constexpr auto pressGap = std::chrono::milliseconds(400);
// A press the assist took and the button still held (ended by
// stopBuildBlock): the game repeats its build call only while its hit is in
// reach, so the assist repeats by itself (2026-11-28 trace).
bool holding = false;
std::chrono::steady_clock::time_point lastTry{};
// Two ticks: 250 ms felt slow next to vanilla's held placement (2026-11-28).
inline constexpr auto repeatGap = std::chrono::milliseconds(100);
bool placing = false, installed = false;
ll::event::ListenerPtr exitListener;

void log(std::string const& text) {
    static int lines = 0;
    if (++lines > 200) return;
    try { Runtime::instance().self().getLogger().info("Placement assist: {}", text); } catch (...) {}
}
bool on() {
    auto& runtime = Runtime::instance();
    auto const& s = runtime.snapshot()->schematic;
    return runtime.enabled() && s.enabled && s.placementAssist && versionSensitiveAllowed(capabilities::Id::PlacementAssist);
}
LocalPlayer* localPlayer();
// Why the last press was left to vanilla, for the trace.
char const* declined = "";
bool decline(char const* why) { declined = why; return false; }
// Whether the game's hit is no block the player can use: nothing, or a block
// or entity beyond the pick range (L-265: the press then reaches only the
// item use; traces 2026-11-28 saw blocks 30-68 and entities 21-25 away).
bool meetsNoBlock(HitResult const& hit, Player& actor) {
    auto* player = localPlayer();
    if (!player || static_cast<Player*>(player) != &actor) return false;
    if (hit.mType == HitResultType::NoHit || hit.mType == HitResultType::EntityOutOfRange) return true;
    if (hit.mIndirectHit) return false;
    double const ex = hit.mPos.x - hit.mStartPos.x, ey = hit.mPos.y - hit.mStartPos.y, ez = hit.mPos.z - hit.mStartPos.z;
    return std::sqrt(ex * ex + ey * ey + ez * ez) > player->getPickRange() + 1e-3;
}
#ifdef LAMIUM_ASSIST_TRACE
// L-265 research: a press that placed nothing and said nothing at some
// angles. Which call it reached, what the game hit and the ghost aimed at.
// The game calls stopBuildBlock every tick: a release is logged only after
// a logged press.
void tracePress(char const* call, bool taken) noexcept {
    static int lines = 0;
    static bool pressed = false; // a release is logged only after a logged press
    bool const release = std::string_view{call}.starts_with("stopBuildBlock");
    if (release && !pressed) return;
    pressed = !release;
    if (++lines > 3000) return;
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) return;
        HitResult const& hit = client->getLatestHitResult();
        double const ex = hit.mPos.x - hit.mStartPos.x, ey = hit.mPos.y - hit.mStartPos.y, ez = hit.mPos.z - hit.mStartPos.z;
        auto ghost = ghosts::aimedGhost(*player, hit);
        Vec2 const rot = player->getRotation();
        Runtime::instance().self().getLogger().info(
            "Assist trace: {} taken={} why={} on={} hit={} indirect={} block={},{},{} face={} dist={:.2f} liquid={} pitch={:.1f} reach={:.1f} eye={:.2f},{:.2f},{:.2f} ghost={}",
            call, taken, taken ? "" : declined, on(), static_cast<int>(hit.mType), hit.mIndirectHit, hit.mBlock.x, hit.mBlock.y, hit.mBlock.z,
            static_cast<int>(hit.mFacing), std::sqrt(ex * ex + ey * ey + ez * ez), hit.mIsHitLiquid, rot.x, player->getPickRange(),
            player->getEyePos().x, player->getEyePos().y, player->getEyePos().z,
            ghost ? std::format("{},{},{}", ghost->cell.x, ghost->cell.y, ghost->cell.z) : std::string{"none"});
    } catch (...) {}
}
#define ASSIST_TRACE(call, taken) tracePress(call, taken)
#else
#define ASSIST_TRACE(call, taken) ((void)0)
#endif
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
std::string nameOf(Block const& block) { return block.getBlockType().mNameInfo->mFullName->getString(); }
BlockPos step(BlockPos pos, unsigned char dir) {
    auto const& o = offsets[dir];
    return {pos.x + o[0], pos.y + o[1], pos.z + o[2]};
}
bool buildable(BlockSource& region, BlockPos const& pos) {
    Block const& block = region.getBlock(pos);
    return block.getBlockType().canBeBuiltOver(block, region, pos);
}
bool holds(Player& player, std::string const& item, short aux) {
    auto const& held = player.getSelectedItem();
    return !held.isNull() && held.getTypeName() == item && held.getAuxValue() == aux;
}
// The local player's rotation set to a look for one call (the block's own
// rule and the client's own placement read it), and put back after.
class AsLook {
    Vec2* rot = nullptr;
    Vec2 was;

public:
    AsLook(Player& player, Look const& look) {
        rot = &*player.mBuiltInComponents->mActorRotationComponent->mRot;
        was = *rot;
        *rot = Vec2{look.pitch, look.yaw};
    }
    ~AsLook() {
        if (rot) *rot = was;
    }
    AsLook(AsLook const&) = delete;
    AsLook& operator=(AsLook const&) = delete;
};
// The level's hit result and the game mode's build context as a fresh press
// on the chosen face and click, for the one buildBlock call (L-265 traces: a
// fresh press takes the click from the hit result; a held one did not).
class AsClicked {
    HitResult& hit;
    HitResult hitWas;
    GameMode& mode;
    GameMode::BuildContext contextWas;

public:
    AsClicked(Player& player, GameMode& m, Pending const& at)
        : hit(player.getLevel().getHitResult()), hitWas(hit), mode(m), contextWas(*m.mBuildContext) {
        hit.mType = HitResultType::Tile;
        hit.mBlock = at.from;
        hit.mFacing = at.face;
        hit.mPos = Vec3{static_cast<float>(at.from.x) + at.click.x, static_cast<float>(at.from.y) + at.click.y,
                        static_cast<float>(at.from.z) + at.click.z};
        hit.mIndirectHit = false;
        hit.mIsHitLiquid = false;
        mode.mBuildContext->mHasBuildDirection = false;
        mode.mBuildContext->mHasLastBuiltPosition = false;
        mode.mBuildContext->mInitialClickPos = hit.mPos;
    }
    ~AsClicked() {
        hit = hitWas;
        *mode.mBuildContext = contextWas;
    }
    AsClicked(AsClicked const&) = delete;
    AsClicked& operator=(AsClicked const&) = delete;
};

void toast(char const* key, std::string const& block) {
    try { ui::showMessageToast(ui::translated(key, block), 2.0); } catch (...) {}
}

// What a press now would do: the ghost it aims at and whether and how that
// ghost can be placed. No side effects, so the aim shown before a press
// (L-265) and the press itself decide alike.
struct Plan {
    enum Verdict { NoGhost, RealBlock, NoItem, SecondHalf, InTheWay, NoWay, Placeable } verdict = NoGhost;
    std::optional<ghosts::GhostAt> ghost;
    BlockPos cell;
    Block const* want = nullptr;
    ItemStack stack;
    std::string item, name, wantName, difference;
    short aux = 0;
    States wantStates;
    std::optional<Candidate> chosen;
};
Plan planFor(LocalPlayer& player, HitResult const& hit) {
    Plan plan;
    auto& region = player.getDimensionBlockSource();
    // A neighbor that opens or toggles when used is clicked only while sneaking.
    bool const sneaking = player.isSneaking();
    auto clickableAround = [&](BlockPos const& cell) {
        std::array<bool, 6> out{};
        for (unsigned char dir = 0; dir < 6; ++dir) {
            BlockPos const at = step(cell, dir);
            out[dir] = !buildable(region, at) && (sneaking || !region.getBlock(at).getBlockType().isInteractiveBlock());
        }
        return out;
    };
    // The ghost under the crosshair, the first on the ray, never one behind
    // it (maintainer 2026-11-28); with shrunk ghosts the ray reaches an inner
    // one between them. An unplaceable one is shown red, not walked past.
    plan.ghost = ghosts::aimedGhost(player, hit);
    if (!plan.ghost || !plan.ghost->block) return plan;
    Block const& want = *plan.ghost->block;
    plan.want = &want;
    plan.cell = BlockPos{plan.ghost->cell.x, plan.ghost->cell.y, plan.ghost->cell.z};
    plan.verdict = Plan::RealBlock;
    if (!buildable(region, plan.cell)) return plan;
    plan.verdict = Plan::NoItem;
    plan.stack = ghosts::ghostItem(want);
    if (plan.stack.isNull()) return plan; // fire, water: nothing to place by hand
    plan.item = plan.stack.getTypeName();
    plan.aux = plan.stack.getAuxValue();
    plan.name = ghosts::describe(want, plan.item).name;
    // A second half (a door's top, a bed's head) comes with its first.
    plan.verdict = Plan::SecondHalf;
    if (ghosts::describe(want, plan.item).perBlock == 0) return plan;
    // The player's own body in the cell: the game would not place there.
    plan.verdict = Plan::InTheWay;
    if (auto const& body = player.getAABB(); body.min.x < plan.cell.x + 1 && body.max.x > plan.cell.x && body.min.y < plan.cell.y + 1
        && body.max.y > plan.cell.y && body.min.z < plan.cell.z + 1 && body.max.z > plan.cell.z)
        return plan;
    // Asked of the ghost's own item before the hand changes: nothing
    // switches where nothing places. An item that is no block (a door, a
    // sign) asks the ghost's own block type.
    Block const* placed = plan.stack.mBlock ? plan.stack.mBlock : &want;
    std::array<bool, 6> const clickable = clickableAround(plan.cell);
    Vec2 const rot = lastSentLook.value_or(player.getRotation());
    plan.wantName = nameOf(want);
    plan.wantStates = ghosts::blockStates(want);
    for (Candidate const& c : candidates(clickable, rot.x, rot.y)) {
        Vec3 const click{c.click[0], c.click[1], c.click[2]};
        Block const* predicted = nullptr;
        {
            AsLook const look(player, c.look);
            predicted = &placed->getBlockType().getPlacementBlock(player, plan.cell, c.face, click, plan.aux);
        }
        std::string const predictedName = nameOf(*predicted);
        States const predictedStates = ghosts::blockStates(*predicted);
        if (placesAs(predictedName, predictedStates, plan.wantName, plan.wantStates)) {
            plan.chosen = c;
            plan.verdict = Plan::Placeable;
            return plan;
        }
        if (plan.difference.empty())
            plan.difference = predictedName != plan.wantName ? predictedName : firstDifference(predictedStates, plan.wantStates);
    }
    std::string around;
    for (unsigned char dir = 0; dir < 6; ++dir)
        around += std::format("{}{}{}", dir ? ", " : "", nameOf(region.getBlock(step(plan.cell, dir))), clickable[dir] ? "" : " (no)");
    if (plan.difference.empty()) plan.difference = "no neighbor to click among " + around;
    else plan.difference += "; neighbors " + around;
    plan.verdict = Plan::NoWay;
    return plan;
}

// Whether the assist takes this press. Called from vanilla's build and use
// calls; the placement itself is queued. Whatever the hand holds: a tool or
// food is used by looking off the ghost (maintainer 2026-11-28, once ghosts
// could be shrunk; switching back after breaking a wrong block was a chore).
bool takePress(Player& actor) noexcept {
    declined = "";
    lastTry = std::chrono::steady_clock::now();
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (placing || !player || static_cast<Player*>(player) != &actor || !on()) return decline(placing ? "placing" : "off");
        auto const now = std::chrono::steady_clock::now();
        bool const held = pressTaken && now - *pressTaken < pressGap;
        if (pending) {
            pressTaken = now;
            return true; // one placement at a time; a held button tries again
        }
        Plan const plan = planFor(*player, client->getLatestHitResult());
        if (plan.verdict == Plan::NoGhost) {
            if (!held) return decline("no ghost aimed");
            pressTaken = now; // the press stays the assist's: no vanilla block where no ghost is aimed
            return true;
        }
        pressTaken = now;
        if (!plan.want) return true;
        if (plan.verdict == Plan::RealBlock) return held || decline("real block in the ghost's cell"); // a real block there stays the player's
        if (plan.verdict == Plan::NoItem) return decline("no item");
        BlockPos const cell = plan.cell;
        if (refused && refused->cell == cell && (refused->item == plan.item || refused->item.empty()) && now - refused->at < refusalKept)
            return true;
        auto refuse = [&](char const* key) {
            toast(key, plan.name);
            refused = Refused{cell, plan.item, now};
            return true;
        };
        if (plan.verdict == Plan::SecondHalf || plan.verdict == Plan::InTheWay) return refuse("schematic.assist.noWay");
        if (plan.verdict == Plan::NoWay) {
            log(std::format("cannot place {} at {},{},{}: {}", plan.wantName, cell.x, cell.y, cell.z, plan.difference));
            return refuse("schematic.assist.noWay");
        }
        std::string const& item = plan.item;
        short const aux = plan.aux;
        bool swapped = false;
        if (!holds(*player, item, aux)) {
            int const selectedWas = player->mInventory->mSelected;
            bool given = false;
            int slot = inventory::bringToHand(*player, item, aux);
            if (slot < 0 && player->isCreative() && versionSensitiveAllowed(capabilities::Id::SchematicCreativePick)) {
                slot = inventory::giveToHand(*player, plan.stack);
                given = slot >= 0;
            }
            if (slot < 0 || !holds(*player, item, aux)) return refuse("schematic.assist.notCarried");
            // A swap into the selected slot or a creative give went as a transaction.
            swapped = given || slot == selectedWas;
        }
        Candidate const& c = *plan.chosen;
        Vec3 const click{c.click[0], c.click[1], c.click[2]};
        refused.reset();
        pending = Pending{cell, step(cell, c.dir), c.face, click, c.look, item, aux, static_cast<int>(player->getDimensionId()),
                          plan.wantName, plan.wantStates};
        pending->swapped = swapped;
        if (swapped) pending->waitAuths = swapWait;
        if (!c.look.faked && !swapped) {
            pending->queued = true;
            ll::thread::ClientThreadExecutor::getDefault().execute([] { placeNow(); });
        }
        return true;
    } catch (...) { return decline("exception"); }
}
// The aim shown before a press, from the client thread each auth input and
// read by the ghost pass (any thread).
std::mutex aimLock;
std::optional<Aim> aimNow;
void updateAim() noexcept {
    std::optional<Aim> next;
    try {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (player && on() && !placing) {
            Plan const plan = planFor(*player, client->getLatestHitResult());
            if (plan.ghost && plan.want && plan.verdict != Plan::RealBlock) {
                Aim aim;
                aim.dimension = static_cast<int>(player->getDimensionId());
                for (int k = 0; k < 3; ++k) { aim.low[k] = plan.ghost->low[k]; aim.high[k] = plan.ghost->high[k]; }
                bool const inHand = plan.verdict == Plan::Placeable
                    && (holds(*player, plan.item, plan.aux) || inventory::carried(*player, plan.item, plan.aux)
                        || (player->isCreative() && versionSensitiveAllowed(capabilities::Id::SchematicCreativePick)));
                aim.placeable = inHand;
                next = aim;
            }
        }
    } catch (...) { next.reset(); }
    std::lock_guard lock(aimLock);
    aimNow = next;
}
}
// Sends the pending placement. Queued to the client thread after the auth
// input that carried its look (or at once when nothing is faked).
void placeNow() {
    try {
        if (!pending) return;
        Pending const at = *pending;
        auto* player = localPlayer();
        GameMode* mode = player ? player->mGameMode.get() : nullptr;
        if (!player || !mode || static_cast<int>(player->getDimensionId()) != at.dimension || !on() || !holds(*player, at.item, at.aux)) {
            pending.reset();
            return;
        }
        bool built = false;
        placing = true;
        try {
            AsLook const look(*player, at.look);
            AsClicked const clicked(*player, *mode, at);
            built = mode->buildBlock(at.from, at.face, HandSlot::Mainhand, false);
        } catch (...) {}
        placing = false;
        if (pending) log(std::format("no placement left for {},{},{} (buildBlock {})", at.cell.x, at.cell.y, at.cell.z, built));
        pending.reset();
        Vec3 const eye = player->getEyePos();
        double const fx = at.from.x + .5 - eye.x, fy = at.from.y + .5 - eye.y, fz = at.from.z + .5 - eye.z;
        if (sent.size() < 16)
            sent.push_back({at.cell, at.wantName, at.wantStates, at.dimension, readBackAfter, at.from, std::sqrt(fx * fx + fy * fy + fz * fz), at.swapped});
    } catch (...) {
        placing = false;
        pending.reset();
    }
}
// The button still held after a press the assist took: aims again from the
// crosshair now and places as a new press would (queued from the auth input).
void repeatPress() {
    try {
        auto* player = localPlayer();
        if (!holding || pending || placing || !player) return;
        [[maybe_unused]] bool const taken = takePress(*player);
        ASSIST_TRACE("repeat (held)", taken);
    } catch (...) {}
}
namespace {
using AuthInput = PlayerAuthInputPacketPayload::InputData;
void setFlag(PlayerAuthInputPacket& auth, AuthInput flag, bool value) { auth.mInputData->mContainer.set(static_cast<size_t>(flag), value); }
bool hasFlag(PlayerAuthInputPacket& auth, AuthInput flag) { return auth.mInputData->mContainer.test(static_cast<size_t>(flag)); }
// The auth input says the pending look; the move input is turned with it so
// the movement stays the same (L-265 fifth trace).
void fakeLook(PlayerAuthInputPacket& auth, Look const& look) {
    float const realYaw = auth.mRot->y;
    auto turn = [&](Vec2& v) {
        Move const m = turnedMove({v.x, v.z}, realYaw, look.yaw);
        v = Vec2{m.strafe, m.forward};
    };
    turn(*auth.mMove);
    turn(*auth.mRawMoveVector);
    turn(*auth.mAnalogMoveVector);
    bool const keyed = hasFlag(auth, AuthInput::Up) || hasFlag(auth, AuthInput::Down) || hasFlag(auth, AuthInput::Left)
                    || hasFlag(auth, AuthInput::Right);
    bool const diagonals = hasFlag(auth, AuthInput::UpLeft) || hasFlag(auth, AuthInput::UpRight)
                        || hasFlag(auth, AuthInput::DownLeft) || hasFlag(auth, AuthInput::DownRight);
    if (keyed || diagonals) {
        Keys const k = keysOf({auth.mMove->x, auth.mMove->z});
        setFlag(auth, AuthInput::Up, k.up);
        setFlag(auth, AuthInput::Down, k.down);
        setFlag(auth, AuthInput::Left, k.left);
        setFlag(auth, AuthInput::Right, k.right);
        setFlag(auth, AuthInput::UpLeft, diagonals && k.up && k.left);
        setFlag(auth, AuthInput::UpRight, diagonals && k.up && k.right);
        setFlag(auth, AuthInput::DownLeft, diagonals && k.down && k.left);
        setFlag(auth, AuthInput::DownRight, diagonals && k.down && k.right);
    }
    *auth.mRot = Vec2{look.pitch, look.yaw};
    auth.mYHeadRot = look.yaw;
    *auth.mInteractRotation = Vec2{look.pitch, look.yaw};
    auto const view = viewOf(look.pitch, look.yaw);
    *auth.mCameraOrientation = Vec3{view[0], view[1], view[2]};
}
void readBack() {
    if (sent.empty()) return;
    try {
        auto* player = localPlayer();
        for (auto it = sent.begin(); it != sent.end();) {
            if (--it->wait > 0) { ++it; continue; }
            if (player && static_cast<int>(player->getDimensionId()) == it->dimension) {
                Block const& now = player->getDimensionBlockSource().getBlock(it->cell);
                std::string const name = nameOf(now);
                States const states = ghosts::blockStates(now);
                if (!placesAs(name, states, it->wantName, it->wantStates)) {
                    // Not asked again while the button stays held.
                    refused = Refused{it->cell, "", std::chrono::steady_clock::now()};
                    log(std::format("{},{},{} holds {} after the placement, not the ghost's {} ({}; clicked {},{},{} {:.2f} from the eye{})",
                                    it->cell.x, it->cell.y, it->cell.z, name, it->wantName,
                                    name == it->wantName ? firstDifference(states, it->wantStates) : "other block", it->from.x,
                                    it->from.y, it->from.z, it->reachToFrom, it->swapped ? ", after a swap" : ""));
                }
            }
            it = sent.erase(it);
        }
    } catch (...) { sent.clear(); }
}
LL_TYPE_INSTANCE_HOOK(PlacementAssistSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    bool queue = false;
    try {
        auto client = ll::service::getClientInstance();
        if (client && packet.mSenderSubId == client->getClientSubId()) {
            if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket) {
                readBack();
                Vec2 const realLook = *static_cast<PlayerAuthInputPacket&>(packet).mRot;
                ll::thread::ClientThreadExecutor::getDefault().execute([] { updateAim(); });
                if (holding && !pending && std::chrono::steady_clock::now() - lastTry >= repeatGap)
                    ll::thread::ClientThreadExecutor::getDefault().execute([] { repeatPress(); });
                // Every auth input until the placement leaves carries its look:
                // the queued placement may run after the next one.
                if (pending && pending->waitAuths > 0) {
                    if (--pending->waitAuths == 0 && !pending->look.faked) queue = pending->queued = true;
                } else if (pending && pending->look.faked) {
                    if (!on() || pending->authsFaked >= 3) pending.reset();
                    else {
                        fakeLook(static_cast<PlayerAuthInputPacket&>(packet), pending->look);
                        ++pending->authsFaked;
                        if (!pending->queued) queue = pending->queued = true;
                    }
                } else if (pending && ++pending->authsFaked > 3) pending.reset();
                lastSentLook = realLook;
            } else if (packet.getId() == MinecraftPacketIds::InventoryTransaction && pending && placing) {
                auto& transaction = static_cast<InventoryTransactionPacket&>(packet);
                auto* complex = transaction.mTransaction.get();
                if (complex && complex->mType == ComplexInventoryTransaction::Type::ItemUseTransaction) {
                    auto& use = static_cast<ItemUseInventoryTransaction&>(*complex);
                    if (use.mActionType == ItemUseInventoryTransaction::ActionType::Place) {
                        Vec3 const click = *use.mClickPos;
                        if (std::abs(click.x - pending->click.x) > 0.01f || std::abs(click.y - pending->click.y) > 0.01f
                            || std::abs(click.z - pending->click.z) > 0.01f) {
                            log(std::format("sent click {:.2f},{:.2f},{:.2f} set to {:.2f},{:.2f},{:.2f}", click.x, click.y, click.z,
                                            pending->click.x, pending->click.y, pending->click.z));
                            *use.mClickPos = pending->click;
                        }
                        pending.reset();
                    }
                }
            }
        }
    } catch (...) {}
    origin(packet);
    if (queue) ll::thread::ClientThreadExecutor::getDefault().execute([] { placeNow(); });
}
#define BUILD_HOOKS(Mode) \
LL_TYPE_INSTANCE_HOOK(PlacementAssist##Mode##Build, ll::memory::HookPriority::Highest, Mode, \
    &Mode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) { \
    bool const taken = hand == HandSlot::Mainhand && takePress(mPlayer); \
    ASSIST_TRACE(#Mode " buildBlock", taken); \
    if (taken) { holding = true; return false; } \
    return origin(pos, face, hand, simTick); \
} \
LL_TYPE_INSTANCE_HOOK(PlacementAssist##Mode##StartBuild, ll::memory::HookPriority::Highest, Mode, \
    &Mode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) { \
    bool const taken = hand == HandSlot::Mainhand && takePress(mPlayer); \
    ASSIST_TRACE(#Mode " startBuildBlock", taken); \
    if (taken) { holding = true; return; } \
    origin(pos, face, hand); \
}
BUILD_HOOKS(GameMode)
BUILD_HOOKS(SurvivalMode)
#undef BUILD_HOOKS
// A press that meets no block (a ghost with air behind) reaches only the
// item use (L-265 fifth trace). The game's hit may still be a block far
// past reach, with no outline (trace 2026-11-28): that is no block either.
LL_TYPE_INSTANCE_HOOK(PlacementAssistUseItem, ll::memory::HookPriority::Highest, GameMode, &GameMode::$useItem, bool,
                      ItemStack& item, HandSlot hand) {
    try {
        auto client = ll::service::getClientInstance();
        bool const noBlock = client && hand == HandSlot::Mainhand && meetsNoBlock(client->getLatestHitResult(), mPlayer);
        if (!noBlock) declined = "hit within reach";
        bool const taken = noBlock && takePress(mPlayer);
        ASSIST_TRACE(hand == HandSlot::Mainhand ? "useItem" : "useItem offhand", taken);
        if (taken) {
            holding = true;
            return false;
        }
    } catch (...) {}
    return origin(item, hand);
}
// The button let go: the press and what it refused end with it.
LL_TYPE_INSTANCE_HOOK(PlacementAssistStopBuild, ll::memory::HookPriority::Highest, GameMode, &GameMode::$stopBuildBlock, void) {
    try {
        auto* player = localPlayer();
        if (player && static_cast<Player*>(player) == &mPlayer) {
            ASSIST_TRACE("stopBuildBlock (released)", false);
            holding = false;
            pressTaken.reset();
            // The refusal stays for refusalKept: the game calls this every
            // tick while nothing is built, so clearing it here toasted and
            // logged a refusal every 0.2 s while held (2026-11-28 log).
        }
    } catch (...) {}
    origin();
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {PlacementAssistSend::hook, PlacementAssistSend::unhook},
    {PlacementAssistGameModeBuild::hook, PlacementAssistGameModeBuild::unhook},
    {PlacementAssistGameModeStartBuild::hook, PlacementAssistGameModeStartBuild::unhook},
    {PlacementAssistSurvivalModeBuild::hook, PlacementAssistSurvivalModeBuild::unhook},
    {PlacementAssistSurvivalModeStartBuild::hook, PlacementAssistSurvivalModeStartBuild::unhook},
    {PlacementAssistUseItem::hook, PlacementAssistUseItem::unhook},
    {PlacementAssistStopBuild::hook, PlacementAssistStopBuild::unhook},
};
void reset() {
    {
        std::lock_guard lock(aimLock);
        aimNow.reset();
    }
    holding = false;
    pressTaken.reset();
    pending.reset();
    sent.clear();
    refused.reset();
    lastSentLook.reset();
    placing = false;
}
}
bool aimed(Aim& out) {
    std::lock_guard lock(aimLock);
    if (!aimNow) return false;
    out = *aimNow;
    return true;
}
void focusLost() {
    pending.reset();
    holding = false;
}
void start() {
    if (installed) return;
    capabilities::begin(capabilities::Id::PlacementAssist);
    try {
        for (auto& hook : hooks)
            if (!hook.installed) {
                if (hook.install(true) != 0) throw std::runtime_error("Could not install placement assist hooks");
                hook.installed = true;
            }
        installed = true;
        exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
        capabilities::ready(capabilities::Id::PlacementAssist);
    } catch (...) {
        capabilities::hooks(capabilities::Id::PlacementAssist, false);
        stop();
        throw;
    }
}
void stop() {
    capabilities::stop(capabilities::Id::PlacementAssist);
    if (exitListener) ll::event::EventBus::getInstance().removeListener(exitListener);
    exitListener.reset();
    reset();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    installed = false;
}
}
