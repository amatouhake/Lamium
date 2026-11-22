#include "app/Desktop.h"
#include <cstring>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

namespace lamium {
namespace {
std::wstring wide(std::string const& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}
}
bool openUrl(std::string const& url) {
    // Only web links: never hand anything else to the shell.
    if (!url.starts_with("https://")) return false;
    auto result = ShellExecuteW(nullptr, L"open", wide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}
bool copyText(std::string const& text) {
    auto value = wide(text);
    if (!OpenClipboard(nullptr)) return false;
    bool done = false;
    if (EmptyClipboard()) {
        size_t bytes = (value.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* target = GlobalLock(memory)) {
                std::memcpy(target, value.c_str(), bytes);
                GlobalUnlock(memory);
                done = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!done) GlobalFree(memory);
        }
    }
    CloseClipboard();
    return done;
}
std::string clipboardText() {
    if (!OpenClipboard(nullptr)) return {};
    std::string out;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (auto const* text = static_cast<wchar_t const*>(GlobalLock(data))) {
            int length = static_cast<int>(wcslen(text));
            int size = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
            out.resize(static_cast<size_t>(size));
            WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), size, nullptr, nullptr);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return out;
}
bool keyDown(int key) { return key > 0 && key < 256 && (GetAsyncKeyState(key) & 0x8000) != 0; }
std::string keyName(int key) {
    if (key <= 0 || key >= 256) return {};
    // The extended form (0xE0 in the high byte) tells the arrows, Insert, Delete and the like from their keypad
    // twins; GetKeyNameText takes that as bit 24.
    UINT const scan = MapVirtualKeyW(static_cast<UINT>(key), MAPVK_VK_TO_VSC_EX);
    if ((scan & 0xFF) == 0) return {};
    LONG const lparam = static_cast<LONG>(((scan & 0xFF) << 16) | ((scan & 0xFF00) == 0xE000 ? 1u << 24 : 0u));
    wchar_t name[64]{};
    int const length = GetKeyNameTextW(lparam, name, 64);
    if (length <= 0) return {};
    std::string out(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, name, length, nullptr, 0, nullptr, nullptr)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, length, out.data(), static_cast<int>(out.size()), nullptr, nullptr);
    return out;
}
}
