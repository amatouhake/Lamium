#pragma once
#include "features/map/Waypoints.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Which saved data belongs to the server being joined (BACKLOG L-343). Pure:
// the caller reads the connection and says how much data each folder holds.
//
// A server's data is kept under a key, the name of its folder and files. The
// address alone named it before, in whatever form it arrived: the name the
// player typed, the IP after a transfer, or a URL with the port in it, so
// one server ended up under several names. Now each key remembers the names,
// IPs and port it was reached by, and a join uses the key whose port matches
// and whose name or IP matches. A different port is a different server.
namespace lamium::map::servers {
// The host of "http://host:port/path", "host:port", "[v6]:port" or "host".
inline std::string hostOf(std::string_view address) {
    if (auto scheme = address.find("://"); scheme != std::string_view::npos) address.remove_prefix(scheme + 3);
    if (auto path = address.find('/'); path != std::string_view::npos) address = address.substr(0, path);
    if (!address.empty() && address.front() == '[') {
        auto end = address.find(']');
        return std::string(address.substr(1, end == std::string_view::npos ? end : end - 1));
    }
    // One colon is host:port; more is a bare IPv6 address.
    if (std::count(address.begin(), address.end(), ':') == 1) address = address.substr(0, address.find(':'));
    return std::string(address);
}
// A key from an address as serverFileName writes it; empty for none.
inline std::string keyOf(std::string_view address, int port) {
    auto file = serverFileName(address, port);
    if (!file.empty()) file.resize(file.size() - std::string_view(".json").size());
    return file;
}
// A world joined without an address (a friend's world): its level id, as hex
// so that any id is a safe, distinct file name.
inline std::string worldKey(std::string_view levelId) {
    if (levelId.empty() || levelId.size() > 64) return {};
    std::string key = "world_";
    for (unsigned char c : levelId) {
        key += "0123456789abcdef"[c >> 4];
        key += "0123456789abcdef"[c & 15];
    }
    return key;
}
// Names this connection's data may already be under, the one in use until
// now first: `address` is the connection's unresolved URL, else its IP.
inline std::vector<std::string> knownKeys(std::string_view address, std::string_view ip, int port) {
    std::vector<std::string> keys;
    auto add = [&](std::string key) {
        if (!key.empty() && std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(std::move(key));
    };
    add(keyOf(address.empty() ? ip : address, port));
    auto host = hostOf(address);
    add(keyOf(host, port));
    add(keyOf(ip, port));
    for (std::string_view name : {std::string_view(host), ip})
        for (std::string_view scheme : {"http", "https"})
            if (!name.empty())
                add(keyOf(std::string(scheme) + "://" + std::string(name) + ":" + std::to_string(port), port));
    return keys;
}
struct Entry {
    std::string key;
    std::vector<std::string> hosts; // Names it was joined by, lower case; not bare IPs.
    std::string ip;                 // Where it was last found.
    int port = 0;
    std::string levelId;            // The last one seen.
    // Other names found holding data for this server and not used: never
    // weighed again, and kept for the player to choose from later.
    std::vector<std::string> others;
    // The player's switch: this address uses this data whatever the port (a
    // server whose port changes with every visit). A record of the exact
    // port still comes first, so one port can be given other data.
    bool anyPort = false;
    bool operator==(Entry const&) const = default;
};
// A world joined without an address, for its name in the saved data list.
struct World {
    std::string key, name;
    bool operator==(World const&) const = default;
};
struct List {
    std::vector<Entry> servers;
    std::vector<World> worlds;
    bool operator==(List const&) const = default;
};
inline constexpr size_t maxEntries = 500, maxNames = 16;
// The join's name (lower case; empty when the address is the bare IP).
inline std::string nameOf(std::string_view address, std::string_view ip) {
    std::string host = hostOf(address);
    for (auto& c : host) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return host == ip ? std::string() : host;
}
// A recorded server is this join's when the port is the same and
// - it was joined by this name before, or
// - it is at this IP now, the level id does not say otherwise, and no name
//   says otherwise: the join has no name (a transfer gives the IP) or the
//   record has none yet.
// An IP is where a server was last found, not who it is: an address handed
// to another server later must not bring this one's data with it.
inline bool reachedBy(Entry const& entry, std::string_view name, std::string_view ip, int port, std::string_view levelId) {
    if (entry.port != port) return false;
    if (!name.empty()) {
        if (std::find(entry.hosts.begin(), entry.hosts.end(), name) != entry.hosts.end()) return true;
        if (!entry.hosts.empty()) return false;
    }
    if (ip.empty() || entry.ip != ip) return false;
    return levelId.empty() || entry.levelId.empty() || entry.levelId == levelId;
}
// The key for this join, recorded in `entries`. Empty when the connection
// has no address.
//
// A recorded server keeps its key: what is in use stays in use, whatever
// its folders hold later. Only when a join brings something new (a server
// not recorded, or a recorded one under a new name or IP) are the names its
// data may already be under looked at, and the one holding the most is
// used; `held(key)` says how much, below zero for nothing at all. Names
// passed over are remembered and not weighed again. Nothing is merged,
// moved or deleted.
template<class Held>
std::string choose(std::vector<Entry>& entries, std::string_view address, std::string_view ip, int port,
                   std::string_view levelId, Held held) {
    std::string host = hostOf(address);
    for (auto& c : host) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (port <= 0 || port > 65535 || (host.empty() && ip.empty())) return {};
    std::string const name = host == ip ? std::string() : host;
    levelId = levelId.substr(0, 64);
    auto remember = [](std::vector<std::string>& list, std::string_view value) {
        if (value.empty() || std::find(list.begin(), list.end(), value) != list.end()) return;
        if (list.size() >= maxNames) list.erase(list.begin());
        list.emplace_back(value);
    };
    auto record = [&](Entry& entry) {
        entry.port = port;
        remember(entry.hosts, name);
        if (!ip.empty()) entry.ip = std::string(ip);
        if (!levelId.empty()) entry.levelId = std::string(levelId);
    };
    auto matched = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return reachedBy(e, name, ip, port, levelId); });
    bool const known = matched != entries.end();
    if (known && (name.empty() || std::find(matched->hosts.begin(), matched->hosts.end(), name) != matched->hosts.end())
        && (ip.empty() || matched->ip == ip)) {
        record(*matched);
        return matched->key;
    }
    // Not recorded at this port: an address the player gave one data for every port.
    if (!known) {
        auto wide = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) {
            return e.anyPort && (name.empty() ? !ip.empty() && e.hosts.empty() && e.ip == ip
                                              : std::find(e.hosts.begin(), e.hosts.end(), name) != e.hosts.end());
        });
        if (wide != entries.end()) return wide->key;
    }
    // Something new: weigh the recorded key, if any, against names not recorded anywhere.
    auto recorded = [&](std::string const& key) {
        return std::any_of(entries.begin(), entries.end(), [&](Entry const& e) {
            return e.key == key || std::find(e.others.begin(), e.others.end(), key) != e.others.end();
        });
    };
    std::string chosen = known ? matched->key : std::string();
    long long most = known ? std::max(0ll, static_cast<long long>(held(chosen))) : -1;
    std::vector<std::string> passed;
    for (auto const& key : knownKeys(address, ip, port)) {
        if (recorded(key)) continue;
        long long const amount = held(key);
        if (amount < 0) continue;
        if (amount > most) {
            if (!chosen.empty()) passed.push_back(chosen);
            chosen = key;
            most = amount;
        } else passed.push_back(key);
    }
    if (chosen.empty()) chosen = keyOf(host.empty() ? ip : std::string_view(host), port);
    if (chosen.empty()) return {};
    Entry next = known ? *matched : Entry{};
    if (known) entries.erase(matched);
    // The name may be another record's already (the same address, another level id): that record is this one.
    if (auto same = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return e.key == chosen; });
        same != entries.end()) {
        for (auto const& key : next.others) remember(same->others, key);
        for (auto const& key : next.hosts) remember(same->hosts, key);
        next = *same;
        entries.erase(same);
    }
    next.key = chosen;
    for (auto const& key : passed) if (key != chosen) remember(next.others, key);
    next.others.erase(std::remove(next.others.begin(), next.others.end(), chosen), next.others.end());
    record(next);
    if (entries.size() >= maxEntries) entries.erase(entries.begin());
    entries.push_back(std::move(next));
    return chosen;
}
// The player's choice (L-343 part 3): this join uses `key` from now on. Its
// own record takes the key and remembers the one it had; another server
// recorded under `key` keeps using it too. False when there is no address.
inline bool assign(std::vector<Entry>& entries, std::string_view address, std::string_view ip, int port,
                   std::string_view levelId, std::string const& key) {
    std::string const name = nameOf(address, ip);
    if (key.empty() || port <= 0 || port > 65535 || (name.empty() && ip.empty())) return false;
    levelId = levelId.substr(0, 64);
    auto matched = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return reachedBy(e, name, ip, port, levelId); });
    if (matched == entries.end()) {
        if (entries.size() >= maxEntries) entries.erase(entries.begin());
        entries.push_back({});
        matched = entries.end() - 1;
        matched->port = port;
    }
    auto& entry = *matched;
    if (!entry.key.empty() && entry.key != key && std::find(entry.others.begin(), entry.others.end(), entry.key) == entry.others.end()) {
        if (entry.others.size() >= maxNames) entry.others.erase(entry.others.begin());
        entry.others.push_back(entry.key);
    }
    entry.key = key;
    entry.others.erase(std::remove(entry.others.begin(), entry.others.end(), key), entry.others.end());
    if (!name.empty() && std::find(entry.hosts.begin(), entry.hosts.end(), name) == entry.hosts.end()) entry.hosts.push_back(name);
    if (!ip.empty()) entry.ip = std::string(ip);
    if (!levelId.empty()) entry.levelId = std::string(levelId);
    return true;
}
// The data under `key` was deleted: its records go, and nothing points at it.
inline void forget(List& list, std::string const& key) {
    std::erase_if(list.servers, [&](Entry const& e) { return e.key == key; });
    for (auto& e : list.servers) std::erase(e.others, key);
    std::erase_if(list.worlds, [&](World const& w) { return w.key == key; });
}
// Two records of one address: a shared name, or neither has a name and the IP is the same.
inline bool sameAddress(Entry const& a, Entry const& b) {
    for (auto const& host : a.hosts)
        if (std::find(b.hosts.begin(), b.hosts.end(), host) != b.hosts.end()) return true;
    return a.hosts.empty() && b.hosts.empty() && !a.ip.empty() && a.ip == b.ip;
}
// The any-port switch of the data under `key`. On: the address's other ports give up their own records, so
// every port uses this data; what they used is remembered as passed over, and a port given other data
// afterwards is recorded again and keeps it. Returns how many ports were released, below zero for no record.
inline int setAnyPort(std::vector<Entry>& entries, std::string const& key, bool on) {
    auto wide = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return e.key == key; });
    if (wide == entries.end()) return -1;
    if (!on) {
        for (auto& e : entries) if (e.key == key) e.anyPort = false;
        return 0;
    }
    Entry kept = *wide;
    kept.anyPort = true;
    entries.erase(wide);
    int released = 0;
    std::erase_if(entries, [&](Entry const& e) {
        if (!sameAddress(e, kept) || e.port == kept.port) return false;
        if (e.key != kept.key && std::find(kept.others.begin(), kept.others.end(), e.key) == kept.others.end()
            && kept.others.size() < maxNames) kept.others.push_back(e.key);
        ++released;
        return true;
    });
    entries.push_back(std::move(kept));
    return released;
}
// How a key reads in the list: "host:port" from its record, else worked out
// of the key itself (a name from before L-343).
inline std::string shownName(std::vector<Entry> const& entries, std::string const& key) {
    for (auto const& e : entries)
        if (e.key == key && e.port > 0 && !(e.hosts.empty() && e.ip.empty()))
            return (e.hosts.empty() ? e.ip : e.hosts.back()) + ":" + std::to_string(e.port);
    std::string_view name = key;
    for (std::string_view scheme : {"https___", "http___"})
        if (name.starts_with(scheme)) {
            name.remove_prefix(scheme.size());
            // "host_port_port": the URL's own port and the one appended.
            if (auto last = name.rfind('_'); last != std::string_view::npos)
                if (auto before = name.rfind('_', last - 1); before != std::string_view::npos
                    && name.substr(before, last - before) == name.substr(last)) name = name.substr(0, last);
            break;
        }
    auto split = name.rfind('_');
    if (split == std::string_view::npos || split + 1 >= name.size()
        || name.substr(split + 1).find_first_not_of("0123456789") != std::string_view::npos) return std::string(name);
    return std::string(name.substr(0, split)) + ":" + std::string(name.substr(split + 1));
}
// Why another key may be this join's data, for the list's mark.
enum class Maybe { No, UsedBefore, SameLevel, SameAddress };
inline Maybe maybe(std::vector<Entry> const& entries, std::string const& current, std::string const& other) {
    if (current.empty() || other == current) return Maybe::No;
    auto mine = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return e.key == current; });
    if (mine == entries.end()) return Maybe::No;
    if (std::find(mine->others.begin(), mine->others.end(), other) != mine->others.end()) return Maybe::UsedBefore;
    for (auto const& e : entries) {
        if (e.key != other) continue;
        if (!mine->levelId.empty() && e.levelId == mine->levelId) return Maybe::SameLevel;
        for (auto const& host : e.hosts)
            if (std::find(mine->hosts.begin(), mine->hosts.end(), host) != mine->hosts.end()) return Maybe::SameAddress;
        if (mine->hosts.empty() && e.hosts.empty() && !mine->ip.empty() && e.ip == mine->ip) return Maybe::SameAddress;
    }
    return Maybe::No;
}
}
