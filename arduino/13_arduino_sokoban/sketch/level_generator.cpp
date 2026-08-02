#include "level_generator.h"

#include <Arduino.h>
#include <string.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#else
#define PROGMEM
#endif

namespace {

constexpr int8_t DIRECTION_X[4] = {0, 1, 0, -1};
constexpr int8_t DIRECTION_Y[4] = {-1, 0, 1, 0};
constexpr uint8_t NO_DIRECTION = 0xFFU;
constexpr uint8_t MAX_PULL_CANDIDATES = MAX_CRATES * 4U;
constexpr uint8_t VISITED_BYTES =
    static_cast<uint8_t>((LEVEL_CELL_COUNT + 7U) / 8U);

// Each byte selects the first generator seed already validated by the host
// test for that level. This compact flash table avoids retrying dozens of
// rejected expert candidates on the 16 MHz target. The fallback loop below
// still makes generation robust if the algorithm is deliberately changed.
const uint8_t RETRY_RECIPES[] PROGMEM = {
    0,0,0,0,0,0,0,0,0,1,1,1,1,0,1,0,0,0,0,5,
    0,1,3,0,10,4,0,5,0,1,2,1,7,7,0,3,0,3,3,7,
    1,1,10,0,7,4,9,3,0,6,0,4,1,16,8,0,8,2,4,3,
    4,8,15,9,13,3,20,19,34,63,0,6,32,25,1,12,26,6,26,7,
    5,37,49,34,43,23,10,10,13,55,38,75,45,0,98,44,14,4,231,13
};
static_assert(sizeof(RETRY_RECIPES) == LEVEL_COUNT,
              "retry recipe count must match LEVEL_COUNT");

uint8_t retry_recipe(uint16_t level_index)
{
#if defined(__AVR__)
    return pgm_read_byte(&RETRY_RECIPES[level_index]);
#else
    return RETRY_RECIPES[level_index];
#endif
}

struct DifficultyProfile {
    uint8_t crate_count;
    uint8_t wall_cells;
    uint8_t desired_pulls;
    uint8_t required_moved_crates;
    uint8_t minimum_push_distance;
    uint8_t minimum_turns;
};

struct PullCandidate {
    uint8_t crate;
    uint8_t direction;
    uint8_t crate_x;
    uint8_t crate_y;
    uint8_t player_x;
    uint8_t player_y;
    int16_t score;
};

uint32_t random_state = 1UL;
uint8_t search_queue[LEVEL_CELL_COUNT];
uint8_t search_seen[LEVEL_CELL_COUNT];
uint8_t target_x[MAX_CRATES];
uint8_t target_y[MAX_CRATES];
uint8_t pulls_by_crate[MAX_CRATES];
uint8_t last_pull_direction[MAX_CRATES];
uint8_t crate_visited[MAX_CRATES][VISITED_BYTES];

bool in_bounds(int8_t x, int8_t y)
{
    return x >= 0 && x < static_cast<int8_t>(LEVEL_COLS) &&
           y >= 0 && y < static_cast<int8_t>(LEVEL_ROWS);
}

uint32_t next_random()
{
    random_state ^= random_state << 13U;
    random_state ^= random_state >> 17U;
    random_state ^= random_state << 5U;
    return random_state;
}

uint8_t random_below(uint8_t limit)
{
    return limit == 0U ? 0U : static_cast<uint8_t>(next_random() % limit);
}

uint8_t absolute_difference(uint8_t first, uint8_t second)
{
    return first > second ? static_cast<uint8_t>(first - second) :
                            static_cast<uint8_t>(second - first);
}

uint8_t manhattan(uint8_t first_x, uint8_t first_y,
                  uint8_t second_x, uint8_t second_y)
{
    return static_cast<uint8_t>(absolute_difference(first_x, second_x) +
                                absolute_difference(first_y, second_y));
}

DifficultyProfile difficulty_for_level(uint16_t level_index)
{
    DifficultyProfile profile;
    if (level_index < 5U) {
        // The opening five levels teach walking, pushing, and positioning in
        // an open room before internal wall structures are introduced.
        profile.crate_count = 2U;
        profile.wall_cells = 0U;
        profile.desired_pulls = static_cast<uint8_t>(level_index + 2U);
        profile.required_moved_crates = level_index < 2U ? 1U : 2U;
        profile.minimum_push_distance =
            static_cast<uint8_t>(1U + level_index / 2U);
        profile.minimum_turns = 0U;
        return profile;
    }

    if (level_index < 10U) {
        const uint8_t medium = static_cast<uint8_t>(level_index - 5U);
        profile.crate_count = 3U;
        profile.wall_cells = static_cast<uint8_t>(6U + medium * 2U);
        profile.desired_pulls = static_cast<uint8_t>(15U + medium * 5U);
        profile.required_moved_crates = 3U;
        profile.minimum_push_distance = static_cast<uint8_t>(8U + medium * 2U);
        profile.minimum_turns = static_cast<uint8_t>(4U + medium * 3U);
        return profile;
    }

    // Levels 11-100 are intentionally expert from the start. Later expert
    // levels add two more crates and continue raising every construction
    // floor rather than slowly repeating beginner-sized arrangements.
    const uint16_t expert = static_cast<uint16_t>(level_index - 10U);
    profile.crate_count = level_index < 40U ? 4U :
                          level_index < 70U ? 5U : 6U;
    profile.wall_cells = static_cast<uint8_t>(
        20U + static_cast<uint32_t>(expert) * 14UL / 89UL);
    profile.desired_pulls = static_cast<uint8_t>(
        70U + static_cast<uint32_t>(expert) * 70UL / 89UL);
    profile.required_moved_crates = profile.crate_count;
    profile.minimum_push_distance = static_cast<uint8_t>(
        18U + static_cast<uint32_t>(expert) * 14UL / 89UL);
    profile.minimum_turns = static_cast<uint8_t>(
        18U + static_cast<uint32_t>(expert) * 30UL / 89UL);
    return profile;
}

bool has_crate(const LevelLayout &layout, uint8_t x, uint8_t y)
{
    for (uint8_t index = 0U; index < layout.crate_count; ++index) {
        if (layout.crate_x[index] == x && layout.crate_y[index] == y) {
            return true;
        }
    }
    return false;
}

bool is_open_cell(const LevelLayout &layout, int8_t x, int8_t y)
{
    return in_bounds(x, y) &&
           layout.terrain[level_cell_index(
               static_cast<uint8_t>(x), static_cast<uint8_t>(y))] !=
               TERRAIN_WALL &&
           !has_crate(layout, static_cast<uint8_t>(x), static_cast<uint8_t>(y));
}

bool player_can_reach(const LevelLayout &layout, uint8_t start_x,
                      uint8_t start_y, uint8_t destination_x,
                      uint8_t destination_y)
{
    if (start_x == destination_x && start_y == destination_y) {
        return true;
    }
    memset(search_seen, 0, sizeof(search_seen));
    uint8_t read_position = 0U;
    uint8_t write_position = 0U;
    const uint8_t start = level_cell_index(start_x, start_y);
    search_queue[write_position++] = start;
    search_seen[start] = 1U;

    while (read_position < write_position) {
        const uint8_t cell = search_queue[read_position++];
        const uint8_t x = static_cast<uint8_t>(cell % LEVEL_COLS);
        const uint8_t y = static_cast<uint8_t>(cell / LEVEL_COLS);
        for (uint8_t direction = 0U; direction < 4U; ++direction) {
            const int8_t next_x = static_cast<int8_t>(x + DIRECTION_X[direction]);
            const int8_t next_y = static_cast<int8_t>(y + DIRECTION_Y[direction]);
            if (!is_open_cell(layout, next_x, next_y)) {
                continue;
            }
            if (next_x == static_cast<int8_t>(destination_x) &&
                next_y == static_cast<int8_t>(destination_y)) {
                return true;
            }
            const uint8_t next = level_cell_index(
                static_cast<uint8_t>(next_x), static_cast<uint8_t>(next_y));
            if (search_seen[next] == 0U) {
                search_seen[next] = 1U;
                search_queue[write_position++] = next;
            }
        }
    }
    return false;
}

bool terrain_is_connected(const LevelLayout &layout)
{
    uint8_t floor_count = 0U;
    uint8_t start = 0U;
    for (uint8_t cell = 0U; cell < LEVEL_CELL_COUNT; ++cell) {
        if (layout.terrain[cell] != TERRAIN_WALL) {
            if (floor_count == 0U) {
                start = cell;
            }
            ++floor_count;
        }
    }
    if (floor_count == 0U) {
        return false;
    }

    memset(search_seen, 0, sizeof(search_seen));
    uint8_t read_position = 0U;
    uint8_t write_position = 0U;
    search_queue[write_position++] = start;
    search_seen[start] = 1U;
    while (read_position < write_position) {
        const uint8_t cell = search_queue[read_position++];
        const uint8_t x = static_cast<uint8_t>(cell % LEVEL_COLS);
        const uint8_t y = static_cast<uint8_t>(cell / LEVEL_COLS);
        for (uint8_t direction = 0U; direction < 4U; ++direction) {
            const int8_t next_x = static_cast<int8_t>(x + DIRECTION_X[direction]);
            const int8_t next_y = static_cast<int8_t>(y + DIRECTION_Y[direction]);
            if (!in_bounds(next_x, next_y)) {
                continue;
            }
            const uint8_t next = level_cell_index(
                static_cast<uint8_t>(next_x), static_cast<uint8_t>(next_y));
            if (search_seen[next] == 0U &&
                layout.terrain[next] != TERRAIN_WALL) {
                search_seen[next] = 1U;
                search_queue[write_position++] = next;
            }
        }
    }
    return write_position == floor_count;
}

bool create_terrain(const DifficultyProfile &profile, LevelLayout *layout)
{
    for (uint8_t y = 0U; y < LEVEL_ROWS; ++y) {
        for (uint8_t x = 0U; x < LEVEL_COLS; ++x) {
            const bool border = x == 0U || y == 0U ||
                x + 1U == LEVEL_COLS || y + 1U == LEVEL_ROWS;
            layout->terrain[level_cell_index(x, y)] =
                border ? TERRAIN_WALL : TERRAIN_FLOOR;
        }
    }

    uint8_t placed = 0U;
    for (uint8_t attempt = 0U;
         attempt < 160U && placed < profile.wall_cells;
         ++attempt) {
        const bool horizontal = random_below(2U) == 0U;
        const uint8_t remaining =
            static_cast<uint8_t>(profile.wall_cells - placed);
        uint8_t length = static_cast<uint8_t>(2U + random_below(3U));
        if (length > remaining) {
            length = remaining;
        }
        if (length == 0U) {
            break;
        }

        const uint8_t span = horizontal ? LEVEL_COLS - 2U : LEVEL_ROWS - 2U;
        if (length > span) {
            continue;
        }
        const uint8_t start_x = horizontal ?
            static_cast<uint8_t>(1U + random_below(span - length + 1U)) :
            static_cast<uint8_t>(1U + random_below(LEVEL_COLS - 2U));
        const uint8_t start_y = horizontal ?
            static_cast<uint8_t>(1U + random_below(LEVEL_ROWS - 2U)) :
            static_cast<uint8_t>(1U + random_below(span - length + 1U));

        bool available = true;
        for (uint8_t offset = 0U; offset < length; ++offset) {
            const uint8_t x = static_cast<uint8_t>(
                start_x + (horizontal ? offset : 0U));
            const uint8_t y = static_cast<uint8_t>(
                start_y + (horizontal ? 0U : offset));
            if (layout->terrain[level_cell_index(x, y)] != TERRAIN_FLOOR) {
                available = false;
                break;
            }
        }
        if (!available) {
            continue;
        }

        for (uint8_t offset = 0U; offset < length; ++offset) {
            const uint8_t x = static_cast<uint8_t>(
                start_x + (horizontal ? offset : 0U));
            const uint8_t y = static_cast<uint8_t>(
                start_y + (horizontal ? 0U : offset));
            layout->terrain[level_cell_index(x, y)] = TERRAIN_WALL;
        }
        if (!terrain_is_connected(*layout)) {
            for (uint8_t offset = 0U; offset < length; ++offset) {
                const uint8_t x = static_cast<uint8_t>(
                    start_x + (horizontal ? offset : 0U));
                const uint8_t y = static_cast<uint8_t>(
                    start_y + (horizontal ? 0U : offset));
                layout->terrain[level_cell_index(x, y)] = TERRAIN_FLOOR;
            }
            continue;
        }
        placed = static_cast<uint8_t>(placed + length);
    }
    layout->internal_walls = placed;
    return placed == profile.wall_cells;
}

bool target_can_be_pulled_from(const LevelLayout &layout, uint8_t x, uint8_t y)
{
    for (uint8_t direction = 0U; direction < 4U; ++direction) {
        const int8_t stance_x = static_cast<int8_t>(x - DIRECTION_X[direction]);
        const int8_t stance_y = static_cast<int8_t>(y - DIRECTION_Y[direction]);
        const int8_t back_x = static_cast<int8_t>(
            x - static_cast<int8_t>(2 * DIRECTION_X[direction]));
        const int8_t back_y = static_cast<int8_t>(
            y - static_cast<int8_t>(2 * DIRECTION_Y[direction]));
        if (in_bounds(stance_x, stance_y) && in_bounds(back_x, back_y) &&
            layout.terrain[level_cell_index(
                static_cast<uint8_t>(stance_x),
                static_cast<uint8_t>(stance_y))] != TERRAIN_WALL &&
            layout.terrain[level_cell_index(
                static_cast<uint8_t>(back_x),
                static_cast<uint8_t>(back_y))] != TERRAIN_WALL) {
            return true;
        }
    }
    return false;
}

bool target_is_separated(uint8_t placed_targets, uint8_t x, uint8_t y)
{
    for (uint8_t target = 0U; target < placed_targets; ++target) {
        if (manhattan(target_x[target], target_y[target], x, y) < 2U) {
            return false;
        }
    }
    return true;
}

bool place_targets(LevelLayout *layout)
{
    for (uint8_t target = 0U; target < layout->crate_count; ++target) {
        bool placed = false;
        for (uint8_t attempt = 0U; attempt < 160U; ++attempt) {
            const uint8_t x = static_cast<uint8_t>(
                1U + random_below(LEVEL_COLS - 2U));
            const uint8_t y = static_cast<uint8_t>(
                1U + random_below(LEVEL_ROWS - 2U));
            const uint8_t cell = level_cell_index(x, y);
            if (layout->terrain[cell] != TERRAIN_FLOOR ||
                !target_can_be_pulled_from(*layout, x, y) ||
                !target_is_separated(target, x, y)) {
                continue;
            }
            layout->terrain[cell] = TERRAIN_TARGET;
            layout->crate_x[target] = x;
            layout->crate_y[target] = y;
            target_x[target] = x;
            target_y[target] = y;
            placed = true;
            break;
        }
        if (!placed) {
            return false;
        }
    }
    return true;
}

bool place_player(LevelLayout *layout)
{
    for (uint8_t attempt = 0U; attempt < 160U; ++attempt) {
        const uint8_t x = static_cast<uint8_t>(
            1U + random_below(LEVEL_COLS - 2U));
        const uint8_t y = static_cast<uint8_t>(
            1U + random_below(LEVEL_ROWS - 2U));
        if (is_open_cell(*layout, static_cast<int8_t>(x),
                         static_cast<int8_t>(y))) {
            layout->player_x = x;
            layout->player_y = y;
            return true;
        }
    }
    return false;
}

bool was_visited(uint8_t crate, uint8_t x, uint8_t y)
{
    const uint8_t cell = level_cell_index(x, y);
    return (crate_visited[crate][cell / 8U] &
            static_cast<uint8_t>(1U << (cell % 8U))) != 0U;
}

void mark_visited(uint8_t crate, uint8_t x, uint8_t y)
{
    const uint8_t cell = level_cell_index(x, y);
    crate_visited[crate][cell / 8U] |=
        static_cast<uint8_t>(1U << (cell % 8U));
}

uint8_t adjacent_wall_count(const LevelLayout &layout, uint8_t x, uint8_t y)
{
    uint8_t count = 0U;
    for (uint8_t direction = 0U; direction < 4U; ++direction) {
        const int8_t next_x = static_cast<int8_t>(x + DIRECTION_X[direction]);
        const int8_t next_y = static_cast<int8_t>(y + DIRECTION_Y[direction]);
        if (!in_bounds(next_x, next_y) ||
            layout.terrain[level_cell_index(
                static_cast<uint8_t>(next_x),
                static_cast<uint8_t>(next_y))] == TERRAIN_WALL) {
            ++count;
        }
    }
    return count;
}

uint8_t enumerate_pull_candidates(const LevelLayout &layout,
                                  const DifficultyProfile &profile,
                                  PullCandidate candidates[MAX_PULL_CANDIDATES])
{
    uint8_t count = 0U;
    const uint8_t fair_share = static_cast<uint8_t>(
        profile.desired_pulls / layout.crate_count);
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        for (uint8_t direction = 0U; direction < 4U; ++direction) {
            const int8_t crate_x = static_cast<int8_t>(
                layout.crate_x[crate] - DIRECTION_X[direction]);
            const int8_t crate_y = static_cast<int8_t>(
                layout.crate_y[crate] - DIRECTION_Y[direction]);
            const int8_t player_x = static_cast<int8_t>(
                layout.crate_x[crate] - 2 * DIRECTION_X[direction]);
            const int8_t player_y = static_cast<int8_t>(
                layout.crate_y[crate] - 2 * DIRECTION_Y[direction]);
            if (!is_open_cell(layout, crate_x, crate_y) ||
                !is_open_cell(layout, player_x, player_y) ||
                !player_can_reach(
                    layout, layout.player_x, layout.player_y,
                    static_cast<uint8_t>(crate_x),
                    static_cast<uint8_t>(crate_y))) {
                continue;
            }

            PullCandidate &candidate = candidates[count++];
            candidate.crate = crate;
            candidate.direction = direction;
            candidate.crate_x = static_cast<uint8_t>(crate_x);
            candidate.crate_y = static_cast<uint8_t>(crate_y);
            candidate.player_x = static_cast<uint8_t>(player_x);
            candidate.player_y = static_cast<uint8_t>(player_y);
            candidate.score = static_cast<int16_t>(random_below(8U));

            if (pulls_by_crate[crate] < fair_share) {
                candidate.score += 45;
            }
            if (layout.terrain[level_cell_index(
                    layout.crate_x[crate], layout.crate_y[crate])] ==
                TERRAIN_TARGET) {
                candidate.score += 28;
            }
            const uint8_t old_distance = manhattan(
                layout.crate_x[crate], layout.crate_y[crate],
                target_x[crate], target_y[crate]);
            const uint8_t new_distance = manhattan(
                candidate.crate_x, candidate.crate_y,
                target_x[crate], target_y[crate]);
            candidate.score += new_distance > old_distance ? 18 : -9;
            if (!was_visited(crate, candidate.crate_x, candidate.crate_y)) {
                candidate.score += 24;
            }
            candidate.score += static_cast<int16_t>(
                adjacent_wall_count(layout, candidate.crate_x,
                                    candidate.crate_y) * 2U);

            const uint8_t previous = last_pull_direction[crate];
            if (previous != NO_DIRECTION) {
                if (direction == static_cast<uint8_t>((previous + 2U) % 4U)) {
                    candidate.score -= 55;
                } else if (direction != previous) {
                    candidate.score += 12;
                }
            }
        }
    }
    return count;
}

