#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>

#include <nlohmann/json.hpp>

#include <future>
#include <iostream>
#include <optional>
#include <string>
#include <unistd.h>

using namespace std;
using namespace gqlxy;
using json = nlohmann::json;

static string ToWsUrl(const string& url) {
    if (url.starts_with("https://")) return "wss://" + url.substr(8);
    if (url.starts_with("http://")) return "ws://" + url.substr(7);
    return url;
}

static GraphQLRequest ParseInput(const string& text) {
    if (!text.empty() && text.front() == '{') {
        try {
            const auto j = json::parse(text);
            if (j.contains("query")) {
                return {
                    .query = j["query"],
                    .variables = j.value("variables", json(nullptr)),
                    .operationName =
                        j.contains("operationName") ? make_optional(j["operationName"].get<string>()) : nullopt,
                };
            }
        } catch (...) {
        }
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

static void PrintResult(const GraphQLResult& r) {
    if (r.data) cout << r.data->dump(2) << "\n";
    if (r.errors)
        for (const auto& e : *r.errors)
            cerr << "GraphQL error: " << e.message << "\n";
}

static void Execute(Client& client, const GraphQLRequest& req) {
    const auto op = DetectOp(req.query);
    const auto obs = op == OperationType::Subscription ? client.Subscribe(req.query, req.variables)
                     : op == OperationType::Mutation   ? client.Mutation(req.query, req.variables)
                                                       : client.Query(req.query, req.variables);
    promise<void> done;
    obs.subscribe(
        [](const GraphQLResult& r) { PrintResult(r); },
        [&done](exception_ptr ep) {
            try {
                rethrow_exception(ep);
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << "\n";
            }
            done.set_value();
        },
        [&done]() { done.set_value(); });
    done.get_future().get();
}

static optional<string> ReadOperation(bool tty) {
    string result;
    string line;
    bool first_line = true;

    while (true) {
        if (tty) cerr << (first_line ? "\n> " : "  ");
        if (!getline(cin, line)) break;
        if (line.empty()) {
            if (!result.empty()) break;
        } else {
            if (!result.empty()) result += '\n';
            result += line;
            first_line = false;
        }
    }

    return result.empty() ? nullopt : optional(result);
}

int main(int argc, char* argv[]) {
    string url = "http://localhost:4000/graphql";

    for (int i = 1; i < argc - 1; ++i)
        if (string(argv[i]) == "--url") url = argv[i + 1];

    const string ws_url = ToWsUrl(url);
    const bool tty = isatty(fileno(stdin));

    // clang-format off
    Client client({
        .link = make_shared<SplitLink>(
            [](const GraphQLRequest& req) {
                return req.type == OperationType::Subscription;
            },
            make_shared<HttpLink>(HttpLinkOptions{.url = url}),
            make_shared<WsLink>(WsLinkOptions{.url = ws_url})
        ),
    });
    // clang-format on

    if (tty) {
        cerr << "gqlxy repl  " << url << endl;
        cerr << "Enter a GraphQL operation (blank line to execute, Ctrl+D to quit)." << endl;
        cerr << R"(Input can be plain GraphQL or {"query":"...","variables":{...}}.)" << endl;
    }

    while (auto input = ReadOperation(tty)) {
        Execute(client, ParseInput(*input));
        cout << flush;
    }

    if (tty) cerr << endl;
    return 0;
}
