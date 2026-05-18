#include <deque>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <gqlxy/client/client.h>
#include <gqlxy/client/links/http_link.h>
#include <gqlxy/client/links/split_link.h>
#include <gqlxy/client/links/ws_link.h>
#include <gqlxy/core/parser/peg/parser/query/parse_document.h>
#include <gqlxy/core/results.h>
#include <gqlxy/client/subscription.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace ftxui;
using json = nlohmann::json;

static Color RandomRequestColor() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution dist(0x000000, 0xFFFFFF);

    int color = dist(rng);
    return {static_cast<uint8_t>(color >> 16U), static_cast<uint8_t>(color >> 8U), static_cast<uint8_t>(color)};
}

static GraphQLRequest ParseInput(const string& text) {
    if (text.empty()) return {.query = text};
    try {
        const auto j = json::parse(text);
        if (j.contains("query")) {
            return {
                .query = j["query"],
                .variables = j.value("variables", nullptr),
                .operationName = make_optional_if(
                    j.contains("operationName"), [&] { return j["operationName"].get<string>(); }),
            };
        }
    } catch (...) {
    }
    return {.query = text};
}

static OperationType DetectOp(const string& query) {
    return ParseDocument(query).operations[0].type;
}

struct LogEntry {
    enum class Kind { Info, Data, Error, Stderr, SubStart, SubEvent, SubEnd };
    Kind kind;
    int sub_id = -1;
    Color color = Color::Default;
    string text;
};

class Log {
public:
    Log(ScreenInteractive& screen) : _screen(screen) {}

    void Append(LogEntry e) {
        lock_guard lock(_mtx);
        _entries.push_back(std::move(e));
        if (_entries.size() > Max) _entries.pop_front();
        _screen.PostEvent(Event::Custom);
    }

    int Size() {
        lock_guard lock(_mtx);
        return static_cast<int>(_entries.size());
    }

    vector<LogEntry> Entries() {
        lock_guard lock(_mtx);
        return {_entries.begin(), _entries.end()};
    }

private:
    ScreenInteractive& _screen;
    mutex _mtx;
    deque<LogEntry> _entries;
    static constexpr size_t Max = 1000;
};

static Elements ColoredLines(const string& s, Color c) {
    auto lines = to_vector(split(s, '\n') | views::transform([&](const string& l) {
        return text(l) | color(c);
    }));
    return lines.empty() ? Elements{text("") | color(c)} : lines;
}

static Element ColoredText(const string& s, Color c) {
    return vbox(ColoredLines(s, c));
}

static Element RenderEntry(const LogEntry& e) {
    switch (e.kind) {
        case LogEntry::Kind::Info: return text(e.text) | dim;
        case LogEntry::Kind::Data: return ColoredText(e.text, e.color);
        case LogEntry::Kind::Error: return ColoredText(e.text, Color::Red);
        case LogEntry::Kind::Stderr: return ColoredText(e.text, Color::Yellow);
        case LogEntry::Kind::SubStart:
            return hbox({ text(format(" \u25B6 sub:{} ", e.sub_id)) | color(e.color) | bold, text(e.text) | color(e.color) });
        case LogEntry::Kind::SubEnd:
            return hbox({ text(format(" \u25A0 sub:{} ", e.sub_id)) | dim, text(e.text) | dim });
        case LogEntry::Kind::SubEvent: {
            return vbox(to_vector(ColoredLines(e.text, e.color) | views::transform([&](Element line) {
                return hbox({text(format("   sub:{} ", e.sub_id)) | color(e.color), line});
            })));
        }
    }
    return text(e.text);
}

class ActiveSubs {
public:
    int AllocId() {
        lock_guard lock(_mutex);
        return _nextId++;
    }

    void Store(int id, const Subscription& sub) {
        lock_guard lock(_mutex);
        _subs[id] = sub;
    }

    void Remove(int id) {
        lock_guard lock(_mutex);
        _subs.erase(id);
    }

    bool Cancel(int id) {
        lock_guard lock(_mutex);
        return and_then(to_optional(_subs, _subs.find(id)), [this](auto it) {
            it.second.Unsubscribe();
            _subs.erase(it.first);
            return make_optional(true);
        }).value_or(false);
    }

