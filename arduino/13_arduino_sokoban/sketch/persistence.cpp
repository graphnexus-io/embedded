#include "persistence.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>
#include <string.h>

#include "config.h"

namespace {

constexpr uint16_t SAVE_MAGIC = 0x534BU;
constexpr uint8_t SAVE_VERSION = 3U;
constexpr uint8_t LEGACY_SAVE_VERSION_1 = 1U;
constexpr uint8_t LEGACY_SAVE_VERSION_2 = 2U;
constexpr uint8_t FLAG_ACTIVE = 0x01U;
constexpr uint16_t LEGACY_LEVEL_COUNT = 500U;
constexpr uint8_t LEGACY_MAX_CRATES = 4U;

struct __attribute__((packed)) SaveRecord {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t sequence;
    uint8_t mode;
    uint16_t level_index;
    uint16_t free_level;
    uint16_t campaign_next;
    uint32_t campaign_score;
    uint32_t level_score;
    uint16_t moves;
    uint16_t pushes;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t crate_count;
    uint8_t crate_x[MAX_CRATES];
    uint8_t crate_y[MAX_CRATES];
    uint16_t crc;
};

// Firmware 1.x used a 40-byte journal entry with four crate positions. Keep
// its exact layout so progress survives the 100-level/large-board migration.
struct __attribute__((packed)) LegacySaveRecord {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t sequence;
    uint8_t mode;
    uint16_t level_index;
    uint16_t free_level;
    uint16_t campaign_next;
    uint32_t campaign_score;
    uint32_t level_score;
    uint16_t moves;
    uint16_t pushes;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t crate_count;
    uint8_t crate_x[LEGACY_MAX_CRATES];
    uint8_t crate_y[LEGACY_MAX_CRATES];
    uint16_t crc;
};

static_assert(sizeof(SaveRecord) == 44U, "Unexpected EEPROM save layout");
static_assert(sizeof(LegacySaveRecord) == 40U, "Unexpected legacy save layout");

uint32_t latest_sequence = 0UL;
uint16_t latest_slot = 0U;
bool journal_initialized = false;

uint16_t crc16(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    for (uint8_t index = 0U; index < length; ++index) {
        crc ^= static_cast<uint16_t>(data[index]) << 8U;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U ?
                static_cast<uint16_t>((crc << 1U) ^ 0x1021U) :
                static_cast<uint16_t>(crc << 1U);
        }
    }
    return crc;
}

uint16_t slot_count()
{
    return static_cast<uint16_t>(EEPROM.length() / sizeof(SaveRecord));
}

bool mode_is_valid(uint8_t mode)
{
    return mode == static_cast<uint8_t>(GameMode::FREE_SELECT) ||
           mode == static_cast<uint8_t>(GameMode::CAMPAIGN);
}

bool sequence_is_newer(uint32_t candidate, uint32_t reference)
{
    return static_cast<int32_t>(candidate - reference) > 0L;
}

bool record_is_valid(const SaveRecord &record)
{
    if (record.magic != SAVE_MAGIC || record.version != SAVE_VERSION ||
        !mode_is_valid(record.mode) || record.free_level >= LEVEL_COUNT ||
        record.campaign_next > LEVEL_COUNT) {
        return false;
    }
    const uint16_t expected = crc16(
        reinterpret_cast<const uint8_t *>(&record),
        static_cast<uint8_t>(offsetof(SaveRecord, crc)));
    if (record.crc != expected) {
        return false;
    }
    if ((record.flags & FLAG_ACTIVE) == 0U) {
        return true;
    }
    if (record.level_index >= LEVEL_COUNT || record.crate_count < 2U ||
        record.crate_count > MAX_CRATES || record.player_x >= LEVEL_COLS ||
        record.player_y >= LEVEL_ROWS) {
        return false;
    }
    for (uint8_t crate = 0U; crate < record.crate_count; ++crate) {
        if (record.crate_x[crate] >= LEVEL_COLS ||
            record.crate_y[crate] >= LEVEL_ROWS) {
            return false;
        }
    }
    return true;
}

bool legacy_record_is_valid(const LegacySaveRecord &record)
{
    if (record.magic != SAVE_MAGIC ||
        (record.version != LEGACY_SAVE_VERSION_1 &&
         record.version != LEGACY_SAVE_VERSION_2) ||
        !mode_is_valid(record.mode) || record.free_level >= LEGACY_LEVEL_COUNT ||
        record.campaign_next > LEGACY_LEVEL_COUNT) {
        return false;
    }
    return record.crc == crc16(
        reinterpret_cast<const uint8_t *>(&record),
        static_cast<uint8_t>(offsetof(LegacySaveRecord, crc)));
}

