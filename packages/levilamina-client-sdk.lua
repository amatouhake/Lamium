-- Consume the published client runtime's exports and matching source headers.
-- Building the runtime itself is unnecessary for a mod and fails for this tag
-- with current MSVC headers (MinecraftCommands deletes incomplete CommandRegistry).
package("levilamina-client-sdk")
    set_homepage("https://github.com/LiteLDev/LeviLamina")
    set_description("LeviLamina client headers and release import library")
    set_license("LGPL-3.0")
    add_urls("https://github.com/LiteLDev/LeviLamina/archive/refs/tags/v$(version).zip")
    add_versions("26.51.5", "f1869b522e9d55cb367bbc799cc68f44e05be0e26c11bc3152525680035fa67d")
    add_resources("26.51.5", "runtime",
        "https://github.com/LiteLDev/LeviLamina/releases/download/v26.51.5/levilamina-v26.51.5-client-release-windows-x64.zip",
        "c88649827a33a81ab3e154a6fdd7ee5bc38d23e78730fc34798bfa57cdca0a5c")
    add_defines("LL_PLAT_C", "ENTT_PACKED_PAGE=128", "ENTT_SPARSE_PAGE=2048", "ENTT_NO_MIXIN")
    add_links("LeviLamina")
    add_deps("entt v4.0.0", "expected-lite v0.8.0", "fmt 11.2.0", "gsl v4.2.0",
        "glm 1.0.1", "leveldb 1.23", "magic_enum v0.9.7", "nlohmann_json v3.12.0",
        "rapidjson 2025.02.05", "type_safe v0.2.4", "pcg_cpp v1.0.0", "pfr 2.1.1",
        "symbolprovider v1.3.0", "parallel-hashmap v1.3.12", "concurrentqueue v1.0.4",
        "stb 2025.03.14", "bedrockdata v26.51.1-client.6")

    on_fetch(function(package, opt)
        if opt.system then return false end
        local result = package:find_package("xmake::" .. package:name(), {
            require_version = package:version_str(), external = opt.external, force = opt.force
        })
        if result then
            -- This .lib contains DLL import records, not runtime implementation.
            -- xmake's Windows scanner otherwise requires a bundled DLL to infer
            -- shared linkage. Keep the runtime outside the SDK installation.
            result.shared = true
            result.static = nil
        end
        return result
    end)

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