    void CancelAll() {
        lock_guard lock(_mutex);
        for (auto& sub : _subs | views::values)
            sub.Unsubscribe();
        _subs.clear();
    }

    vector<int> Ids() {
        lock_guard lock(_mutex);
        return to_vector(_subs | views::keys);
    }

private:
    mutex _mutex;
    map<int, Subscription> _subs;
    int _nextId = 0;
};

class StreamCaptureBuf : public streambuf {
public:
    using Callback = function<void(const string&)>;

    StreamCaptureBuf(streambuf* original, Callback cb) : _original(original), _cb(std::move(cb)) {}

protected:
    int overflow(int c) override {
        if (c == EOF) return EOF;
        if (c == '\n') FlushLine();
        else _buf += static_cast<char>(c);
        return c;
    }

    streamsize xsputn(const char* s, streamsize n) override {
        for (auto i = 0; i < n; ++i)
            overflow(s[i]);
        return n;
    }

private:
    void FlushLine() {
        if (!_buf.empty()) {
            _cb(_buf);
            _buf.clear();
        }
    }

    streambuf* _original;
    Callback _cb;
    string _buf;
};

static string FormatResult(const GraphQLResponse& r) {
    return Serialize(r).dump(2);
}

static string ExceptionMessage(exception_ptr ep) {
    try {
        rethrow_exception(ep);
    } catch (const exception& e) {
        return e.what();
    }
    return "unknown error";
}

class Request {
public:
    Request(Client& client, Log& log, ActiveSubs& activeSubs) : _client(client), _log(log), _activeSubs(activeSubs) {}

