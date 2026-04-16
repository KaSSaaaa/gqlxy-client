#include "../../src/gqlxy/internal/url.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/subscription.h>

#include <nlohmann/json.hpp>

#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace ftxui;
using json = nlohmann::json;

// ─── input parsing ────────────────────────────────────────────────────────────

static GraphQLRequest ParseInput(const string& text) {
    if (!text.empty() && text.front() == '{') {
        try {
            const auto j = json::parse(text);
            if (j.contains("query")) {
                return {
                    .query = j["query"],
                    .variables = j.value("variables", json(nullptr)),
                    .operationName = j.contains("operationName")
                                         ? make_optional(j["operationName"].get<string>())
                                         : nullopt,
                };
            }
        } catch (...) {}
    }
    return {.query = text};
}

static OperationType DetectOp(const string& query) {
    const auto s = query.find_first_not_of(" \t\n\r");
    if (s == string::npos) return OperationType::Query;
    if (query.compare(s, 12, "subscription") == 0) return OperationType::Subscription;
    if (query.compare(s, 8, "mutation") == 0) return OperationType::Mutation;
    return OperationType::Query;
}

// ─── log ──────────────────────────────────────────────────────────────────────

struct LogEntry {
    enum class Kind { Info, Data, Error, SubStart, SubEvent, SubEnd };
    Kind kind;
    int sub_id = -1;
    string text;
};

struct Log {
    mutable mutex mtx;
    deque<LogEntry> entries;
    static constexpr size_t Max = 1000;

    void Append(LogEntry e) {
        lock_guard lock(mtx);
        entries.push_back(std::move(e));
        if (entries.size() > Max) entries.pop_front();
    }

    size_t Size() const {
        lock_guard lock(mtx);
        return entries.size();
    }
};

static Element RenderEntry(const LogEntry& e) {
    Elements lines;
    istringstream ss(e.text);
    string line;
    while (getline(ss, line)) lines.push_back(text(line));
    if (lines.empty()) lines.push_back(text(""));

    auto styled = [&](Color c) -> Element {
        Elements r;
        for (auto& l : lines) r.push_back(l | color(c));
        return vbox(r);
    };

    switch (e.kind) {
        case LogEntry::Kind::Info:
            return vbox(lines) | dim;
        case LogEntry::Kind::Data:
            return vbox(lines);
        case LogEntry::Kind::Error:
            return styled(Color::Red);
        case LogEntry::Kind::SubStart:
            return hbox({text(" ▶ sub:" + to_string(e.sub_id) + " ") | color(Color::Cyan) | bold,
                         lines[0] | color(Color::Cyan)});
        case LogEntry::Kind::SubEnd:
            return hbox({text(" ■ sub:" + to_string(e.sub_id) + " ") | color(Color::Cyan) | dim,
                         vbox(lines) | dim});
        case LogEntry::Kind::SubEvent: {
            Elements r;
            for (size_t i = 0; i < lines.size(); ++i) {
                if (i == 0)
                    r.push_back(hbox({text("   sub:" + to_string(e.sub_id) + " ") | color(Color::Cyan),
                                      lines[i]}));
                else
                    r.push_back(hbox({text(string(10 + to_string(e.sub_id).size(), ' ')), lines[i]}));
            }
            return vbox(r);
        }
    }
    return vbox(lines);
}

// ─── subscriptions ────────────────────────────────────────────────────────────

struct ActiveSubs {
    mutable mutex mtx;
    map<int, Subscription> subs;
    int nextId = 0;

    int AllocId() {
        lock_guard lock(mtx);
        return nextId++;
    }

    void Store(int id, Subscription sub) {
        lock_guard lock(mtx);
        subs.emplace(id, std::move(sub));
    }

    void Remove(int id) {
        lock_guard lock(mtx);
        subs.erase(id);
    }

    bool Cancel(int id) {
        lock_guard lock(mtx);
        auto it = subs.find(id);
        if (it == subs.end()) return false;
        it->second.Unsubscribe();
        subs.erase(it);
        return true;
    }

    void CancelAll() {
        lock_guard lock(mtx);
        for (auto& [_, sub] : subs) sub.Unsubscribe();
        subs.clear();
    }

