#include <algorithm>
#include <cstdio>
#include <stdint.h>

#include "config.h"
#include "level_generator.h"

namespace {

uint64_t hash_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * 1099511628211ULL;
}

uint64_t board_hash(const LevelLayout &layout)
{
    uint64_t hash = 1469598103934665603ULL;
    for (uint8_t cell = 0U; cell < LEVEL_CELL_COUNT; ++cell) {
        hash = hash_byte(hash, layout.terrain[cell]);
    }
    hash = hash_byte(hash, layout.player_x);
    hash = hash_byte(hash, layout.player_y);
    hash = hash_byte(hash, layout.crate_count);
    uint8_t crates[MAX_CRATES] = {};
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        crates[crate] = level_cell_index(layout.crate_x[crate], layout.crate_y[crate]);
    }
    std::sort(crates, crates + layout.crate_count);
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        hash = hash_byte(hash, crates[crate]);
    }
    return hash;
}

bool is_solved(const LevelLayout &layout)
{
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (layout.terrain[level_cell_index(
                layout.crate_x[crate], layout.crate_y[crate])] != TERRAIN_TARGET) {
            return false;
        }
    }
    return true;
}

uint8_t count_targets(const LevelLayout &layout)
{
    uint8_t targets = 0U;
    for (uint8_t cell = 0U; cell < LEVEL_CELL_COUNT; ++cell) {
        if (layout.terrain[cell] == TERRAIN_TARGET) {
            ++targets;
        }
    }
    return targets;
}

uint8_t expected_crates(uint16_t level)
{
    if (level < 5U) return 2U;
    if (level < 10U) return 3U;
    return level < 40U ? 4U : level < 70U ? 5U : 6U;
}

uint8_t expected_walls(uint16_t level)
{
    if (level < 5U) return 0U;
    if (level < 10U) {
        return static_cast<uint8_t>(6U + (level - 5U) * 2U);
    }
    return static_cast<uint8_t>(
        20U + static_cast<uint32_t>(level - 10U) * 14UL / 89UL);
}

uint8_t expected_pulls(uint16_t level)
{
    if (level < 5U) return static_cast<uint8_t>(level + 2U);
    if (level < 10U) {
        return static_cast<uint8_t>(15U + (level - 5U) * 5U);
    }
    return static_cast<uint8_t>(
        70U + static_cast<uint32_t>(level - 10U) * 70UL / 89UL);
}

uint8_t required_distance(uint16_t level)
{
    if (level < 5U) return static_cast<uint8_t>(1U + level / 2U);
    if (level < 10U) {
        return static_cast<uint8_t>(8U + (level - 5U) * 2U);
    }
    return static_cast<uint8_t>(
        18U + static_cast<uint32_t>(level - 10U) * 14UL / 89UL);
}

uint8_t required_turns(uint16_t level)
{
    if (level < 5U) return 0U;
    if (level < 10U) {
        return static_cast<uint8_t>(4U + (level - 5U) * 3U);
    }
    return static_cast<uint8_t>(
        18U + static_cast<uint32_t>(level - 10U) * 30UL / 89UL);
}

uint8_t required_moved_crates(uint16_t level)
{
    if (level < 2U) return 1U;
    return expected_crates(level);
}

void print_metrics(uint16_t level, const LevelLayout &layout)
{
    std::printf(
        "level %u: crates %u, walls %u, pulls %u, lower bound %u, turns %u\n",
        level + 1U, layout.crate_count, layout.internal_walls,
        layout.scramble_pulls, layout.minimum_push_distance,
        layout.scramble_turns);
}

#ifdef PRINT_FINAL_BOARD
void print_board(const LevelLayout &layout)
{
    for (uint8_t y = 0U; y < LEVEL_ROWS; ++y) {
        for (uint8_t x = 0U; x < LEVEL_COLS; ++x) {
            const bool target = layout.terrain[level_cell_index(x, y)] ==
                TERRAIN_TARGET;
            char symbol = layout.terrain[level_cell_index(x, y)] ==
                TERRAIN_WALL ? '#' : target ? '.' : ' ';
            for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
                if (layout.crate_x[crate] == x && layout.crate_y[crate] == y) {
                    symbol = target ? '*' : '$';
                }
            }
            if (layout.player_x == x && layout.player_y == y) {
                symbol = target ? '+' : '@';
            }
            std::putchar(symbol);
        }
        std::putchar('\n');
    }
}
#endif

}  // namespace

