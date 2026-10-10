#include "features/inspection/EnglishSearch.h"
void check(bool, char const*);
void englishSearchTests() {
    using lamium::inspection::englishSearch::matches;
    check(matches("plank", "Oak Planks", "minecraft:oak_planks") && matches("OAK pl", "Oak Planks", "minecraft:oak_planks"),
          "words match the English name ignoring case");
    check(matches("oak_planks", "Oak Planks", "minecraft:oak_planks") && matches("planks oak", "Oak Planks", ""),
          "the identifier and word order both work");
    check(!matches("birch", "Oak Planks", "minecraft:oak_planks") && !matches("  ", "Oak Planks", "x"),
          "missing words and empty queries do not match");
    check(!matches("minecraft", "Stone", "minecraft:stone"), "the namespace is not searched");
    check(!matches("\xe6\x9c\xa8", "Oak Planks", "minecraft:oak_planks"), "Japanese text never matches English");
}
