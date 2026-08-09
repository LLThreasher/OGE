/// Smoke test for DebugScene3 — verifies compilation, linkage, and type hierarchy.
#include "test_macros.hpp"

#include "game/debug_scene.hpp"
#include "game/app_context.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

TEST(type_register) {
    auto name = oge::runtime::TypeName<game::DebugScene3>::Get();
    CHECK_EQ(name, "core::DebugScene3");
}

TEST(inheritance) {
    bool ok1 = std::is_base_of_v<game::SceneExt, game::DebugScene3>;
    bool ok2 = std::is_base_of_v<game::Scene, game::DebugScene3>;
    bool ok3 = std::is_base_of_v<game::AppRuntime, game::DebugScene3>;
    CHECK(ok1);
    CHECK(ok2);
    CHECK(ok3);
}

TEST(default_scene_config) {
    oge::runtime::OgeRegistry dummyReg;
    oge::runtime::OGEContext octx(dummyReg.Raw());
    oge::runtime::TypeRegistry types(octx);
    auto cfg = game::GetDefaultSceneConfig(types);
    CHECK(!cfg.subsystems.empty());
    CHECK(!cfg.realtimeSubsystems.empty());
    CHECK(cfg.loadMask & game::SceneConfig::LOAD_MASK_BLOCKS);
    CHECK(cfg.loadMask & game::SceneConfig::LOAD_MASK_TERRAIN);
}

namespace
{
// Lightweight stand-in types to exercise registration without heavy context.
struct TestBase
{
    struct Def
    {
        int value = 0;
    };
    virtual ~TestBase() = default;
    int value = 0;
};

struct TestDerived : TestBase
{
    TestDerived(Def def)
    {
        value = def.value;
    }
};
}  // namespace

DECL_TYPE_NAME(TestBase, "core::TestBase")
DECL_TYPE_NAME(TestDerived, "core::TestDerived")

TEST(type_name_registration) {
    oge::runtime::OgeRegistry dummyReg;
    oge::runtime::OGEContext octx(dummyReg.Raw());
    oge::runtime::TypeRegistry types(octx);

    types.RegisterABC<TestBase>();
    types.RegisterDerived<TestBase, TestDerived>();

    // Family name recorded on RegisterFamily — family resolvable by name.
    CHECK(types.GetFamily("core::TestBase") ==
          entt::hashed_string{"core::TestBase"}.value());

    // Type names recorded on register — Id(name) matches Id<T>() (type hash).
    CHECK(types.Id("core::TestBase") == types.Id<TestBase>());
    CHECK(types.Id("core::TestDerived") == types.Id<TestDerived>());
    CHECK(types.GetDescriptor(types.Id<TestDerived>()) != nullptr);

    // Name-based Build constructs via the registered factory.
    auto raw = types.Build("core::TestDerived", TestDerived::Def{42});
    CHECK(raw.has_value());
    auto ptr = entt::any_cast<std::unique_ptr<TestBase>>(std::move(raw));
    CHECK(ptr != nullptr);
    CHECK_EQ(ptr->value, 42);

    // Name-based Build on an unregistered name returns empty any.
    CHECK(!types.Build("core::NoSuchType").has_value());
}

RUN_TESTS("DebugScene3 Smoke Tests")
