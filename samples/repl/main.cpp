#include <deque>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/parser/peg/parser/query/parse_document.h>
#include <gqlxy/results.h>
#include <gqlxy/subscription.h>
#include <gqlxy/utils/optional.h>
#include <gqlxy/utils/ranges.h>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace ftxui;
using json = nlohmann::json;

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
    enum class Kind {
        Info,
        Data,
        Error,
        Stdout,
        Stderr,
        SubStart,
        SubEvent,
        SubEnd
    };
    Kind kind;
    int sub_id = -1;
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

    size_t Size() {
        lock_guard lock(_mtx);
        return _entries.size();
    }

    vector<LogEntry> Entries() {
        lock_guard lock(_mtx);
        return std::vector<LogEntry> { _entries.begin(), _entries.end() };
    }

private:
    ScreenInteractive& _screen;
    mutex _mtx;
    deque<LogEntry> _entries;
    static constexpr size_t Max = 1000;
};

static Elements SplitLines(const string& s) {
    auto result = to_vector(split(s, '\n') | views::transform([](const string& l) { return text(l); }));
    if (result.empty()) return {text("")};
    return result;
}

static Element ColoredLines(const Elements& lines, Color c) {
    return vbox(to_vector(lines | views::transform([&](const Element& l) { return l | color(c); })));
}

static Element SubPrefix(int id, const string& icon) {
    return text(format(" {} sub:{} ", icon, id)) | color(Color::Cyan);
}

static Element RenderEntry(const LogEntry& e) {
    auto lines = SplitLines(e.text);

    switch (e.kind) {
        case LogEntry::Kind::Info: return vbox(lines) | dim;
        case LogEntry::Kind::Data: return vbox(lines);
        case LogEntry::Kind::Error: return ColoredLines(lines, Color::Red);
        case LogEntry::Kind::Stdout: return ColoredLines(lines, Color::White);
        case LogEntry::Kind::Stderr: return ColoredLines(lines, Color::Yellow);
        case LogEntry::Kind::SubStart:
            return hbox({SubPrefix(e.sub_id, "\u25B6") | bold, lines[0] | color(Color::Cyan)});
        case LogEntry::Kind::SubEnd: return hbox({SubPrefix(e.sub_id, "\u25A0") | dim, vbox(lines) | dim});
        case LogEntry::Kind::SubEvent:
            return vbox(to_vector(lines | views::transform([&](const auto& line) {
                return hbox({text(format("   sub:{} ", e.sub_id)) | color(Color::Cyan), line});
            })));
    }
    return vbox(lines);
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
            return true;
        });
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

        if (op._value == OperationType::SUBSCRIPTION) {
            const int id = _activeSubs.AllocId();
            auto sub =
                _client.Subscribe({.query = req.query, .variables = req.variables})
                    .subscribe(
                        [&, id](const GraphQLResponse& r) {
                            _log.Append({LogEntry::Kind::SubEvent, id, FormatResult(r)});
                        },
                        [&, id](exception_ptr ep) {
                            _log.Append({LogEntry::Kind::SubEnd, id, "error: " + ExceptionMessage(ep)});
                            _activeSubs.Remove(id);
                        },
                        [&, id]() {
                            _log.Append({LogEntry::Kind::SubEnd, id, "completed"});
                            _activeSubs.Remove(id);
                        });
            _activeSubs.Store(id, std::move(sub));
            _log.Append({LogEntry::Kind::SubStart, id, "started"});
            return;
        }

        auto obs = op._value == OperationType::MUTATION
                       ? _client.Mutation({.query = req.query, .variables = req.variables})
                       : _client.Query({.query = req.query, .variables = req.variables});

        obs.subscribe(
            [&](const GraphQLResponse& r) { _log.Append({LogEntry::Kind::Data, -1, FormatResult(r)}); },
            [&](exception_ptr ep) { _log.Append({LogEntry::Kind::Error, -1, ExceptionMessage(ep)}); });

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
        log.Append({LogEntry::Kind::Stderr, -1, line});
    });
    auto* previousCerr = cerr.rdbuf(&cerrBuffer);

    string input;
    auto inputComponent = Input(&input, "GraphQL query/mutation/subscription  :subs  :cancel [id]", {
        .multiline = false,
        .on_enter = [&] {
            if (input.empty()) return;
            log.Append({LogEntry::Kind::Info, -1, format("> {}", input)});

            if (input == ":subs") {
                log.Append({LogEntry::Kind::Info, -1, format("active: {}", active.Ids()
                    | views::transform([](int id) { return format("sub:{}", id); })
                    | join_with(" "))});
            }
            else if (input == ":cancel") {
                active.CancelAll();
                log.Append({LogEntry::Kind::Info, -1, "all subscriptions cancelled"});
                screen.PostEvent(Event::Custom);
            }
            else if (input.starts_with(":cancel ")) {
                try {
                    const int id = stoi(input.substr(8));
                    if (active.Cancel(id)) log.Append({LogEntry::Kind::Info, -1, format("sub:{} cancelled", id)});
                    else log.Append({LogEntry::Kind::Info, -1, format("no active sub with id {}", id)});
                } catch (...) {
                    log.Append({LogEntry::Kind::Info, -1, "usage: :cancel [id]"});
                }
                screen.PostEvent(Event::Custom);
            }
            else {
                try {
                    request.Execute(ParseInput(input));
                } catch (...) {
                    log.Append({LogEntry::Kind::Error, -1, "Not a command or a GraphQL query message"});
                }
            }
            input.clear();
        }
    });

    auto logRenderer = Renderer([&] {
        auto snapshot = log.Entries();
        if (snapshot.empty()) return text("") | flex;

        const int target = focusLine < 0 || focusLine >= snapshot.size() ? snapshot.size() - 1 : focusLine;

        return vbox(to_vector(std::views::iota(0, static_cast<int>(snapshot.size()))
            | std::views::transform([&](int i) {
                auto e = RenderEntry(snapshot[i]);
                return i == target ? e = e | focus : e;
            }))) | vscroll_indicator | frame | flex;
    });

    auto statusBar = Renderer([&]() {
        const auto ids = active.Ids();
        return !ids.empty() ? hbox({text("  active: ") | bold, hbox(ids | views::transform([](int id) {
           return hbox({text("sub:" + to_string(id)) | color(Color::Cyan) | bold, text("  ")});
        }))}) : text("");
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
                    case Mouse::WheelUp: {
                        focusLine = max(0, current - 3);
                        break;
                    }
                    case Mouse::WheelDown: {
                        const int next = min(n - 1, current + 3);
                        focusLine = next >= n - 1 ? -1 : next;
                        break;
                    }
                    default:
                        return false;
                }
                return true;
            }
            return false;
        }
    );

    log.Append({LogEntry::Kind::Info, -1, "Connected to " + url});
    screen.Loop(root);
    active.CancelAll();
    cerr.rdbuf(previousCerr);
    return 0;
}
