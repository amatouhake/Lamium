#include "overlay/ShapeStore.h"
#include <atomic>
#include <fstream>
#include <system_error>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace lamium::overlay {
std::vector<ShapeDefinition> readShapes(std::filesystem::path const& path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("Could not open shape document");
    std::string text(1024*1024+1,'\0');
    file.read(text.data(),static_cast<std::streamsize>(text.size()));
    if (file.bad()) throw std::runtime_error("Could not read shape document");
    text.resize(static_cast<size_t>(file.gcount()));
    return decodeShapes(text);
}
void writeShapes(std::filesystem::path const& path, std::vector<ShapeDefinition> const& definitions) {
    auto text = encodeShapes(definitions);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    static std::atomic<uint64_t> sequence{0};
    std::filesystem::path temporary;
    HANDLE handle = INVALID_HANDLE_VALUE;
    auto fail = [](char const* action) {
        throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),action);
    };
    for (int attempt=0;attempt<128;++attempt) {
        temporary = path;
        temporary += "." + std::to_string(GetCurrentProcessId()) + "." + std::to_string(sequence++) + ".tmp";
        handle = CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (handle != INVALID_HANDLE_VALUE) break;
        if (GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) fail("Creating shape temporary file");
    }
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not reserve a shape temporary file");
    try {
        DWORD written=0;
        if (!WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)) fail("Writing shapes");
        if (written != text.size()) throw std::runtime_error("Incomplete shape write");
        if (!FlushFileBuffers(handle)) fail("Flushing shapes");
        auto closing = handle;
        handle = INVALID_HANDLE_VALUE;
        if (!CloseHandle(closing)) fail("Closing shape temporary file");
        if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            fail("Replacing shape document");
    } catch (...) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        std::error_code ignored;
        std::filesystem::remove(temporary,ignored);
        throw;
    }
}
}