    vector<int> Ids() const {
        lock_guard lock(mtx);
        vector<int> ids;
        for (const auto& [id, _] : subs) ids.push_back(id);
        return ids;
    }
};

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    string url = "http://localhost:4000/graphql";
    string wsUrl = "ws://localhost:4000/graphql";

    Client client({
        .link = make_shared<SplitLink>(
            [](const GraphQLRequest& req) { return req.type != OperationType::Subscription; },
            make_shared<HttpLink>(HttpLinkOptions{.url = url}),
            make_shared<WsLink>(WsLinkOptions{.url = wsUrl})
        )
    });

    Log log;
    ActiveSubs active;
    auto screen = ScreenInteractive::Fullscreen();

    // focus_line: -1 = auto-scroll to bottom, >= 0 = pinned entry index
    int focus_line = -1;

    auto append = [&](LogEntry e) {
        log.Append(std::move(e));
        screen.PostEvent(Event::Custom);
    };

    auto format_result = [](const GraphQLResponse& r) -> string {
        string out;
        if (r.data) out += r.data->dump(2);
        if (r.errors)
            for (const auto& e : *r.errors) {
                if (!out.empty()) out += "\n";
                out += "error: " + e.message;
            }
        return out.empty() ? "(empty)" : out;
    };

    auto execute = [&](const GraphQLRequest& req) {
        const auto op = DetectOp(req.query);

        if (op == OperationType::Subscription) {
            const int id = active.AllocId();
            auto obs = client.Subscribe({.query = req.query, .variables = req.variables});
            auto sub = obs.subscribe(
                [&, id](const GraphQLResult& r) {
                    append({LogEntry::Kind::SubEvent, id, format_result(r)});
                },
                [&, id](exception_ptr ep) {
                    string msg;
                    try { rethrow_exception(ep); } catch (const exception& e) { msg = e.what(); }
                    append({LogEntry::Kind::SubEnd, id, "error: " + msg});
                    active.Remove(id);
                    screen.PostEvent(Event::Custom);
                },
                [&, id]() {
                    append({LogEntry::Kind::SubEnd, id, "completed"});
                    active.Remove(id);
                    screen.PostEvent(Event::Custom);
                }
            );
            active.Store(id, std::move(sub));
            append({LogEntry::Kind::SubStart, id, "started"});
            return;
        }

        auto obs = op == OperationType::Mutation
            ? client.Mutation({.query = req.query, .variables = req.variables})
            : client.Query({.query = req.query, .variables = req.variables});

        obs.subscribe(
            [&](const GraphQLResponse& r) { append({LogEntry::Kind::Data, -1, format_result(r)}); },
            [&](exception_ptr ep) {
                string msg;
                try { rethrow_exception(ep); } catch (const exception& e) { msg = e.what(); }
                append({LogEntry::Kind::Error, -1, msg});
            }
        );
    };

    auto handle_command = [&](const string& cmd) -> bool {
        if (cmd == ":subs") {
            const auto ids = active.Ids();
            if (ids.empty()) {
                append({LogEntry::Kind::Info, -1, "no active subscriptions"});
            } else {
                string s = "active:";
                for (int id : ids) s += " sub:" + to_string(id);
                append({LogEntry::Kind::Info, -1, s});
            }
            return true;
        }
        if (cmd == ":cancel") {
            active.CancelAll();
            append({LogEntry::Kind::Info, -1, "all subscriptions cancelled"});
            screen.PostEvent(Event::Custom);
            return true;
        }
        if (cmd.starts_with(":cancel ")) {
            try {
                const int id = stoi(cmd.substr(8));
                if (active.Cancel(id))
                    append({LogEntry::Kind::Info, -1, "sub:" + to_string(id) + " cancelled"});
                else
                    append({LogEntry::Kind::Info, -1, "no active sub with id " + to_string(id)});
            } catch (...) {
                append({LogEntry::Kind::Info, -1, "usage: :cancel [id]"});
            }
            screen.PostEvent(Event::Custom);
            return true;
        }
        return false;
    };

    // Input
    string input_content;
    InputOption input_opt;
    input_opt.multiline = false;
    input_opt.on_enter = [&] {
        const string txt = input_content;
        input_content.clear();
        if (txt.empty()) return;
        append({LogEntry::Kind::Info, -1, "> " + txt});
        if (!handle_command(txt)) execute(ParseInput(txt));
    };
    auto input_component = Input(&input_content, "GraphQL query/mutation/subscription  :subs  :cancel [id]", input_opt);

    // Log renderer
    auto log_renderer = Renderer([&]() {
        vector<LogEntry> snapshot;
        {
            lock_guard lock(log.mtx);
            snapshot = {log.entries.begin(), log.entries.end()};
        }
        if (snapshot.empty()) return text("") | flex;

        const int n = (int)snapshot.size();
        const int target = (focus_line < 0 || focus_line >= n) ? n - 1 : focus_line;

        Elements elems;
        for (int i = 0; i < n; ++i) {
            Element e = RenderEntry(snapshot[i]);
            if (i == target) e = e | focus;
            elems.push_back(std::move(e));
        }
        return vbox(elems) | vscroll_indicator | frame | flex;
    });

    // Status bar
    auto status_bar = Renderer([&]() -> Element {
        const auto ids = active.Ids();
        if (ids.empty()) return text("");
        Elements parts = {text("  active: ") | bold};
        for (int id : ids) {
            parts.push_back(text("sub:" + to_string(id)) | color(Color::Cyan) | bold);
            parts.push_back(text("  "));
        }
        return hbox(parts);
    });

    auto layout = Container::Vertical({log_renderer, input_component});

    auto renderer = Renderer(layout, [&]() {
        const auto ids = active.Ids();
        Elements elems = {
            hbox({
                text(" gqlxy ") | bold | color(Color::Blue),
                text("· ") | dim,
                text(url) | dim,
                wsUrl.empty() ? text("") : (text("  ws: " + wsUrl) | dim),
            }),
            separator(),
            log_renderer->Render() | flex,
            separator(),
        };
        if (!ids.empty()) {
            elems.push_back(status_bar->Render());
            elems.push_back(separator());
        }
        elems.push_back(hbox({text(" > ") | bold, input_component->Render() | flex}));
        elems.push_back(text("  Enter: send  Ctrl-C: quit  :subs  :cancel [id]") | dim);
        return vbox(elems);
    });

    auto root = CatchEvent(renderer, [&](Event e) {
        if (e == Event::CtrlC) {
            active.CancelAll();
            screen.ExitLoopClosure()();
            return true;
        }
        if (e.is_mouse()) {
            const int n = (int)log.Size();
            if (n == 0) return false;
            const int current = (focus_line < 0 || focus_line >= n) ? n - 1 : focus_line;
            if (e.mouse().button == Mouse::WheelUp) {
                focus_line = max(0, current - 3);
                return true;
            }
            if (e.mouse().button == Mouse::WheelDown) {
                const int next = min(n - 1, current + 3);
                focus_line = (next >= n - 1) ? -1 : next;
                return true;
            }
        }
        return false;
    });

    append({LogEntry::Kind::Info, -1, "Connected to " + url});
    screen.Loop(root);
    active.CancelAll();
    return 0;
}