bool perform_reverse_pull(LevelLayout *layout,
                          const DifficultyProfile &profile,
                          uint16_t *turns)
{
    PullCandidate candidates[MAX_PULL_CANDIDATES];
    const uint8_t count = enumerate_pull_candidates(
        *layout, profile, candidates);
    if (count == 0U) {
        return false;
    }

    uint8_t selected = 0U;
    for (uint8_t index = 1U; index < count; ++index) {
        if (candidates[index].score > candidates[selected].score) {
            selected = index;
        }
    }
    const PullCandidate &pull = candidates[selected];
    const uint8_t previous = last_pull_direction[pull.crate];
    if (previous != NO_DIRECTION && pull.direction != previous &&
        pull.direction != static_cast<uint8_t>((previous + 2U) % 4U)) {
        ++(*turns);
    }
    last_pull_direction[pull.crate] = pull.direction;
    if (pulls_by_crate[pull.crate] < 0xFFU) {
        ++pulls_by_crate[pull.crate];
    }
    layout->crate_x[pull.crate] = pull.crate_x;
    layout->crate_y[pull.crate] = pull.crate_y;
    layout->player_x = pull.player_x;
    layout->player_y = pull.player_y;
    mark_visited(pull.crate, pull.crate_x, pull.crate_y);
    return true;
}

