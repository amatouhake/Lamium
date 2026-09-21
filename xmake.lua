add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- Lamium is a client-only mod. The option is kept because LeviBuildScript's
-- link/pack rules read it, but "client" is the only accepted value.
option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

-- The "v" form checks out the upstream git tag directly; the LeviMC xmake-repo
-- had not published a 26.51.3 version entry when this was written. Switch to
-- "levilamina 26.51.3" once it has one.
add_requires("levilamina v26.51.3", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("Lamium")
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
    add_packages("levilamina", "nlohmann_json")
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
    add_files("src/features/inventory/sort/**.cpp")
    add_packages("nlohmann_json")
    if is_plat("windows") then
        add_cxflags("/utf-8", "/W4")
        set_toolchains("clang-cl")
    end

