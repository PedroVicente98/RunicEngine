#include "EntityStates.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "EntityWorld.hpp"

using namespace Runic::ECS;

namespace {

struct Health {
    std::int32_t current = 100;
    std::int32_t max = 100;
};

// Collects the facts of observable states, in the order they were reported.
class FactLog final : public StateFactSink {
  public:
    void OnStateEntered(const StateEntered &fact) override { facts.emplace_back(fact); }
    void OnStateExited(const StateExited &fact) override { facts.emplace_back(fact); }

    std::vector<std::variant<StateEntered, StateExited>> facts;
};

using Facts = std::vector<std::variant<StateEntered, StateExited>>;

// A world with one state of each category, in the engine's neutral test vocabulary.
struct Fixture {
    Fixture() : states(world) {
        world.RegisterComponent<Health>("engine:test_health");

        StateDeclaration stunned;
        stunned.name = "engine:test_stunned";
        stunned.description = "Cannot act";
        stunned.category = StateCategory::HotFlag;
        stunned.owner = "engine:test_control_system";
        stunned.observable = true;
        stunned.visibility = StateVisibility::Everyone;
        this->stunned = states.Declare(stunned);

        StateDeclaration mountedOn;
        mountedOn.name = "engine:test_mounted_on";
        mountedOn.category = StateCategory::Relation;
        mountedOn.owner = "engine:test_mount_system";
        mountedOn.observable = true;
        mountedOn.targets = TargetCardinality::Many;
        this->mountedOn = states.Declare(mountedOn);

        StateDeclaration inTeam;
        inTeam.name = "engine:test_in_team";
        inTeam.category = StateCategory::Relation;
        inTeam.owner = "engine:test_team_system";
        inTeam.observable = true;
        inTeam.targets = TargetCardinality::Few;
        this->inTeam = states.Declare(inTeam);

        StateDeclaration metTheGuide;
        metTheGuide.name = "engine:test_met_the_guide";
        metTheGuide.category = StateCategory::ContentFlag;
        metTheGuide.owner = "engine:test_dialogue_system";
        metTheGuide.observable = true;
        metTheGuide.lifetime = StateLifetime::Durable;
        this->metTheGuide = states.Declare(metTheGuide);

        StateDeclaration standing;
        standing.name = "engine:test_standing";
        standing.category = StateCategory::Value;
        standing.owner = "engine:test_reputation_system";
        standing.observable = true;
        this->standing = states.Declare(standing);

        StateDeclaration healthLow;
        healthLow.name = "engine:test_health_low";
        healthLow.category = StateCategory::Derived;
        healthLow.condition = [](const EntityWorld &world, Entity entity, void *user) {
            const Health *health = world.TryGet<Health>(entity);
            return health != nullptr &&
                   health->current * 100 < health->max * *static_cast<int *>(user);
        };
        healthLow.conditionUser = &lowPercent;
        this->healthLow = states.Declare(healthLow);

        states.SetFactSink(&log);
    }

    // An entity with a component, attached or not.
    Entity Spawn(bool attach = true) {
        const Entity entity = world.Create();
        world.Set(entity, Health{});
        if (attach) {
            states.Attach(entity);
        }
        return entity;
    }

    EntityWorld world;
    EntityStates states;
    FactLog log;
    int lowPercent = 25;
    StateId stunned;
    StateId mountedOn;
    StateId inTeam;
    StateId metTheGuide;
    StateId standing;
    StateId healthLow;
};

} // namespace

