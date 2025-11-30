// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design) / unit testing
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
#include "cpatricia.h"
#include "helper_build_tree.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>


static PatriciaMapT map;

void setUp(void)
{
    patricia_init(&map);
}
void tearDown(void)
{
    patricia_fini(&map);
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

static void val_reset(PTMapNodeT *node)
{
    node->payload = 0;
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->_m_bpos > node->_m_bpos)
            val_reset(node->_m_child[i]);
}

static void val_count(PTMapNodeT *node)
{
    ++node->payload;
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->_m_bpos > node->_m_bpos)
            val_count(node->_m_child[i]);
        else
            ++node->_m_child[i]->payload;
}

static void val_check(PTMapNodeT *node)
{
    TEST_ASSERT_EQUAL(2, node->payload);
    for (int i = 0; i < 2; ++i)
        if (node->_m_child[i]->_m_bpos > node->_m_bpos)
            val_check(node->_m_child[i]);
}

static void validate(PTMapNodeT *node)
{
    val_reset(node);
    val_count(node);
    node->payload -= 1; // We entered the root from the outside -- remove 1 ref!
    val_check(node);
}

static void test_insert(void)
{
    unsigned idx;
    bool     ins;

    for (idx = 0; names[idx]; ++idx) {
        const PTMapNodeT *np = patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_TRUE(ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        const PTMapNodeT *np = patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_FALSE(ins);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
}

static void test_lookup(void)
{
    unsigned idx;
    bool ins;
    const PTMapNodeT *np;
    char buf[64];

    for (idx = 0; names[idx]; ++idx) {
        (void)patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        np = patricia_lookup(&map, names[idx], str2bits(names[idx]));
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
    for (idx = 0; names[idx]; ++idx) {
        snprintf(buf, sizeof(buf), "%sXX", names[idx]);
        TEST_ASSERT_NULL(patricia_lookup(&map, buf, str2bits(buf)));
    }
}

static void test_prefix(void)
{
    unsigned idx;
    bool ins;
    const PTMapNodeT *np;
    char buf[64];

    for (idx = 0; names[idx]; ++idx) {
        (void)patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        snprintf(buf, sizeof(buf), "%sXX", names[idx]);
        np = patricia_prefix(&map, buf, str2bits(buf));
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_EQUAL_STRING(names[idx], np->data);
    }
}

static void test_delete(void)
{
    unsigned idx;
    bool ins;

    for (idx = 0; names[idx]; ++idx) {
        (void)patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
    }
    validate(map._m_root);

    for (idx = 0; names[idx]; ++idx) {
        TEST_ASSERT_TRUE(patricia_remove(&map, names[idx], str2bits(names[idx]), NULL));
        validate(map._m_root);
        TEST_ASSERT_NULL(patricia_lookup(&map, names[idx], str2bits(names[idx])));
        for (int jdx = idx + 1; names[jdx]; ++jdx) {
            TEST_ASSERT_NOT_NULL(patricia_lookup(&map, names[jdx], str2bits(names[jdx])));
        }
    }
}

static void test_dotgen(void)
{
    unsigned idx;
    bool ins;
    FILE *ofp;

    for (idx = 0; names[idx]; ++idx) {
        (void)patricia_insert(&map, names[idx], str2bits(names[idx]), idx, &ins);
    }
    validate(map._m_root);

    ofp = fopen("tree.dot", "w");
    TEST_ASSERT_NOT_NULL(ofp);
    patricia_todot(ofp, &map, NULL);
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