#pragma once
#include "overlay/ShapeStore.h"
#include <optional>
#include <utility>

namespace lamium::overlay {
// The caller supplies an already resolved world-specific path. Display names,
// dimension numbers and temporary connection IDs are not world identities.
// No game pointers or file I/O belong in the rendering callback.
class ShapeWorkspace {
    ShapeCollection shapes;
    std::optional<std::filesystem::path> destination;
    bool loadFailed = false;

    static std::vector<ShapeDefinition> definitions(ShapeCollection const& collection) {
        std::vector<ShapeDefinition> result;
        result.reserve(collection.entries().size());
        for (auto const& [id, shape] : collection.entries()) {
            (void)id;
            result.push_back(shape.definition);
        }
        return result;
    }
public:
    ShapeCollection const& collection() const { return shapes; }
    bool persistent() const { return destination.has_value(); }
    bool failedToLoad() const { return loadFailed; }

    void leave() {
        shapes.clear();
        destination.reset();
        loadFailed = false;
    }
    void enter(std::filesystem::path path) {
        // Clear the old world's rendering even if the new file is unreadable.
        // A failed load must never authorize overwriting that unreadable file.
        leave();
        try {
            if (path.empty() || path.filename().empty())
                throw std::invalid_argument("Missing shape workspace file");
            if (std::filesystem::exists(path)) shapes.replace(readShapes(path));
            destination = std::move(path);
        } catch (...) {
            loadFailed = true;
            throw;
        }
    }

    // Change a candidate first. Publish it only after persistence succeeds;
    // failed writes preserve both the live collection and its previous file.
    template<class Mutation> auto change(Mutation mutation) {
        using Result = std::invoke_result_t<Mutation, ShapeCollection&>;
        static_assert(std::is_void_v<Result> || (!std::is_reference_v<Result> && std::is_nothrow_move_constructible_v<Result>));
        static_assert(std::is_nothrow_move_assignable_v<ShapeCollection>);
        if (loadFailed) throw std::runtime_error("Shape workspace failed to load; retry before editing");
        auto candidate = shapes;
        auto commit = [&] {
            if (destination) {
                try { writeShapes(*destination, definitions(candidate)); }
                catch (std::exception const& error) { throw ShapeSaveError(error.what()); }
            }
            shapes = std::move(candidate);
        };
        if constexpr (std::is_void_v<Result>) {
            mutation(candidate);
            commit();
        } else {
            auto result = mutation(candidate);
            commit();
            return result;
        }
    }
};
}
