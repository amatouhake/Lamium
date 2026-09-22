-- Consume the published client runtime's exports and matching source headers.
-- Building the runtime itself is unnecessary for a mod and fails for this tag
-- with current MSVC headers (MinecraftCommands deletes incomplete CommandRegistry).
package("levilamina-client-sdk")
    set_homepage("https://github.com/LiteLDev/LeviLamina")
    set_description("LeviLamina client headers and release import library")
    set_license("LGPL-3.0")
    add_urls("https://github.com/LiteLDev/LeviLamina/archive/refs/tags/v$(version).zip")
    add_versions("26.51.3", "75de717a3e87e38aff05daa61016b9cc7708e60e7ec26a0e0c7a40a4547b175f")
    add_resources("26.51.3", "runtime",
        "https://github.com/LiteLDev/LeviLamina/releases/download/v26.51.3/levilamina-v26.51.3-client-release-windows-x64.zip",
        "ab92c0e77235a7cdecfdb015e4dc04e210552b09ec6ca993799ac8b82bc2bef6")
    add_defines("LL_PLAT_C", "ENTT_PACKED_PAGE=128", "ENTT_SPARSE_PAGE=2048", "ENTT_NO_MIXIN")
    add_links("LeviLamina")
    add_deps("entt v4.0.0", "expected-lite v0.8.0", "fmt 11.2.0", "gsl v4.2.0",
        "glm 1.0.1", "leveldb 1.23", "magic_enum v0.9.7", "nlohmann_json v3.12.0",
        "rapidjson 2025.02.05", "type_safe v0.2.4", "pcg_cpp v1.0.0", "pfr 2.1.1",
        "symbolprovider v1.3.0", "parallel-hashmap v1.3.12", "concurrentqueue v1.0.4",
        "stb 2025.03.14", "bedrockdata v26.51.1-client.4")

    on_install("windows|x64", function(package)
        import("core.tool.toolchain")
        import("lib.detect.find_tool")
        local msvc = toolchain.load("msvc", {plat = package:plat(), arch = package:arch()})
        assert(msvc:check(), "Visual Studio C++ tools are required")
        local envs = msvc:runenvs()
        local dumpbin = assert(find_tool("dumpbin", {envs = envs}), "dumpbin not found")
        local librarian = assert(find_tool("lib", {envs = envs}), "MSVC lib not found")
        local dll = path.join(package:resourcedir("runtime"), "LeviLamina", "LeviLamina.dll")
        local exports = os.iorunv(dumpbin.program, {"/nologo", "/exports", dll}, {envs = envs})
        local definitions = {"LIBRARY LeviLamina.dll", "EXPORTS"}
        for line in exports:gmatch("[^\r\n]+") do
            local symbol = line:match("^%s*%d+%s+%x+%s+%x+%s+(%S+)")
            if symbol then table.insert(definitions, "    " .. symbol) end
        end
        assert(#definitions > 2, "No exports found in the LeviLamina client runtime")
        io.writefile("LeviLamina.def", table.concat(definitions, "\n") .. "\n")
        os.vrunv(librarian.program, {"/nologo", "/machine:x64", "/def:LeviLamina.def",
            "/out:LeviLamina.lib"}, {envs = envs})
        os.cp("LeviLamina.lib", package:installdir("lib"))
        for _, root in ipairs({"src", "src-client"}) do
            for _, pattern in ipairs({"ll/api/**.h", "mc/**.h"}) do
                for _, header in ipairs(os.files(path.join(root, pattern))) do
                    local destination = path.join(package:installdir("include"), path.relative(header, root))
                    os.mkdir(path.directory(destination))
                    os.cp(header, destination)
                end
            end
        end
        assert(os.isfile(path.join(package:installdir("include"), "ll/api/memory/MemoryOperators.h")),
            "SDK API headers were not installed")
        os.cp("COPYING", package:installdir("share", "licenses"))
        os.cp("COPYING.LESSER", package:installdir("share", "licenses"))
    end)
package_end()
