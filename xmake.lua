add_rules("mode.debug", "mode.release")
set_license("LGPL-3.0")

-- Single source for the manifest version and the string the Debug View shows.
local lamiumVersion = "0.1.8"
set_policy("package.requires_lock", true)

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- Lamium is a client-only mod. The option is kept because LeviBuildScript's
-- link/pack rules read it, but "client" is the only accepted value.
option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

option("restock_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded HUD mapping and restock use diagnostics")
option_end()

option("automation_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Observe bounded vanilla button registration and dispatch diagnostics")
option_end()

option("research_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded diagnostics for research items L-36, L-37, L-40, L-44, L-49 and L-58")
option_end()

option("icon_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded diagnostics for L-91/L-119 (vanilla slot icon routes vs Lamium's icon calls)")
option_end()

option("playerlist_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable L-131 diagnostics: what the client knows about each listed player (permission, locator, loaded)")
option_end()

option("ctm_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable the L-96 spike: trim glass borders on connected sides from the block tessellator, with a log")
option_end()

option("inventorymove_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable L-132 trial: feed the movement keys while the inventory screen is open, with a log")
option_end()

option("trade_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable L-129 diagnostics: dump the trade data the server sends when a trade screen opens")
option_end()

option("hunger_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Log hunger/saturation and the vanilla hunger bar position for the L-63 research")
option_end()

option("consumption_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Observe use/consumption callbacks and held-slot counts for the L-66 trigger spike")
option_end()

option("offhand_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Observe item-use and selection lifecycles for L-95")
option_end()

option("transfer_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Observe Inventory Transfer input, queue and requests")
option_end()

option("placement_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded local placement diagnostics")
option_end()

option("shape_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded local shape world-identity diagnostics")
option_end()

option("effects_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Observe bounded UI renderer routes and immersion fog selection for L-42")
option_end()

option("camera_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Enable bounded read-only render camera diagnostics")
option_end()

option("camera_probe")
    set_default(false)
    set_showmenu(true)
    set_description("Experimental render-only rotation while Zoom is held (enables camera trace)")
option_end()

option("radar_icon_probe")
    set_default(false)
    set_showmenu(true)
    set_description("L-85 research: log each radar face built and write them all to logs/radar-faces.bmp")
option_end()

option("schematic_perf_trace")
    set_default(false)
    set_showmenu(true)
    set_description("Schematic C: log ghost frame time, section rebuilds and their reasons every 5 s")
option_end()

option("schematic_model_trace")
    set_default(false)
    set_showmenu(true)
    set_description("L-115: log how each schematic entity model is chosen and posed")
option_end()

option("ghost_probe")
    set_default(false)
    set_showmenu(true)
    set_description("L-93 research: F7 draws test blocks with candidate ghost render paths")
option_end()

option("camera_position_probe")
    set_default(false)
    set_showmenu(true)
    set_description("Experimental render-only camera translation while Zoom is held")
option_end()

includes("packages/levilamina-client-sdk.lua")
add_requires("levilamina-client-sdk 26.51.5", {configs = {shared = true}})

add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("Lamium")
    if has_config("effects_trace") then add_defines("LAMIUM_EFFECTS_TRACE") end
    if has_config("automation_trace") then add_defines("LAMIUM_AUTOMATION_TRACE") end
    if has_config("restock_trace") then add_defines("LAMIUM_RESTOCK_TRACE") end
    if has_config("camera_trace") or has_config("camera_probe") or has_config("camera_position_probe") then add_defines("LAMIUM_CAMERA_TRACE") end
    if has_config("camera_probe") then add_defines("LAMIUM_CAMERA_PROBE") end
    if has_config("camera_position_probe") then add_defines("LAMIUM_CAMERA_POSITION_PROBE") end
    if has_config("shape_trace") then add_defines("LAMIUM_SHAPE_TRACE") end
    if has_config("radar_icon_probe") then add_defines("LAMIUM_RADAR_ICON_PROBE") end
    if has_config("placement_trace") then add_defines("LAMIUM_PLACEMENT_TRACE") end
    if has_config("research_trace") then add_defines("LAMIUM_RESEARCH_TRACE") end
    if has_config("ghost_probe") then add_defines("LAMIUM_GHOST_PROBE") end
    if has_config("schematic_model_trace") then add_defines("LAMIUM_SCHEMATIC_MODEL_TRACE") end
    if has_config("schematic_perf_trace") then add_defines("LAMIUM_SCHEMATIC_PERF_TRACE") end
    if has_config("consumption_trace") then add_defines("LAMIUM_CONSUMPTION_TRACE") end
    if has_config("offhand_trace") then add_defines("LAMIUM_OFFHAND_TRACE") end
    if has_config("transfer_trace") then add_defines("LAMIUM_TRANSFER_TRACE") end
    if has_config("hunger_trace") then add_defines("LAMIUM_HUNGER_TRACE") end
    if has_config("icon_trace") then add_defines("LAMIUM_ICON_TRACE") end
    if has_config("trade_trace") then add_defines("LAMIUM_TRADE_TRACE") end
    if has_config("playerlist_trace") then add_defines("LAMIUM_PLAYERLIST_TRACE") end
    if has_config("inventorymove_trace") then add_defines("LAMIUM_INVENTORYMOVE_TRACE") end
    if has_config("ctm_trace") then add_defines("LAMIUM_CTM_TRACE") end
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker", {modVersion = lamiumVersion})
    add_defines('LAMIUM_VERSION="' .. lamiumVersion .. '"')
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
        -- SystemInfo reads the registry and display adapter for Debug View.
        add_syslinks("advapi32", "user32", "shell32")
    end
    add_packages("levilamina-client-sdk", "nlohmann_json")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    after_build(function (target)
        local destination = path.join(os.projectdir(), "bin", target:name())
        os.mkdir(destination)
        os.cp("COPYING", destination)
        os.cp("COPYING.LESSER", destination)
        os.cp("THIRD_PARTY_NOTICES.md", destination)
        os.cp("licenses", destination)
    end)

-- Unit tests for the pure (game-independent) view math/state. Not built by
-- default: `xmake build LamiumTests && xmake run LamiumTests`.
target("LamiumTests")
    set_kind("binary")
    set_default(false)
    set_languages("c++20")
    add_includedirs("src")
    add_files("tests/**.cpp")
    add_files("src/settings/SettingsStore.cpp")
    add_files("src/overlay/ShapeDocument.cpp")
    add_files("src/overlay/ShapeStore.cpp")
    add_files("src/app/AtomicFile.cpp")
    add_files("src/features/map/WaypointStore.cpp")
    add_files("src/features/inventory/DeathLayoutStore.cpp")
    add_files("src/features/schematic/Nbt.cpp", "src/features/schematic/Structure.cpp", "src/features/schematic/PlacementStore.cpp")
    add_files("src/features/inventory/sort/**.cpp")
    add_packages("nlohmann_json")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/W4")
        set_toolchains("clang-cl")
    end

-- SDK data-layout checks that do not launch Minecraft or call engine symbols.
target("LamiumNativeTests")
    set_kind("binary")
    set_default(false)
    set_languages("c++20")
    add_includedirs("src")
    add_packages("levilamina-client-sdk")
    add_files("tests-native/**.cpp", "src/features/camera/CameraMovementInput.cpp")
    add_defines("NOMINMAX", "UNICODE")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/W4")
        set_toolchains("clang-cl")
    end