bool next_permutation(uint8_t permutation[MAX_CRATES], uint8_t count)
{
    if (count < 2U) {
        return false;
    }
    int8_t pivot = static_cast<int8_t>(count - 2U);
    while (pivot >= 0 && permutation[static_cast<uint8_t>(pivot)] >=
                         permutation[static_cast<uint8_t>(pivot + 1)]) {
        --pivot;
    }
    if (pivot < 0) {
        return false;
    }
    uint8_t successor = static_cast<uint8_t>(count - 1U);
    while (permutation[successor] <= permutation[static_cast<uint8_t>(pivot)]) {
        --successor;
    }
    const uint8_t pivot_index = static_cast<uint8_t>(pivot);
    uint8_t temporary = permutation[pivot_index];
    permutation[pivot_index] = permutation[successor];
    permutation[successor] = temporary;
    uint8_t left = static_cast<uint8_t>(pivot_index + 1U);
    uint8_t right = static_cast<uint8_t>(count - 1U);
    while (left < right) {
        temporary = permutation[left];
        permutation[left++] = permutation[right];
        permutation[right--] = temporary;
    }
    return true;
}

uint8_t minimum_target_distance(const LevelLayout &layout)
{
    uint8_t permutation[MAX_CRATES];
    for (uint8_t index = 0U; index < layout.crate_count; ++index) {
        permutation[index] = index;
    }
    uint8_t minimum = 0xFFU;
    do {
        uint8_t distance = 0U;
        for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
            distance = static_cast<uint8_t>(distance + manhattan(
                layout.crate_x[crate], layout.crate_y[crate],
                target_x[permutation[crate]], target_y[permutation[crate]]));
        }
        if (distance < minimum) {
            minimum = distance;
        }
    } while (next_permutation(permutation, layout.crate_count));
    return minimum;
}

