#include "viewmodels/validation/RuleValidatorRegistry.h"

#include <gtest/gtest.h>
#include <QSet>

using namespace OpenC3::ViewModels::Validation;

TEST(RuleValidatorRegistryTest, AllReturnsANonEmptyStableList)
{
    const auto& all = RuleValidatorRegistry::all();
    EXPECT_FALSE(all.isEmpty());
    // Calling all() twice must return the same function-local-static list.
    EXPECT_EQ(&all, &RuleValidatorRegistry::all());
}

TEST(RuleValidatorRegistryTest, EveryValidatorHasAUniqueNonEmptyId)
{
    QSet<QString> ids;
    for (const auto* v : RuleValidatorRegistry::all()) {
        ASSERT_NE(v, nullptr);
        EXPECT_FALSE(v->id().isEmpty());
        EXPECT_FALSE(ids.contains(v->id())) << v->id().toStdString() << " is registered twice";
        ids.insert(v->id());
    }
}

TEST(RuleValidatorRegistryTest, EveryValidatorHasANonEmptyLabel)
{
    for (const auto* v : RuleValidatorRegistry::all())
        EXPECT_FALSE(v->label().isEmpty()) << v->id().toStdString() << " has no label";
}

TEST(RuleValidatorRegistryTest, ByIdFindsEveryRegisteredValidatorByItsOwnId)
{
    for (const auto* v : RuleValidatorRegistry::all())
        EXPECT_EQ(RuleValidatorRegistry::byId(v->id()), v);
}

TEST(RuleValidatorRegistryTest, ByIdReturnsNullptrForUnknownId)
{
    EXPECT_EQ(RuleValidatorRegistry::byId("not-a-real-validator-id"), nullptr);
    EXPECT_EQ(RuleValidatorRegistry::byId(""), nullptr);
}
