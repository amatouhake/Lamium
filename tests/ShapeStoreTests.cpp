#include "overlay/ShapeStore.h"
#include <chrono>
#include <fstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
void check(bool,char const*);
void shapeStoreTests() {
    using namespace lamium::overlay;
    auto root = std::filesystem::temp_directory_path() / ("lamium-shapes-test-"
        + std::to_string(GetCurrentProcessId()) + "-"
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    root = std::filesystem::absolute(root).lexically_normal();
    if (root.parent_path() != std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal())
        throw std::runtime_error("Test cleanup path escaped temporary directory");
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("Test directory already exists");
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root,ignored); } } cleanup{root};
    auto path = root / "nested" / "shapes.json";
    std::vector<ShapeDefinition> definitions{{"保存テスト",0,true,ShapeSpec{Shape::Sphere,{0,0,0},Snap::BlockCenter,1,1}}};
    writeShapes(path,definitions);
    check(encodeShapes(readShapes(path)) == encodeShapes(definitions),"shape file creates parents and round trips");
    definitions[0].name="Updated";
    writeShapes(path,definitions);
    check(readShapes(path)[0].name=="Updated","shape file replacement updates complete document");
    auto locked = CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked != INVALID_HANDLE_VALUE,"test acquires replacement-blocking handle");
    definitions[0].name="Must not replace";
    bool failed=false;
    try { writeShapes(path,definitions); } catch (std::exception const&) { failed=true; }
    CloseHandle(locked);
    check(failed && readShapes(path)[0].name=="Updated","failed replacement preserves previous shape file");
    size_t files=0;
    for (auto const& entry : std::filesystem::directory_iterator(path.parent_path())) { (void)entry; ++files; }
    check(files==1,"failed replacement removes only its own temporary file");
    definitions[0].name.clear();
    failed=false;
    try { writeShapes(path,definitions); } catch (std::exception const&) { failed=true; }
    check(failed && readShapes(path)[0].name=="Updated","invalid geometry metadata cannot overwrite a valid document");
    auto oversized = root / "oversized.json";
    { std::ofstream file(oversized,std::ios::binary); file << std::string(1024*1024+1,' '); }
    failed=false;
    try { (void)readShapes(oversized); } catch (std::length_error const&) { failed=true; }
    check(failed,"shape file reads enforce bounded document size");
}
