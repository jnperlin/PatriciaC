// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design) / unit testing
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
#include "cpatricia_set.h"
#include "helper_build_tree.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>


static PatriciaSetT map;

void setUp(void)
{
    patriset_init(&map);
}
void tearDown(void)
{
    patriset_fini(&map);
}

static const char *const names[] = {
    "evenly", "even",
    "acornix",   "banquetor", "cascadeum", "emberlyn",    "falconet",  "harborin",   "junctiona", "keystoner",
    "forgewin",  "gullymar",  "hollowet",  "isletorn",    "jesterin",  "kilnaris",   "ledgerox",  "mosaicor",
    "lanternis", "meadowen",  "nectaros",  "opalith",     "quiveron",  "rippletar",  "sagelynn",  "tundravel",
    "venturex",  "willowen",  "yonderix",  "zephyran",    "bristleno", "cobblethor", "duskmire",  "elmshade",
    "frostelle", "glimmeron", "harvestra", "inkwellor",   "jigsawen",  "kindleth",   "loomaris",  "mirthan",
    "noblewen",  "outpostel", "parlorin",  "quartzor",    "rangelyn",  "solacium",   "thicketra", "umberon",
    "vesselith", "wanderix",  "yarnivar",  "zestarin",    "beaconyr",  "cradlenor",  "driftona",  "emberlyx",
    "notchwyn",  "orchardel", "paddlora",  "quillex",     "ravineth",  "shelterox",  "timberan",  "upliftor",
    "vigilen",   "wharflyn",  "yearlinga", "zodiacor",    "boulderis", "cupolath",   "dewfallor", "eskerin",
    "flintar",   "grovelin",  "harpset",   "ivoryon",     "juniperix", "kettlorn",   "latchora",  "masonel",
    "nectaryx",  "ospreylin", "picketra",  "quaynor",     "reliceth",  "spindleon",  "troughel",  "uplandar",
    "verityon",  "wicketra",  "yokelan",   "zigzagor",    "brambleet", "cairnon",    "dormantix", "figmentor",
    "glistenar", "huskell",   "lumenar",   "muddlex",
     NULL
};

static void val_reset(PTSetNodeT *node)
{
    node->lcount = 0;
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->bpos > node->bpos)
            val_reset(node->_m_child[i]);
}

static void val_count(PTSetNodeT *node)
{
    ++node->lcount;
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->bpos > node->bpos)
            val_count(node->_m_child[i]);
        else
            ++node->_m_child[i]->lcount;
}

static void val_check(PTSetNodeT *node)
{
    TEST_ASSERT_EQUAL(2, node->lcount);
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->bpos > node->bpos)
            val_check(node->_m_child[i]);
}

static void validate(PTSetNodeT *node)
{
    val_reset(node);
    val_count(node);
    node->lcount -= 1; // We entered the root from the outside -- remove 1 ref!
    val_check(node);
}

static void test_insert(void)
{
    unsigned idx;
    bool     ins;

    for (idx = 0; names[idx]; ++idx) {
        const PTSetNodeT *np = patriset_insert(&map, names[idx], str2bits(names[idx]), &ins);
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_TRUE(ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        const PTSetNodeT *np = patriset_insert(&map, names[idx], str2bits(names[idx]), &ins);
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_FALSE(ins);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
}

static void test_lookup(void)
{
    unsigned idx;
    bool ins;
    const PTSetNodeT *np;
    char buf[64];

    for (idx = 0; names[idx]; ++idx) {
        (void)patriset_insert(&map, names[idx], str2bits(names[idx]), &ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        np = patriset_lookup(&map, names[idx], str2bits(names[idx]));
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
    for (idx = 0; names[idx]; ++idx) {
        snprintf(buf, sizeof(buf), "%sXX", names[idx]);
        TEST_ASSERT_NULL(patriset_lookup(&map, buf, str2bits(buf)));
    }
}

static void test_prefix(void)
{
    unsigned idx;
    bool ins;
    const PTSetNodeT *np;
    char buf[64];

    for (idx = 0; names[idx]; ++idx) {
        (void)patriset_insert(&map, names[idx], str2bits(names[idx]), &ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        snprintf(buf, sizeof(buf), "%sXX", names[idx]);
        np = patriset_prefix(&map, buf, str2bits(buf));
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
}

static void test_delete(void)
{
    unsigned idx;
    bool ins;

    for (idx = 0; names[idx]; ++idx) {
        (void)patriset_insert(&map, names[idx], str2bits(names[idx]),&ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        TEST_ASSERT_TRUE(patriset_remove(&map, names[idx], str2bits(names[idx])));
        validate(map._m_root);
        TEST_ASSERT_NULL(patriset_lookup(&map, names[idx], str2bits(names[idx])));
        for (int jdx = idx + 1; names[jdx]; ++jdx) {
            TEST_ASSERT_NOT_NULL(patriset_lookup(&map, names[jdx], str2bits(names[jdx])));
        }
    }
}

static void test_dotgen(void)
{
    unsigned idx;
    bool ins;
    FILE *ofp;

    for (idx = 0; names[idx]; ++idx) {
        (void)patriset_insert(&map, names[idx], str2bits(names[idx]), &ins);
    }
    validate(map._m_root);

    ofp = fopen("tree.dot", "w");
    TEST_ASSERT_NOT_NULL(ofp);
    patriset_todot(ofp, &map, NULL);
    fclose(ofp);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_insert);
    RUN_TEST(test_lookup);
    RUN_TEST(test_prefix);
    RUN_TEST(test_delete);
    RUN_TEST(test_dotgen);
    return UNITY_END();
}