uint8_t moved_crate_count(const LevelLayout &layout)
{
    uint8_t count = 0U;
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (pulls_by_crate[crate] > 0U &&
            layout.terrain[level_cell_index(
                layout.crate_x[crate], layout.crate_y[crate])] !=
                TERRAIN_TARGET) {
            ++count;
        }
    }
    return count;
}

bool all_crates_on_targets(const LevelLayout &layout)
{
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (layout.terrain[level_cell_index(
                layout.crate_x[crate], layout.crate_y[crate])] !=
                TERRAIN_TARGET) {
            return false;
        }
    }
    return true;
}

bool generate_attempt(uint16_t level_index, uint8_t retry, LevelLayout *layout)
{
    memset(layout, 0, sizeof(*layout));
    memset(pulls_by_crate, 0, sizeof(pulls_by_crate));
    memset(last_pull_direction, NO_DIRECTION, sizeof(last_pull_direction));
    memset(crate_visited, 0, sizeof(crate_visited));
    random_state = 0x9E3779B9UL ^
        (static_cast<uint32_t>(level_index + 1U) * 0x85EBCA6BUL) ^
        (static_cast<uint32_t>(retry + 1U) * 0xC2B2AE35UL);
    if (random_state == 0UL) {
        random_state = 1UL;
    }

    const DifficultyProfile profile = difficulty_for_level(level_index);
    layout->crate_count = profile.crate_count;
    if (!create_terrain(profile, layout) || !place_targets(layout) ||
        !place_player(layout)) {
        return false;
    }
    for (uint8_t crate = 0U; crate < layout->crate_count; ++crate) {
        mark_visited(crate, layout->crate_x[crate], layout->crate_y[crate]);
    }

    uint16_t successful_pulls = 0U;
    uint16_t turns = 0U;
    const uint16_t maximum_attempts = static_cast<uint16_t>(
        profile.desired_pulls) * 8U;
    for (uint16_t attempt = 0U;
         attempt < maximum_attempts &&
         successful_pulls < profile.desired_pulls;
         ++attempt) {
        if (perform_reverse_pull(layout, profile, &turns)) {
            ++successful_pulls;
        }
    }

    layout->scramble_pulls = successful_pulls;
    layout->scramble_turns = turns;
    layout->moved_crates = moved_crate_count(*layout);
    layout->minimum_push_distance = minimum_target_distance(*layout);
    if (successful_pulls != profile.desired_pulls ||
        layout->moved_crates < profile.required_moved_crates ||
        layout->minimum_push_distance < profile.minimum_push_distance ||
        turns < profile.minimum_turns || all_crates_on_targets(*layout)) {
        return false;
    }

    layout->starting_score = 1200UL +
        static_cast<uint32_t>(layout->crate_count) * 500UL +
        static_cast<uint32_t>(successful_pulls) * 40UL +
        static_cast<uint32_t>(turns) * 25UL +
        static_cast<uint32_t>(layout->minimum_push_distance) * 60UL +
        static_cast<uint32_t>(level_index) * 3UL;
    return level_entities_are_valid(*layout);
}

}  // namespace