TEST(EntityStates, AStateOfEachCategoryIsSetReadAndCleared) {
    Fixture f;
    const Entity rider = f.Spawn();
    const Entity horse = f.Spawn();
    const Entity team = f.world.Create();

    // Nothing is set at first.
    for (const StateId state :
         {f.stunned, f.mountedOn, f.inTeam, f.metTheGuide, f.standing, f.healthLow}) {
        EXPECT_FALSE(f.states.Has(rider, state));
    }
    EXPECT_TRUE(f.states.TargetOf(rider, f.mountedOn).IsNull());
    EXPECT_EQ(f.states.ValueOf(rider, f.standing), 0);

    f.states.Set(rider, f.stunned);                // a hot flag
    f.states.SetTarget(rider, f.mountedOn, horse); // a fact with a target, many targets
    f.states.SetTarget(rider, f.inTeam, team);     // a fact with a target, few targets
    f.states.Set(rider, f.metTheGuide);            // a content flag
    f.states.SetValue(rider, f.standing, -350);    // a value
    f.world.Get<Health>(rider).current = 20;       // what the derived state is computed from

    EXPECT_TRUE(f.states.Has(rider, f.stunned));
    EXPECT_TRUE(f.states.Has(rider, f.mountedOn));
    EXPECT_EQ(f.states.TargetOf(rider, f.mountedOn), horse);
    EXPECT_EQ(f.states.TargetOf(rider, f.inTeam), team);
    EXPECT_TRUE(f.states.Has(rider, f.metTheGuide));
    EXPECT_TRUE(f.states.Has(rider, f.standing));
    EXPECT_EQ(f.states.ValueOf(rider, f.standing), -350);
    EXPECT_TRUE(f.states.Has(rider, f.healthLow));
    // Another entity is untouched.
    EXPECT_FALSE(f.states.Has(horse, f.stunned));
    EXPECT_FALSE(f.states.Has(horse, f.metTheGuide));
    EXPECT_FALSE(f.states.Has(horse, f.healthLow));

    f.states.Clear(rider, f.stunned);
    f.states.Clear(rider, f.mountedOn);
    f.states.Clear(rider, f.inTeam);
    f.states.Clear(rider, f.metTheGuide);
    f.states.Clear(rider, f.standing);
    f.world.Get<Health>(rider).current = 90;
    for (const StateId state :
         {f.stunned, f.mountedOn, f.inTeam, f.metTheGuide, f.standing, f.healthLow}) {
        EXPECT_FALSE(f.states.Has(rider, state));
    }
    EXPECT_TRUE(f.states.TargetOf(rider, f.inTeam).IsNull());
    EXPECT_EQ(f.states.ValueOf(rider, f.standing), 0);
}

TEST(EntityStates, HotAndContentFlagsNeverMoveTheEntityToAnotherTable) {
    Fixture f;
    const Entity entity = f.Spawn();
    const Entity horse = f.Spawn();
    const ArchetypeId home = f.world.ArchetypeOf(entity);

    for (int round = 0; round < 10; ++round) {
        f.states.Set(entity, f.stunned);
        EXPECT_EQ(f.world.ArchetypeOf(entity), home) << "a hot flag moved the entity";
        f.states.Set(entity, f.metTheGuide);
        EXPECT_EQ(f.world.ArchetypeOf(entity), home) << "a content flag moved the entity";
        f.states.SetValue(entity, f.standing, round);
        EXPECT_EQ(f.world.ArchetypeOf(entity), home) << "a value moved the entity";
        f.states.SetTarget(entity, f.mountedOn, horse);
        EXPECT_EQ(f.world.ArchetypeOf(entity), home) << "a relation with many targets moved it";

        f.states.Clear(entity, f.stunned);
        f.states.Clear(entity, f.metTheGuide);
        f.states.Clear(entity, f.standing);
        f.states.Clear(entity, f.mountedOn);
        EXPECT_EQ(f.world.ArchetypeOf(entity), home);
    }

    // The contrast: a relation with few targets stores entities of one target together.
    const Entity team = f.world.Create();
    f.states.SetTarget(entity, f.inTeam, team);
    EXPECT_NE(f.world.ArchetypeOf(entity), home);
    f.states.Clear(entity, f.inTeam);
    EXPECT_EQ(f.world.ArchetypeOf(entity), home);
}