    void Execute(const GraphQLRequest& req) {
        const auto op = DetectOp(req.query);
        const Color color = RandomRequestColor();

        if (op._value == OperationType::SUBSCRIPTION) {
            const int id = _activeSubs.AllocId();
            auto sub = _client.Subscribe({.query = req.query, .variables = req.variables})
                .subscribe(
                    [&, id, color](const GraphQLResponse& r) {
                        _log.Append({LogEntry::Kind::SubEvent, id, color, FormatResult(r)});
                    },
                    [&, id, color](exception_ptr ep) {
                        _log.Append({LogEntry::Kind::SubEnd, id, color, "error: " + ExceptionMessage(ep)});
                        _activeSubs.Remove(id);
                    },
                    [&, id, color]() {
                        _log.Append({LogEntry::Kind::SubEnd, id, color, "completed"});
                        _activeSubs.Remove(id);
                    });
            _activeSubs.Store(id, std::move(sub));
            _log.Append({LogEntry::Kind::SubStart, id, color, "started"});
            return;
        }

        auto obs = op._value == OperationType::MUTATION
                       ? _client.Mutation({.query = req.query, .variables = req.variables})
                       : _client.Query({.query = req.query, .variables = req.variables});

        obs.subscribe(
            [&, color](const GraphQLResponse& r) {
                _log.Append({
                    .kind = LogEntry::Kind::Data,
                    .color = color,
                    .text = FormatResult(r)
                });
            },
            [&](exception_ptr ep) {
                _log.Append({
                    .kind = LogEntry::Kind::Error,
                    .text = ExceptionMessage(ep)
                });
            });
    }

private:
    Client& _client;
    Log& _log;
    ActiveSubs& _activeSubs;
};

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    const string url = "http://localhost:4000/graphql";
    const string wsUrl = "ws://localhost:4000/graphql";

    // clang-format off
    Client client({
        .link = /*make_shared<SplitLink>(
            [](const GraphQLRequest& req) { return req.type != OperationType::Subscription; },*/
            make_shared<HttpLink>(HttpLinkOptions{.url = url})/*,
            make_shared<WsLink>(WsLinkOptions{.url = wsUrl})
        )*/
    });
    // clang-format on

    auto screen = ScreenInteractive::Fullscreen();
    Log log(screen);
    ActiveSubs active;
    Request request(client, log, active);
    int focusLine = -1;

    StreamCaptureBuf cerrBuffer(cerr.rdbuf(), [&](const string& line) {
        log.Append({ .kind = LogEntry::Kind::Stderr, .text  = line});
    });
    auto* previousCerr = cerr.rdbuf(&cerrBuffer);

    string input;
    auto inputComponent = Input(&input, "GraphQL query/mutation/subscription  :subs  :cancel [id]", {
        .multiline = false,
        .on_enter = [&] {
            if (input.empty()) return;
            log.Append({ .kind = LogEntry::Kind::Info, .text = format("> {}", input)});

            if (input == ":subs") {
                const auto ids = active.Ids();
                const auto label = ids.empty() ? "none" : to_vector(ids | views::transform([](int id) {
                    return format("sub:{}", id);
                })) | join_with(" ");
                log.Append({ .kind = LogEntry::Kind::Info, .text = format("active: {}", label)});
            }
            else if (input == ":cancel") {
                active.CancelAll();
                log.Append({ .kind = LogEntry::Kind::Info, .text = "all subscriptions cancelled"});
                screen.PostEvent(Event::Custom);
            }
            else if (input.starts_with(":cancel ")) {
                try {
                    const int id = stoi(input.substr(8));
                    const auto msg = active.Cancel(id)
                        ? format("sub:{} cancelled", id)
                        : format("no active sub with id {}", id);
                    log.Append({ .kind = LogEntry::Kind::Info, .text = msg});
                } catch (...) {
                    log.Append({ .kind = LogEntry::Kind::Info, .text = "usage: :cancel [id]"});
                }
                screen.PostEvent(Event::Custom);
            }
            else {
                try {
                    request.Execute(ParseInput(input));
                } catch (...) {
                    log.Append({ .kind = LogEntry::Kind::Error, .text = "not a command or valid GraphQL query"});
                }
            }
            input.clear();
        }
    });

    auto logRenderer = Renderer([&] {
        auto snapshot = log.Entries();
        if (snapshot.empty()) return text("") | flex;

        const int size = static_cast<int>(snapshot.size());
        const int target = focusLine < 0 || focusLine >= size ? size - 1 : focusLine;

        return vbox(to_vector(views::iota(0, size)
            | views::transform([&](int i) {
                auto e = RenderEntry(snapshot[i]);
                return i == target ? e | focus : e;
            }))) | vscroll_indicator | frame | flex;
    });

    auto statusBar = Renderer([&]() {
        const auto ids = active.Ids();
        return !ids.empty() ? hbox({text("  active: ") | bold, hbox(to_vector(ids | views::transform([](int id) {
            return hbox({text(format("sub:{}", id)) | bold, text("  ")});
        })))}) : text("");
    });

    auto root = CatchEvent(
        Renderer(
            Container::Vertical({logRenderer, inputComponent}),
            [&] {
                return vbox(concat(Elements {
                    hbox({
                        text(" gqlxy ") | bold | color(Color::Blue),
                        text("\u00B7 ") | dim,
                        text(url) | dim,
                        !wsUrl.empty() ? text(format("  ws: {}", wsUrl)) | dim : text(""),
                    }),
                    separator(),
                    logRenderer->Render() | flex,
                    separator(),
                }, active.Ids().empty() ? Elements{
                    statusBar->Render(),
                    separator(),
                } : Elements{}, Elements{
                    hbox({text(" > ") | bold, inputComponent->Render() | flex}),
                    text("  Enter: send  Ctrl-C: quit  :subs  :cancel [id]") | dim
                }));
            }
        ), [&](Event e) {
            if (e == Event::CtrlC) {
                active.CancelAll();
                screen.ExitLoopClosure()();
                return true;
            }
            if (e.is_mouse()) {
                int n = log.Size();
                if (n == 0) return false;
                const int current = focusLine < 0 || focusLine >= n ? n - 1 : focusLine;
                switch (e.mouse().button) {
                    case Mouse::WheelUp:
                        focusLine = max(0, current - 3);
                        return true;
                    case Mouse::WheelDown: {
                        const int next = min(n - 1, current + 3);
                        focusLine = next >= n - 1 ? -1 : next;
                        return true;
                    }
                    default: break;
                }
            }
            return false;
        }
    );

    log.Append({ .kind = LogEntry::Kind::Info, .text = format("Connected to {}", url) });
    screen.Loop(root);
    active.CancelAll();
    cerr.rdbuf(previousCerr);
    return 0;
}