void record_to_state(const SaveRecord &record, SessionState *state)
{
    state->active = (record.flags & FLAG_ACTIVE) != 0U;
    state->mode = static_cast<GameMode>(record.mode);
    state->level_index = record.level_index;
    state->free_level = record.free_level;
    state->campaign_next = record.campaign_next;
    state->campaign_score = record.campaign_score;
    state->level_score = record.level_score;
    state->moves = record.moves;
    state->pushes = record.pushes;
    state->player_x = record.player_x;
    state->player_y = record.player_y;
    state->crate_count = record.crate_count;
    memcpy(state->crate_x, record.crate_x, sizeof(state->crate_x));
    memcpy(state->crate_y, record.crate_y, sizeof(state->crate_y));
}

void migrate_legacy_record(const LegacySaveRecord &record, SessionState *state)
{
    persistence_set_defaults(state);
    state->mode = static_cast<GameMode>(record.mode);
    state->free_level = record.free_level >= LEVEL_COUNT ?
        static_cast<uint16_t>(LEVEL_COUNT - 1U) : record.free_level;
    state->campaign_next = record.campaign_next > LEVEL_COUNT ?
        LEVEL_COUNT : record.campaign_next;
    state->campaign_score = record.campaign_score;
    state->active = false;
}

void state_to_record(const SessionState &state, SaveRecord *record)
{
    memset(record, 0, sizeof(*record));
    record->magic = SAVE_MAGIC;
    record->version = SAVE_VERSION;
    record->flags = state.active ? FLAG_ACTIVE : 0U;
    record->sequence = latest_sequence + 1UL;
    record->mode = static_cast<uint8_t>(state.mode);
    record->level_index = state.level_index;
    record->free_level = state.free_level;
    record->campaign_next = state.campaign_next;
    record->campaign_score = state.campaign_score;
    record->level_score = state.level_score;
    record->moves = state.moves;
    record->pushes = state.pushes;
    record->player_x = state.player_x;
    record->player_y = state.player_y;
    record->crate_count = state.crate_count;
    memcpy(record->crate_x, state.crate_x, sizeof(record->crate_x));
    memcpy(record->crate_y, state.crate_y, sizeof(record->crate_y));
    record->crc = crc16(
        reinterpret_cast<const uint8_t *>(record),
        static_cast<uint8_t>(offsetof(SaveRecord, crc)));
}

}  // namespace

void persistence_set_defaults(SessionState *state)
{
    if (state == nullptr) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->mode = GameMode::FREE_SELECT;
    state->free_level = 0U;
    state->campaign_next = 0U;
}

bool persistence_load(SessionState *state)
{
    if (state == nullptr) {
        return false;
    }
    persistence_set_defaults(state);

    bool new_found = false;
    SaveRecord newest;
    const uint16_t new_count = slot_count();
    for (uint16_t slot = 0U; slot < new_count; ++slot) {
        SaveRecord candidate;
        EEPROM.get(static_cast<int>(slot * sizeof(SaveRecord)), candidate);
        if (!record_is_valid(candidate)) {
            continue;
        }
        if (!new_found || sequence_is_newer(candidate.sequence, newest.sequence)) {
            newest = candidate;
            latest_slot = slot;
            new_found = true;
        }
    }

    bool legacy_found = false;
    LegacySaveRecord legacy_newest;
    const uint16_t legacy_count = static_cast<uint16_t>(
        EEPROM.length() / sizeof(LegacySaveRecord));
    for (uint16_t slot = 0U; slot < legacy_count; ++slot) {
        LegacySaveRecord candidate;
        EEPROM.get(static_cast<int>(slot * sizeof(LegacySaveRecord)), candidate);
        if (!legacy_record_is_valid(candidate)) {
            continue;
        }
        if (!legacy_found ||
            sequence_is_newer(candidate.sequence, legacy_newest.sequence)) {
            legacy_newest = candidate;
            legacy_found = true;
        }
    }

    if (new_found &&
        (!legacy_found || !sequence_is_newer(legacy_newest.sequence,
                                             newest.sequence))) {
        latest_sequence = newest.sequence;
        journal_initialized = true;
        record_to_state(newest, state);
        return true;
    }
    if (legacy_found) {
        latest_sequence = legacy_newest.sequence;
        latest_slot = 0U;
        journal_initialized = false;
        migrate_legacy_record(legacy_newest, state);
        Serial.println(F("EEPROM: v1/v2 progress migrated; active board reset"));
        return true;
    }

    latest_sequence = 0UL;
    latest_slot = 0U;
    journal_initialized = false;
    return false;
}

bool persistence_save(const SessionState &state)
{
    const uint16_t count = slot_count();
    if (count == 0U) {
        return false;
    }
    const uint16_t target_slot = journal_initialized ?
        static_cast<uint16_t>((latest_slot + 1U) % count) : 0U;
    SaveRecord record;
    state_to_record(state, &record);
    EEPROM.put(static_cast<int>(target_slot * sizeof(SaveRecord)), record);

    SaveRecord verification;
    EEPROM.get(static_cast<int>(target_slot * sizeof(SaveRecord)), verification);
    if (!record_is_valid(verification) ||
        verification.sequence != record.sequence) {
        return false;
    }
    latest_slot = target_slot;
    latest_sequence = record.sequence;
    journal_initialized = true;
    return true;
}