TEST(EntityStates, AnEntityThatWasNotAttachedMovesOnceAtItsFirstWrite) {
    Fixture f;
    const Entity entity = f.Spawn(false);
    const ArchetypeId before = f.world.ArchetypeOf(entity);
    EXPECT_FALSE(f.states.Has(entity, f.stunned)); // reading needs no storage
    f.states.Clear(entity, f.stunned);             // clearing what is not set needs none either
    f.states.Clear(entity, f.metTheGuide);
    EXPECT_EQ(f.world.ArchetypeOf(entity), before);

    f.states.Set(entity, f.stunned);
    const ArchetypeId withFlag = f.world.ArchetypeOf(entity);
    EXPECT_NE(withFlag, before); // the flag's storage arrived
    f.states.Clear(entity, f.stunned);
    f.states.Set(entity, f.stunned);
    EXPECT_EQ(f.world.ArchetypeOf(entity), withFlag); // and from then on it stays

    f.states.Set(entity, f.metTheGuide);
    const ArchetypeId withContainer = f.world.ArchetypeOf(entity);
    EXPECT_NE(withContainer, withFlag); // the container arrived
    f.states.SetValue(entity, f.standing, 5);
    f.states.Clear(entity, f.metTheGuide);
    EXPECT_EQ(f.world.ArchetypeOf(entity), withContainer);
    EXPECT_TRUE(f.states.Has(entity, f.stunned));
    EXPECT_EQ(f.states.ValueOf(entity, f.standing), 5);
}

TEST(EntityStates, ObservableStatesReportEnteringAndLeaving) {
    Fixture f;
    const Entity rider = f.Spawn();
    const Entity horse = f.Spawn();
    const Entity mule = f.Spawn();

    f.states.Set(rider, f.stunned);
    f.states.Set(rider, f.stunned); // already set: nothing to report
    f.states.Clear(rider, f.stunned);
    f.states.Clear(rider, f.stunned);
    f.states.Set(rider, f.metTheGuide);
    f.states.SetValue(rider, f.standing, 10);
    f.states.SetValue(rider, f.standing, 25); // a new number is not entering again
    f.states.SetTarget(rider, f.mountedOn, horse);
    f.states.SetTarget(rider, f.mountedOn, horse); // the same target: nothing
    f.states.SetTarget(rider, f.mountedOn, mule);  // leaves the horse, enters the mule
    f.states.Clear(rider, f.mountedOn);
    f.states.Clear(rider, f.standing);
    f.states.Clear(rider, f.metTheGuide);

    const Entity none;
    EXPECT_EQ(f.log.facts, (Facts{
                               StateEntered{rider, f.stunned, none},
                               StateExited{rider, f.stunned, none},
                               StateEntered{rider, f.metTheGuide, none},
                               StateEntered{rider, f.standing, none},
                               StateEntered{rider, f.mountedOn, horse},
                               StateExited{rider, f.mountedOn, horse},
                               StateEntered{rider, f.mountedOn, mule},
                               StateExited{rider, f.mountedOn, mule},
                               StateExited{rider, f.standing, none},
                               StateExited{rider, f.metTheGuide, none},
                           }));
    EXPECT_EQ(f.states.ValueOf(rider, f.standing), 0);
}

TEST(EntityStates, AStateThatIsNotObservableReportsNothing) {
    Fixture f;
    StateDeclaration quiet;
    quiet.name = "engine:test_quiet";
    quiet.category = StateCategory::HotFlag;
    quiet.owner = "engine:test_system";
    const StateId quietId = f.states.Declare(quiet); // observable is false by default
    const Entity entity = f.Spawn();

    f.states.Set(entity, quietId);
    f.states.Clear(entity, quietId);
    EXPECT_TRUE(f.log.facts.empty());

    f.states.SetFactSink(nullptr); // and an observable state with no sink is simply not reported
    f.states.Set(entity, f.stunned);
    EXPECT_TRUE(f.states.Has(entity, f.stunned));
    EXPECT_TRUE(f.log.facts.empty());
}

TEST(EntityStates, ARelationEndsWhenItsTargetIsDestroyed) {
    Fixture f;
    const Entity rider = f.Spawn();
    const Entity horse = f.Spawn();
    const Entity team = f.world.Create();
    f.states.SetTarget(rider, f.mountedOn, horse);
    f.states.SetTarget(rider, f.inTeam, team);

    f.world.Destroy(horse);
    f.world.Destroy(team);

    EXPECT_TRUE(f.world.IsAlive(rider));
    EXPECT_FALSE(f.states.Has(rider, f.mountedOn));
    EXPECT_TRUE(f.states.TargetOf(rider, f.mountedOn).IsNull());
    EXPECT_FALSE(f.states.Has(rider, f.inTeam));
}