int main()
{
    uint64_t hashes[LEVEL_COUNT];
#ifdef DUMP_RETRY_RECIPES
    uint8_t recipes[LEVEL_COUNT];
#endif
    uint16_t minimum_pulls = 0xFFFFU;
    uint16_t maximum_pulls = 0U;
    uint8_t maximum_retry = 0U;
    uint16_t maximum_retry_level = 0U;
    uint32_t beginner_distance = 0UL;
    uint32_t medium_distance = 0UL;
    uint32_t expert_distance = 0UL;
    uint32_t expert_turns = 0UL;

    for (uint16_t level = 0U; level < LEVEL_COUNT; ++level) {
        LevelLayout layout;
        if (!level_generate(level, &layout)) {
            std::fprintf(stderr, "generation failed for level %u\n", level + 1U);
            return 1;
        }
#ifdef DUMP_RETRY_RECIPES
        recipes[level] = layout.generation_retry;
#endif
        if (!level_entities_are_valid(layout) || is_solved(layout) ||
            count_targets(layout) != layout.crate_count) {
            std::fprintf(stderr, "invalid level %u\n", level + 1U);
            return 1;
        }
        if (layout.crate_count != expected_crates(level) ||
            layout.internal_walls != expected_walls(level) ||
            layout.scramble_pulls != expected_pulls(level) ||
            layout.moved_crates < required_moved_crates(level) ||
            layout.minimum_push_distance < required_distance(level) ||
            layout.scramble_turns < required_turns(level)) {
            std::fprintf(
                stderr,
                "difficulty contract failed at level %u: C%u W%u P%u M%u D%u T%u\n",
                level + 1U, layout.crate_count, layout.internal_walls,
                layout.scramble_pulls, layout.moved_crates,
                layout.minimum_push_distance, layout.scramble_turns);
            return 1;
        }
#if !defined(REBUILD_RETRY_RECIPES)
        if (layout.generation_attempts != 1U) {
            std::fprintf(stderr, "stale retry recipe at level %u\n", level + 1U);
            return 1;
        }
#endif
        if (level < 5U &&
            (layout.internal_walls != 0U ||
             layout.minimum_push_distance > layout.scramble_pulls)) {
            std::fprintf(stderr, "beginner level %u is too difficult\n", level + 1U);
            return 1;
        }

        hashes[level] = board_hash(layout);
        for (uint16_t previous = 0U; previous < level; ++previous) {
            if (hashes[level] == hashes[previous]) {
                std::fprintf(stderr, "duplicate boards at levels %u and %u\n",
                             previous + 1U, level + 1U);
                return 1;
            }
        }
        minimum_pulls = std::min(minimum_pulls, layout.scramble_pulls);
        maximum_pulls = std::max(maximum_pulls, layout.scramble_pulls);
        if (layout.generation_retry > maximum_retry) {
            maximum_retry = layout.generation_retry;
            maximum_retry_level = level;
        }
        if (level < 5U) {
            beginner_distance += layout.minimum_push_distance;
        } else if (level < 10U) {
            medium_distance += layout.minimum_push_distance;
        } else {
            expert_distance += layout.minimum_push_distance;
            expert_turns += layout.scramble_turns;
        }
        if (level < 5U || level == 5U || level == 9U || level == 10U ||
            level == 49U || level == 99U) {
            print_metrics(level, layout);
        }
    }

    if (medium_distance / 5UL <= beginner_distance / 5UL ||
        expert_distance / 90UL <= medium_distance / 5UL) {
        std::fprintf(stderr, "difficulty-band averages did not rise\n");
        return 1;
    }

    LevelLayout final_level;
    if (!level_generate(LEVEL_COUNT - 1U, &final_level) ||
        final_level.crate_count != 6U || final_level.internal_walls != 34U ||
        final_level.scramble_pulls != 140U ||
        final_level.minimum_push_distance < 32U ||
        final_level.scramble_turns < 48U || final_level.moved_crates != 6U) {
        std::fprintf(stderr, "level 100 does not meet the super-expert contract\n");
        return 1;
    }

    std::printf(
        "validated %u unique levels; pulls %u..%u; largest recipe %u at level %u\n",
        LEVEL_COUNT, minimum_pulls, maximum_pulls, maximum_retry,
        maximum_retry_level + 1U);
    std::printf(
        "average lower bound: beginner %.2f, medium %.2f, super expert %.2f\n",
        beginner_distance / 5.0, medium_distance / 5.0,
        expert_distance / 90.0);
    std::printf("super-expert average turns: %.2f\n", expert_turns / 90.0);

#ifdef PRINT_FINAL_BOARD
    std::printf("level 100 board:\n");
    print_board(final_level);
#endif
#ifdef DUMP_RETRY_RECIPES
    std::printf("RETRY_RECIPES_BEGIN\n");
    for (uint16_t level = 0U; level < LEVEL_COUNT; ++level) {
        std::printf("%u%s", recipes[level],
                    level + 1U == LEVEL_COUNT ? "\n" : ",");
    }
    std::printf("RETRY_RECIPES_END\n");
#endif
    return 0;
}
