add_rules("mode.debug", "mode.release")
set_license("LGPL-3.0")
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

option("camera_position_probe")
    set_default(false)
    set_showmenu(true)
    set_description("Experimental render-only camera translation while Zoom is held")
option_end()

option("hud_fill_probe")
    set_default(false)
    set_showmenu(true)
    set_description("Draw numbered translucent test squares in the HUD")
option_end()

includes("packages/levilamina-client-sdk.lua")
add_requires("levilamina-client-sdk 26.51.5", {configs = {shared = true}})

add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("Lamium")
    if has_config("automation_trace") then add_defines("LAMIUM_AUTOMATION_TRACE") end
    if has_config("restock_trace") then add_defines("LAMIUM_RESTOCK_TRACE") end
    if has_config("camera_trace") or has_config("camera_probe") or has_config("camera_position_probe") then add_defines("LAMIUM_CAMERA_TRACE") end
    if has_config("camera_probe") then add_defines("LAMIUM_CAMERA_PROBE") end
    if has_config("camera_position_probe") then add_defines("LAMIUM_CAMERA_POSITION_PROBE") end
    if has_config("shape_trace") then add_defines("LAMIUM_SHAPE_TRACE") end
    if has_config("hud_fill_probe") then add_defines("LAMIUM_HUD_FILL_PROBE") end
    if has_config("placement_trace") then add_defines("LAMIUM_PLACEMENT_TRACE") end
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
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