TEST(EntityStates, ADerivedStateIsComputedWhenAsked) {
    Fixture f;
    const Entity entity = f.Spawn();
    EXPECT_FALSE(f.states.Has(entity, f.healthLow));
    f.world.Get<Health>(entity).current = 24;
    EXPECT_TRUE(f.states.Has(entity, f.healthLow)); // nothing was written to make it so
    f.lowPercent = 10;                              // the rule's own parameter
    EXPECT_FALSE(f.states.Has(entity, f.healthLow));
    EXPECT_TRUE(f.log.facts.empty());
}

TEST(EntityStates, ChangesThatDoNotMoveAnEntityApplyAtOnceInsideAnIteration) {
    Fixture f;
    std::vector<Entity> entities;
    for (int index = 0; index < 50; ++index) {
        entities.push_back(f.Spawn());
    }
    const Entity horse = f.Spawn();
    const auto everyone = f.world.MakeQuery<Health>("engine:test_everyone");

    int visited = 0;
    everyone.Each([&](Entity entity, Health &) {
        ++visited;
        if (entity == horse) {
            return;
        }
        f.states.Set(entity, f.stunned);
        f.states.Set(entity, f.metTheGuide);
        f.states.SetValue(entity, f.standing, 7);
        f.states.SetTarget(entity, f.mountedOn, horse);
        // Visible at once: a system later in the same phase sees them.
        EXPECT_TRUE(f.states.Has(entity, f.stunned));
        EXPECT_TRUE(f.states.Has(entity, f.metTheGuide));
        EXPECT_EQ(f.states.ValueOf(entity, f.standing), 7);
        EXPECT_EQ(f.states.TargetOf(entity, f.mountedOn), horse);
    });
    EXPECT_EQ(visited, 51); // and nobody was visited twice or skipped
    EXPECT_EQ(f.log.facts.size(), 200u);
}

TEST(EntityStates, ChangesThatMoveAnEntityWaitForTheEndOfTheScope) {
    Fixture f;
    const Entity entity = f.Spawn(false); // no state storage yet
    const Entity team = f.world.Create();
    const Entity otherTeam = f.world.Create();
    const ArchetypeId before = f.world.ArchetypeOf(entity);
    {
        const EntityWorld::DeferScope phase(f.world);
        f.states.Set(entity, f.stunned);
        f.states.Set(entity, f.stunned); // asked twice: reported once
        f.states.Set(entity, f.metTheGuide);
        f.states.SetValue(entity, f.standing, 40);
        f.states.SetValue(entity, f.standing, 41);
        f.states.SetTarget(entity, f.inTeam, team);
        f.states.SetTarget(entity, f.inTeam, otherTeam);

        // Until the scope ends the entity has not moved and reads show the states as they were.
        EXPECT_EQ(f.world.ArchetypeOf(entity), before);
        EXPECT_FALSE(f.states.Has(entity, f.stunned));
        EXPECT_FALSE(f.states.Has(entity, f.metTheGuide));
        EXPECT_EQ(f.states.ValueOf(entity, f.standing), 0);
        EXPECT_TRUE(f.states.TargetOf(entity, f.inTeam).IsNull());
    }
    EXPECT_TRUE(f.states.Has(entity, f.stunned));
    EXPECT_TRUE(f.states.Has(entity, f.metTheGuide));
    EXPECT_EQ(f.states.ValueOf(entity, f.standing), 41);
    EXPECT_EQ(f.states.TargetOf(entity, f.inTeam), otherTeam);

    const Entity none;
    EXPECT_EQ(f.log.facts, (Facts{
                               StateEntered{entity, f.stunned, none},
                               StateEntered{entity, f.metTheGuide, none},
                               StateEntered{entity, f.standing, none},
                               StateEntered{entity, f.inTeam, team},
                               StateExited{entity, f.inTeam, team},
                               StateEntered{entity, f.inTeam, otherTeam},
                           }));

    // A later scope starts from what the world now holds.
    f.log.facts.clear();
    {
        const EntityWorld::DeferScope phase(f.world);
        f.states.Set(entity, f.stunned);  // already set: nothing
        f.states.Clear(entity, f.inTeam); // held back again, reported once
        f.states.Clear(entity, f.inTeam);
        EXPECT_EQ(f.states.TargetOf(entity, f.inTeam), otherTeam);
    }
    EXPECT_TRUE(f.states.TargetOf(entity, f.inTeam).IsNull());
    EXPECT_EQ(f.log.facts, (Facts{StateExited{entity, f.inTeam, otherTeam}}));
}