uint8_t level_cell_index(uint8_t x, uint8_t y)
{
    return static_cast<uint8_t>(y * LEVEL_COLS + x);
}

bool level_entities_are_valid(const LevelLayout &layout)
{
    if (layout.crate_count < 2U || layout.crate_count > MAX_CRATES ||
        layout.player_x >= LEVEL_COLS || layout.player_y >= LEVEL_ROWS ||
        layout.terrain[level_cell_index(layout.player_x, layout.player_y)] ==
            TERRAIN_WALL ||
        has_crate(layout, layout.player_x, layout.player_y)) {
        return false;
    }
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (layout.crate_x[crate] >= LEVEL_COLS ||
            layout.crate_y[crate] >= LEVEL_ROWS ||
            layout.terrain[level_cell_index(
                layout.crate_x[crate], layout.crate_y[crate])] == TERRAIN_WALL) {
            return false;
        }
        for (uint8_t other = static_cast<uint8_t>(crate + 1U);
             other < layout.crate_count; ++other) {
            if (layout.crate_x[crate] == layout.crate_x[other] &&
                layout.crate_y[crate] == layout.crate_y[other]) {
                return false;
            }
        }
    }
    return true;
}

bool level_generate(uint16_t level_index, LevelLayout *layout)
{
    if (layout == nullptr || level_index >= LEVEL_COUNT) {
        return false;
    }
#if defined(REBUILD_RETRY_RECIPES)
    const uint8_t first_retry = 0U;
#else
    const uint8_t first_retry = retry_recipe(level_index);
#endif
    for (uint16_t offset = 0U; offset < 256U; ++offset) {
        const uint8_t retry = static_cast<uint8_t>(
            first_retry + offset);
        if (generate_attempt(level_index, retry, layout)) {
            layout->generation_retry = retry;
            layout->generation_attempts = offset < 255U ?
                static_cast<uint8_t>(offset + 1U) : 0xFFU;
            return true;
        }
    }
    return false;
}
