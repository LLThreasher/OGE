#include <test_macros.hpp>
#include <vector>

#include "oge/runtime/oge_registry.hpp"

struct TestComp { int v = 0; };
DECL_TYPE_NAME(TestComp, "TestComp")

// -- owning / non-owning --
TEST(default_ctr) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    CHECK(reg.valid(e));
}

// -- entity lifecycle --
TEST(create_destroy) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    CHECK(reg.valid(e));
    reg.destroy(e);
    CHECK(!reg.valid(e));
}

TEST(create_hint) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create(entt::entity{42});
    CHECK(reg.valid(e));
    CHECK_EQ(static_cast<uint32_t>(e), 42u);
}

// -- component emplace / get --
TEST(emplace_and_get) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    reg.emplace<TestComp>(e, 99);
    auto& c = reg.get<TestComp>(e);
    CHECK_EQ(c.v, 99);
}

TEST(get_const) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    reg.emplace<TestComp>(e, 42);
    const auto& cr = reg;
    auto& c = cr.get<TestComp>(e);
    CHECK_EQ(c.v, 42);
}

TEST(try_get) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    CHECK(reg.try_get<TestComp>(e) == nullptr);
    reg.emplace<TestComp>(e);
    CHECK(reg.try_get<TestComp>(e) != nullptr);
}

TEST(emplace_or_replace) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    reg.emplace<TestComp>(e, 1);
    reg.emplace_or_replace<TestComp>(e, 2);
    CHECK_EQ(reg.get<TestComp>(e).v, 2);
}

// -- component queries --
TEST(all_of) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    CHECK(!reg.all_of<TestComp>(e));
    reg.emplace<TestComp>(e);
    CHECK(reg.all_of<TestComp>(e));
}

TEST(any_of) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    CHECK(!reg.any_of<TestComp>(e));
    reg.emplace<TestComp>(e);
    CHECK(reg.any_of<TestComp>(e));
}

TEST(remove) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    reg.emplace<TestComp>(e);
    CHECK(reg.all_of<TestComp>(e));
    reg.remove<TestComp>(e);
    CHECK(!reg.all_of<TestComp>(e));
}

// -- views --
TEST(view_iterate) {
    oge::runtime::OgeRegistry reg;
    auto e1 = reg.create(); reg.emplace<TestComp>(e1, 10);
    auto e2 = reg.create(); reg.emplace<TestComp>(e2, 20);
    int sum = 0;
    for (auto e : reg.view<TestComp>())
        sum += reg.get<TestComp>(e).v;
    CHECK_EQ(sum, 30);
}

TEST(const_view) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create(); reg.emplace<TestComp>(e, 5);
    const auto& cr = reg;
    int sum = 0;
    for (auto e : cr.view<const TestComp>())
        sum += cr.get<const TestComp>(e).v;
    CHECK_EQ(sum, 5);
}

// -- raw access --
TEST(raw_access) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.Raw().create();
    CHECK(reg.Raw().valid(e));
}

// -- implicit conversion --
TEST(implicit_conv) {
    oge::runtime::OgeRegistry reg;
    entt::registry& raw = reg;
    auto e = raw.create();
    CHECK(raw.valid(e));
}

// -- clear --
TEST(clear) {
    oge::runtime::OgeRegistry reg;
    auto e = reg.create();
    reg.emplace<TestComp>(e);
    reg.clear();
    CHECK(!reg.valid(e));
}

// -- mutable view --
TEST(view_mutate) {
    oge::runtime::OgeRegistry reg;
    auto e1 = reg.create(); reg.emplace<TestComp>(e1, 10);
    auto e2 = reg.create(); reg.emplace<TestComp>(e2, 20);
    for (auto e : reg.view<TestComp>())
        reg.get<TestComp>(e).v *= 2;
    CHECK_EQ(reg.get<TestComp>(e1).v, 20);
    CHECK_EQ(reg.get<TestComp>(e2).v, 40);
}

// -- mutable ctx --
TEST(ctx_mutate) {
    oge::runtime::OgeRegistry reg;
    reg.ctx().emplace<int>(7);
    reg.ctx().get<int>() = 99;
    CHECK_EQ(reg.ctx().get<int>(), 99);
}

TEST(ctx_contains_and_erase) {
    oge::runtime::OgeRegistry reg;
    CHECK(!reg.ctx().contains<int>());
    reg.ctx().emplace<int>(1);
    CHECK(reg.ctx().contains<int>());
    reg.ctx().erase<int>();
    CHECK(!reg.ctx().contains<int>());
}

// -- signals --
static int g_sigCount = 0;
static void onSig(entt::registry&, entt::entity) { ++g_sigCount; }

TEST(signals_fire) {
    oge::runtime::OgeRegistry reg;
    g_sigCount = 0;
    reg.on_construct<TestComp>().template connect<&onSig>();
    auto e = reg.create();
    reg.emplace<TestComp>(e);
    CHECK_EQ(g_sigCount, 1);
}

RUN_TESTS("OgeRegistry Tests")
