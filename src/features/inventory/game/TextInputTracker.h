#pragma once

#include <map>
#include <set>

class ScreenView;
class TextEditComponent;

namespace lamium::inventory::game {

/// Knows whether a screen currently has a text box selected for typing, so
/// the Sort key can stay quiet while the player types an `r` into the
/// Creative search field or an anvil name.
///
/// The information comes from the game's own text-edit selection event
/// (`ScreenView::_fireSelectedStateChangeEvent`), tracked per ScreenView.
/// All members run on the client's main thread.
class TextInputTracker {
public:
    static TextInputTracker& getInstance();

    void install();
    void uninstall();

    /// True while at least one text box of `view` is selected for editing.
    [[nodiscard]] bool isEditing(ScreenView const* view) const;

    /// Called from the hook.
    void onSelectedStateChanged(ScreenView const& view, TextEditComponent const& component, bool selected);

    /// Forgets everything known about `view` (it was closed or just opened).
    void forget(ScreenView const* view);

private:
    std::map<ScreenView const*, std::set<TextEditComponent const*>> mSelected;
    bool                                                            mInstalled{false};
};

} // namespace lamium::inventory::game