TEST(EntityStates, AnEntitySpawnedInAPhaseIsGivenItsStatesInTheSamePhase) {
    Fixture f;
    const Entity horse = f.Spawn();
    Entity spawned;
    {
        const EntityWorld::DeferScope phase(f.world);
        spawned = f.world.Create();
        f.world.Set(spawned, Health{30, 100});
        f.states.Attach(spawned);
        f.states.Set(spawned, f.stunned);
        f.states.Set(spawned, f.metTheGuide);
        f.states.SetValue(spawned, f.standing, 3);
        f.states.SetTarget(spawned, f.mountedOn, horse);
        f.states.Clear(spawned, f.metTheGuide); // set and cleared before it ever applied
    }
    EXPECT_TRUE(f.states.Has(spawned, f.stunned));
    EXPECT_FALSE(f.states.Has(spawned, f.metTheGuide));
    EXPECT_EQ(f.states.ValueOf(spawned, f.standing), 3);
    EXPECT_EQ(f.states.TargetOf(spawned, f.mountedOn), horse);
    EXPECT_EQ(f.world.Get<Health>(spawned).current, 30);

    // It has all its storage now: flipping its states does not move it.
    const ArchetypeId home = f.world.ArchetypeOf(spawned);
    f.states.Clear(spawned, f.stunned);
    f.states.Set(spawned, f.metTheGuide);
    EXPECT_EQ(f.world.ArchetypeOf(spawned), home);
}

TEST(EntityStates, AttachGivesEveryHotFlagDeclaredSoFarAndIsHarmlessTwice) {
    Fixture f;
    std::vector<StateId> flags;
    for (int index = 0; index < 20; ++index) {
        StateDeclaration flag;
        const std::string name = "engine:test_flag_" + std::to_string(index);
        flag.name = name;
        flag.category = StateCategory::HotFlag;
        flag.owner = "engine:test_system";
        flags.push_back(f.states.Declare(flag));
    }
    const Entity entity = f.Spawn();
    f.states.Attach(entity); // again: nothing changes
    const ArchetypeId home = f.world.ArchetypeOf(entity);
    f.states.Attach(entity);
    EXPECT_EQ(f.world.ArchetypeOf(entity), home);

    for (std::size_t index = 0; index < flags.size(); ++index) {
        EXPECT_FALSE(f.states.Has(entity, flags[index]));
        if (index % 2 == 0) {
            f.states.Set(entity, flags[index]);
        }
    }
    EXPECT_EQ(f.world.ArchetypeOf(entity), home);
    for (std::size_t index = 0; index < flags.size(); ++index) {
        EXPECT_EQ(f.states.Has(entity, flags[index]), index % 2 == 0);
    }
    EXPECT_TRUE(f.world.IsAlive(entity));
    EXPECT_EQ(f.world.Get<Health>(entity).max, 100);
}

