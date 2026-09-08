#pragma once

#include "paper2_scotland2/shared/logger.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

// #define LOG_INFO(value...)
#define LOG_INFO(value...) logger.info(value)
#define LOG_DEBUG(value...)
// #define LOG_DEBUG(value...) logger.debug(value)
// #define LOG_ERROR(value...)
#define LOG_ERROR(value...) logger.error(value)

#define LOWER(string) std::transform(string.begin(), string.end(), string.begin(), tolower)

std::string GetPlaylistsPath();
std::string GetConfigPath();
std::string GetCoversPath();

#include "songcore/shared/SongLoader/RuntimeSongLoader.hpp"

#define CustomLevelPackPrefixID SongCore::SongLoader::RuntimeSongLoader::CUSTOM_LEVEL_PACK_PREFIX_ID
