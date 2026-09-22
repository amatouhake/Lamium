#include "overlay/ShapeStore.h"
#include "overlay/ShapeWorkspace.h"
#include "overlay/LocalShapePath.h"
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
    if (!std::filesystem::equivalent(root.parent_path(),std::filesystem::temp_directory_path()))
        throw std::runtime_error("Test cleanup path escaped temporary directory");
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("Test directory already exists");
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root,ignored); } } cleanup{root};
    auto profiles = root / "profiles";
    for (auto profile : {"a","b"}) {
        auto world = profiles / profile / "worlds" / "same-id";
        std::filesystem::create_directories(world);
        std::ofstream marker(world / "level.dat"); marker << "test marker";
    }
    auto scopedA=localShapePath(profiles / "a" / "worlds","same-id");
    auto scopedB=localShapePath(profiles / "b" / "worlds","same-id");
    check(scopedA && scopedB && *scopedA != *scopedB,"same level ID remains separated by profile storage root");
    check(scopedA->filename()=="shapes.json" && scopedA->parent_path().filename()=="lamium","shape sidecar stays in dedicated world directory");
    for (auto id : {"", ".", "..", "../same-id", "same-id/child", "same-id\\child", "C:world", "world.", "world ", "missing"})
        check(!localShapePath(profiles / "a" / "worlds",id),"invalid or missing local world is not a save target");
    check(!localShapePath("relative-worlds","same-id"),"relative storage roots cannot select a world");
    std::filesystem::create_directory(profiles / "a" / "worlds" / "not-a-world");
    check(!localShapePath(profiles / "a" / "worlds","not-a-world"),"arbitrary directories are not local worlds");
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

    ShapeWorkspace workspace;
    auto worldA = root / "world-a" / "shapes.json";
    auto worldB = root / "world-b" / "shapes.json";
    workspace.enter(worldA);
    check(workspace.persistent() && workspace.collection().entries().empty(),"new world begins empty and permits saving");
    ShapeDefinition original{"World A sphere",0,true,ShapeSpec{Shape::Sphere,{2,4,6},Snap::BlockCenter,1,1}};
    auto first = workspace.change([&](auto& collection) { return collection.add(original); });
    check(readShapes(worldA)[0].name == original.name,"workspace creation persists without a separate save action");
    locked = CreateFileW(worldA.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked != INVALID_HANDLE_VALUE,"workspace test blocks replacement");
    failed=false;
    try { workspace.change([&](auto& collection) { collection.rename(first,"Unsaved"); }); }
    catch (ShapeSaveError const&) { failed=true; }
    CloseHandle(locked);
    check(failed && workspace.collection().find(first)->definition.name == original.name
        && readShapes(worldA)[0].name == original.name,"failed workspace edit preserves live and saved values");
    workspace.change([&](auto& collection) { collection.setVisible(first,false); });
    check(!readShapes(worldA)[0].visible,"visibility persists after a failed save is retried");
    workspace.enter(worldB);
    check(workspace.collection().entries().empty(),"switching to another world clears old shapes");
    original.name="World B sphere";
    auto second=workspace.change([&](auto& collection) { return collection.add(original); });
    check(second != first,"world transitions do not reuse stale editor IDs");
    workspace.enter(worldA);
    auto restored=workspace.collection().entries().begin()->first;
    check(restored != first && restored != second && !workspace.collection().find(first)
        && workspace.collection().find(restored)->definition.name == "World A sphere"
        && !workspace.collection().find(restored)->definition.visible,"reentry restores only the chosen world's shapes with fresh IDs");
    auto corrupt=root / "broken.json";
    { std::ofstream file(corrupt); file << "broken shape document"; }
    failed=false;
    try { workspace.enter(corrupt); } catch (std::exception const&) { failed=true; }
    check(failed && workspace.failedToLoad() && !workspace.persistent() && workspace.collection().entries().empty(),
        "failed world load clears previous rendering and disables persistence");
    failed=false;
    try { workspace.change([&](auto& collection) { return collection.add(original); }); }
    catch (std::exception const&) { failed=true; }
    std::ifstream unchanged(corrupt);
    std::string contents((std::istreambuf_iterator<char>(unchanged)),{});
    check(failed && workspace.collection().entries().empty() && contents=="broken shape document",
        "editing cannot overwrite an unreadable workspace with an empty replacement");
    workspace.enter(worldB);
    auto removeId=workspace.collection().entries().begin()->first;
    workspace.change([&](auto& collection) { return collection.remove(removeId); });
    check(readShapes(worldB).empty(),"removing the final shape saves an empty collection");
    workspace.leave();
    check(!workspace.persistent() && !workspace.failedToLoad() && workspace.collection().entries().empty(),"leave clears binding and transient errors");
    workspace.change([&](auto& collection) { return collection.add(original); });
    check(readShapes(worldB).empty(),"unbound session changes cannot write to a departed world");
}