TEST(EntityStates, ThousandsOfContentFlagsCostOnlyWhatAnEntityUses) {
    Fixture f;
    std::vector<StateId> flags;
    for (int index = 0; index < 5'000; ++index) {
        StateDeclaration flag;
        const std::string name = "engine:test_content_" + std::to_string(index);
        flag.name = name;
        flag.category = StateCategory::ContentFlag;
        flag.owner = "engine:test_quest_system";
        flags.push_back(f.states.Declare(flag));
    }
    const Entity entity = f.Spawn();
    const ArchetypeId home = f.world.ArchetypeOf(entity);
    f.states.Set(entity, flags[3]);
    f.states.Set(entity, flags[640]);
    f.states.Set(entity, flags[4'999]);
    f.states.SetValue(entity, f.standing, 12);

    EXPECT_EQ(f.world.ArchetypeOf(entity), home);
    EXPECT_TRUE(f.states.Has(entity, flags[640]));
    EXPECT_FALSE(f.states.Has(entity, flags[641]));
    EXPECT_TRUE(f.states.Has(entity, flags[4'999]));

    // Tools see the container as one component and how much is in it.
    std::string shown;
    f.world.ForEachComponentOf(entity, [&](const ComponentInfo &info, const void *component) {
        if (info.name == "engine:state_container") {
            ASSERT_NE(info.format, nullptr);
            info.format(component, shown);
        }
    });
    EXPECT_EQ(shown, "3 content flag(s), 1 value(s)");
}

TEST(EntityStates, StatesAreFoundByNameAndDescribed) {
    Fixture f;
    EXPECT_EQ(f.states.Count(), 6u);
    EXPECT_EQ(f.states.Find("engine:test_stunned"), f.stunned);
    EXPECT_EQ(f.states.Find("engine:test_standing"), f.standing);
    EXPECT_FALSE(f.states.Find("engine:test_unknown").IsValid());
    EXPECT_FALSE(StateId().IsValid());

    const StateInfo stunned = f.states.Describe(f.stunned);
    EXPECT_EQ(stunned.name, "engine:test_stunned");
    EXPECT_EQ(stunned.description, "Cannot act");
    EXPECT_EQ(stunned.category, StateCategory::HotFlag);
    EXPECT_EQ(stunned.owner, "engine:test_control_system");
    EXPECT_TRUE(stunned.observable);
    EXPECT_EQ(stunned.visibility, StateVisibility::Everyone);
    EXPECT_EQ(stunned.lifetime, StateLifetime::Transient);
    EXPECT_EQ(f.states.Describe(f.metTheGuide).lifetime, StateLifetime::Durable);
    EXPECT_EQ(f.states.Describe(f.inTeam).targets, TargetCardinality::Few);
    EXPECT_EQ(f.states.Describe(f.healthLow).category, StateCategory::Derived);
}

#if RUNIC_ASSERTS_ENABLED

TEST(EntityStatesDeathTest, ADeclarationIsChecked) {
    Fixture f;
    StateDeclaration again;
    again.name = "engine:test_stunned";
    again.owner = "engine:test_system";
    EXPECT_DEATH(f.states.Declare(again), "declared twice");

    StateDeclaration unnamed;
    unnamed.name = "Stunned";
    unnamed.owner = "engine:test_system";
    EXPECT_DEATH(f.states.Declare(unnamed), "namespace:name");

    StateDeclaration ownerless;
    ownerless.name = "engine:test_ownerless";
    EXPECT_DEATH(f.states.Declare(ownerless), "names its owner");

    StateDeclaration derived;
    derived.name = "engine:test_derived";
    derived.category = StateCategory::Derived;
    EXPECT_DEATH(f.states.Declare(derived), "has a condition");
}

TEST(EntityStatesDeathTest, AStateIsWrittenTheWayItsCategoryAllows) {
    Fixture f;
    const Entity entity = f.Spawn();
    EXPECT_DEATH(f.states.Set(entity, f.standing), "HotFlag or a ContentFlag");
    EXPECT_DEATH(f.states.SetValue(entity, f.stunned, 1), "only a Value");
    EXPECT_DEATH(f.states.SetTarget(entity, f.stunned, entity), "only a Relation");
    EXPECT_DEATH(f.states.Clear(entity, f.healthLow), "cannot be cleared");
    EXPECT_DEATH(f.states.SetTarget(entity, f.mountedOn, Entity()), "target must be alive");
}

#endif // RUNIC_ASSERTS_ENABLED
