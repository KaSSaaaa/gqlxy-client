#pragma once

#include <gqlxy/ResolverArgs.h>
#include <gqlxy/resolvers.h>
#include <gqlxy/schema.h>
#include <gqlxy/subscription.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace gqlxy::e2e {

inline const std::map<std::string, Resolver> Users = {
    {"1", Resolver{
        {"id", "1"},
        {"name", "Alice"},
        {"email", "alice@example.com"}
    }},
    {"2", Resolver{{"id", "2"}, {"name", "Bob"},   {"email", "bob@example.com"}}},
};

inline Schema MakeE2ESchema() {
    using namespace std;
    return Schema({
        .typeDefs = R"(
            type User {
                id: ID!
                name: String!
                email: String!
            }
            type Query {
                hello: String!
                echo(message: String!): String!
                user(id: ID!): User
                delay(ms: Int!): String!
                fail: String
            }
            type Subscription {
                onCount(to: Int!): Int!
            }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"hello", "Hello from gqlxy!"},
                {"echo", FunctionResolver{[](const ResolverArgs& a) -> ValueResolver {
                    return a.Args()["message"].get<string>();
                }}},
                {"user", FunctionResolver{[](const ResolverArgs& a) -> ValueResolver {
                    auto id = a.Args()["id"].get<string>();
                    auto it = Users.find(id);
                    if (it == Users.end()) return monostate{};
                    return it->second;
                }}},
                {"delay", FunctionResolver{[](const ResolverArgs& a) -> ValueResolver {
                    auto ms = a.Args()["ms"].get<int>();
                    this_thread::sleep_for(chrono::milliseconds(ms));
                    return "delayed " + to_string(ms) + "ms";
                }}},
                {"fail", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
                    throw runtime_error("Intentional resolver failure");
                }}},
            }},
            {"Subscription", Resolver{
                {"onCount", SubscriptionResolver{[](const ResolverArgs& a) -> SubscriptionEventStream {
                    auto counter = make_shared<int>(0);
                    return SubscriptionEventStream{
                        [counter, to = a.Args().value("to", 0)]() -> ValueResolver {
                            if (*counter >= to) return monostate{};
                            return ++(*counter);
                        },
                        [] {}
                    };
                }}},
            }},
        },
    });
}

}
