#pragma once
#include <array>
#include <string_view>

namespace lamium::ui::translations {
struct Entry { std::string_view key, english, japanese; };
inline constexpr auto entries = std::to_array<Entry>({
    {"title", "Lamium / Settings", "Lamium / 設定"},
    {"search", "Search: {}", "検索: {}"},
    {"noResults", "No matching settings", "該当する設定はありません"},
    {"featuresView", "Features  |  Switch to Hotkeys", "機能  |  キー一覧に切り替え"},
    {"hotkeysView", "Hotkeys  |  Switch to Features", "キー一覧  |  機能に切り替え"},
    {"bindingRow", "{}: {}", "{}: {}"},
    {"capturing", "Input: {}", "入力: {}"},
    {"clearBinding", "Clear (Unbound)", "割り当てを解除"},
    {"resetBinding", "Reset to Minecraft mapping", "Minecraft の割り当てに戻す"},
    {"cancelBinding", "Cancel binding edit", "キー編集をキャンセル"},
    {"captureHint", "Press chord, release to set | Esc: cancel", "キーを押して離すと設定 | Esc: キャンセル"},
    {"invalidBinding", "Unsupported binding for this action", "この操作には割り当てられない入力です"},
    {"sharedBinding", "[Shared] {}", "[重複] {}"},
    {"feature.zoom", "Zoom", "ズーム"},
    {"feature.chunkBorders", "Chunk Borders", "チャンク境界"},
    {"chunkBorders", "Chunk Borders: {}", "チャンク境界: {}"},
    {"help.chunkBorders", "Show current chunk edges and 16-block layers.", "現在のチャンクの境界と16ブロックごとの高さを表示。"},
    {"feature.nightVision", "NightVision", "暗視"},
    {"feature.previews", "Container previews", "収納アイテムのプレビュー"},
    {"feature.durability", "Durability", "耐久値"},
    {"feature.sorting", "Inventory sorting", "インベントリの整頓"},
    {"feature.gameplayHints", "Gameplay key hints", "プレイ中のキー案内"},
    {"feature.settings", "Settings screen", "設定画面"},
    {"help.zoom", "Hold to zoom; use the wheel to adjust.", "長押しでズーム。ホイールで倍率を調整。"},
    {"help.nightVision", "Brighten the view locally; no status effect.", "視界を明るくします。状態効果は付与しません。"},
    {"help.previews", "Inspect Shulker or Bundle contents on hover.", "カーソルを重ねてシュルカーやバンドルの中身を表示。"},
    {"help.durability", "Show remaining durability in item tooltips.", "アイテムのツールチップに残り耐久値を表示。"},
    {"help.sorting", "Merge and sort inventory or container slots.", "インベントリや収納内をまとめて整頓。"},
    {"help.gameplayHints", "Show or hide the gameplay key guide.", "プレイ中のキー案内の表示を切り替え。"},
    {"help.settings", "Choose the key that opens this screen.", "この画面を開くキーを設定。"},
    {"subtitle", "Camera, lighting, items and inventory", "視点・明るさ・アイテム・インベントリ"},
    {"on", "On", "オン"}, {"off", "Off", "オフ"},
    {"zoom", "Zoom: {}", "ズーム: {}"},
    {"magnification", "Magnification: {:.1f}x", "倍率: {:.1f}倍"},
    {"wheelStep", "Wheel step: {:.1f}", "ホイールの調整幅: {:.1f}"},
    {"nightVision", "NightVision: {}", "暗視: {}"},
    {"previews", "Container previews: {}", "収納アイテムのプレビュー: {}"},
    {"shulkerPreviews", "Shulker previews: {}", "シュルカーのプレビュー: {}"},
    {"emptyShulkerPreviews", "Show empty Shulkers: {}", "空のシュルカーを表示: {}"},
    {"hideShulkerContents", "Hide Shulker contents text: {}", "シュルカーの内容テキストを隠す: {}"},
    {"bundlePreviews", "Bundle previews: {}", "バンドルのプレビュー: {}"},
    {"emptyBundlePreviews", "Show empty Bundles: {}", "空のバンドルを表示: {}"},
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
    {"wheelUp", "Wheel up", "ホイール上"},
    {"wheelDown", "Wheel down", "ホイール下"},
    {"mouseButton", "Mouse {}", "マウス {}"},
    {"gameplay", "Lamium | {}: settings | Hold {}: zoom | {}: NightVision", "Lamium | {}: 設定 | {} 長押し: ズーム | {}: 暗視"},
    {"key.Lamium.sort", "Lamium: Sort inventory", "Lamium: インベントリの整頓"},
    {"key.Lamium.chunkborders", "Lamium: Toggle chunk borders", "Lamium: チャンク境界の切り替え"},
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
