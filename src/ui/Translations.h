#pragma once
#include <array>
#include <string_view>

namespace lamium::ui::translations {
struct Entry { std::string_view key, english, japanese; };
inline constexpr auto entries = std::to_array<Entry>({
    {"title", "Lamium / Settings", "Lamium / 設定"},
    {"subtitle", "Camera, lighting, items and inventory", "視点・明るさ・アイテム・インベントリ"},
    {"on", "On", "オン"}, {"off", "Off", "オフ"},
    {"zoom", "Zoom: {}", "ズーム: {}"},
    {"magnification", "Magnification: {:.1f}x", "倍率: {:.1f}倍"},
    {"wheelStep", "Wheel step: {:.1f}", "ホイールの調整幅: {:.1f}"},
    {"nightVision", "NightVision: {}", "暗視: {}"},
    {"previews", "Container previews: {}", "収納アイテムのプレビュー: {}"},
    {"durability", "Durability: {}", "耐久値の表示: {}"},
    {"durabilityValue", "Durability: {} / {}", "耐久値: {} / {}"},
    {"sorting", "Inventory sorting: {}", "インベントリの整頓: {}"},
    {"storage", "Sort storage containers: {}", "チェストなどの整頓: {}"},
    {"gameplayHints", "Gameplay key hints: {}", "プレイ中のキー案内: {}"},
    {"close", "Close", "閉じる"},
    {"saveError", "Save failed; change not applied. Try again.", "保存失敗・変更は未反映です。再試行してください。"},
    {"smallWindow", "Enlarge window | Esc: close", "画面を広げてください | Esc: 閉じる"},
    {"navigation", "Up/Down / wheel: navigate | Enter | Esc", "上下・ホイール: 選択 | Enter: 決定 | Esc: 戻る"},
    {"adjustment", "Left/Right: adjust | Click: toggle | Right click: decrease", "左右: 調整 | 左クリック: 切替 | 右: 減らす"},
    {"controls", "Lamium | Configure controls in Keyboard & Mouse settings", "Lamium | キーボードとマウスの設定でキーを変更できます"},
    {"unbound", "Unbound", "未割り当て"},
    {"gameplay", "Lamium | {}: settings | Hold {}: zoom | {}: NightVision", "Lamium | {}: 設定 | {} 長押し: ズーム | {}: 暗視"},
    {"key.Lamium.sort", "Lamium: Sort inventory", "Lamium: インベントリの整頓"},
    {"key.Lamium.nightvision", "Lamium: Toggle NightVision", "Lamium: 暗視の切替"},
    {"key.Lamium.settings", "Lamium: Open settings", "Lamium: 設定を開く"},
    {"key.Lamium.zoom", "Lamium: Hold to zoom", "Lamium: 長押しでズーム"},
});
inline constexpr bool japanese(std::string_view locale) {
    return locale == "ja" || locale.starts_with("ja_") || locale.starts_with("ja-");
}
inline constexpr std::string_view find(std::string_view key, std::string_view locale) {
    for (auto const& entry : entries)
        if (entry.key == key) return japanese(locale) ? entry.japanese : entry.english;
    return {};
}
}
