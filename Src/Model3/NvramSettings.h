/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * NvramSettings.h
 *
 * Operator settings stored in a Model 3 game's NVRAM (93C46 EEPROM, or Backup
 * RAM for a few games): country, difficulty, link mode, cabinet type and other
 * per-game options that are otherwise set in the game's Test menu, some of them
 * (like the country) only after entering an unlock code.
 *
 * Apply() writes the selected values into the EEPROM words and Backup RAM and
 * regenerates the game's own checksum, the same way the Test menu stores them.
 * The field locations, values and checksums come from the libretro Supermodel
 * core (libretro/Libretro-Supermodel, Src/OSD/libretro/LibretroNvramSettings.h),
 * where they were validated against each game's Test menu.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "Game.h"

namespace NvramSettings {

enum class Setting : unsigned {
  Country = 0,
  Difficulty,
  LinkMode,
  LinkId,
  Cabinet,
  SpecialCar,
  DisplayType,
  ChristmasMode,
  Vocal,
  LeverFeedback,
  DefaultView,
  ProvideCalibrationData,
  Count,
};

enum Capability : unsigned {
  None          = 0,
  Country       = 1u << static_cast<unsigned>(Setting::Country),
  Difficulty    = 1u << static_cast<unsigned>(Setting::Difficulty),
  LinkMode      = 1u << static_cast<unsigned>(Setting::LinkMode),
  LinkId        = 1u << static_cast<unsigned>(Setting::LinkId),
  Cabinet       = 1u << static_cast<unsigned>(Setting::Cabinet),
  SpecialCar    = 1u << static_cast<unsigned>(Setting::SpecialCar),
  DisplayType   = 1u << static_cast<unsigned>(Setting::DisplayType),
  ChristmasMode = 1u << static_cast<unsigned>(Setting::ChristmasMode),
  Vocal         = 1u << static_cast<unsigned>(Setting::Vocal),
  LeverFeedback = 1u << static_cast<unsigned>(Setting::LeverFeedback),
  DefaultView   = 1u << static_cast<unsigned>(Setting::DefaultView),
  ProvideCalibrationData =
    1u << static_cast<unsigned>(Setting::ProvideCalibrationData),
};

constexpr unsigned CapabilityFor(Setting setting)
{
  return 1u << static_cast<unsigned>(setting);
}

struct Value {
  const char *key;
  const char *label;
  uint16_t stored;
};

struct SettingInfo {
  Setting setting;
  const char *label;
  const char *description;
  const Value *values;
  size_t valueCount;
};

// Config file key of each setting, in Setting order. "keep" (the default) leaves
// the value stored by the game unchanged.
static constexpr const char *kKeepValue = "keep";

inline const char *ConfigKey(Setting setting)
{
  static const char *keys[] = {
    "NvramCountry",
    "NvramDifficulty",
    "NvramLinkMode",
    "NvramLinkId",
    "NvramCabinet",
    "NvramSpecialCar",
    "NvramDisplayType",
    "NvramChristmasMode",
    "NvramVocal",
    "NvramLeverFeedback",
    "NvramDefaultView",
    "NvramCalibration",
  };
  static_assert(sizeof(keys) / sizeof(keys[0]) == static_cast<unsigned>(Setting::Count), "one key per setting");
  return keys[static_cast<unsigned>(setting)];
}

// Settings that can be set for all games at once, in the global section of the
// config file: the country names are the same in every game, and a game that
// lacks the chosen country keeps its own. The other settings have values of
// their own in each game, so they are only set in a game's section.
inline bool IsGlobalSetting(Setting setting)
{
  return setting == Setting::Country;
}

// Every country name used by some game, for the global setting
static constexpr const char *kAllCountries[] = { "japan", "usa", "export", "australia", "korea", "asia" };
static constexpr const char *kAllCountryLabels[] = { "Japan", "USA", "Export", "Australia", "Korea", "Asia" };

struct Selection {
  const char *values[static_cast<unsigned>(Setting::Count)] = {};
  bool normalizeAnalogInputs = false;

  const char *Get(Setting setting) const
  {
    return values[static_cast<unsigned>(setting)];
  }

  bool HasAny() const
  {
    if (normalizeAnalogInputs)
      return true;
    for (const char *value : values)
      if (value)
        return true;
    return false;
  }
};

enum class ApplyResult {
  Unsupported,
  InvalidLayout,
  Unchanged,
  Changed,
};

namespace Detail {

enum class Family {
  None,
  Bass,
  Daytona2,
  DirtDevils,
  Eca,
  FightingVipers2,
  Harley,
  Lamachin,
  LeMans24,
  LostWorld,
  MagTruck,
  OceanHunter,
  Scud,
  SkiChamp,
  Spikeout,
  SegaRally2,
  StarWars,
  VirtuaFighter3,
  VirtualOn2,
  Vs2,
  Vs298,
  Vs299,
};

inline Family GetFamily(const Game *game)
{
  if (!game)
    return Family::None;

  if (game->name == "mgtrkbad")
    return Family::None;

  // Prototype, location-test and other pre-release sets are catalogued by the
  // Service-menu campaign but intentionally excluded from NVRAM management.
  if (game->name == "srally2p" || game->name == "srally2pa" ||
      game->name == "lostwsgp" || game->name == "swtrilgyp" ||
      game->name == "ecap")
    return Family::None;

  const std::string &family = game->parent.empty() ? game->name : game->parent;
  if (family == "bassdx")   return Family::Bass;
  if (family == "daytona2") return Family::Daytona2;
  if (family == "dirtdvls") return Family::DirtDevils;
  if (family == "eca")      return Family::Eca;
  if (family == "fvipers2") return Family::FightingVipers2;
  if (family == "harley")   return Family::Harley;
  if (family == "lamachin") return Family::Lamachin;
  if (family == "lemans24") return Family::LeMans24;
  if (family == "lostwsga") return Family::LostWorld;
  if (family == "magtruck") return Family::MagTruck;
  if (family == "oceanhun") return Family::OceanHunter;
  if (family == "scud")     return Family::Scud;
  if (family == "skichamp") return Family::SkiChamp;
  if (family == "spikeout" || family == "spikeofe")
    return Family::Spikeout;
  if (family == "srally2")  return Family::SegaRally2;
  if (family == "swtrilgy") return Family::StarWars;
  if (family == "vf3")      return Family::VirtuaFighter3;
  if (family == "von2")     return Family::VirtualOn2;
  if (family == "vs2")      return Family::Vs2;
  if (family == "vs298")    return Family::Vs298;
  if (family == "vs2v991")  return Family::Vs299;
  return Family::None;
}

inline bool CountryExcluded(const Game &game)
{
  return game.name == "vs215o" || game.name == "vs29815" ||
         game.name == "vs299j" || game.name == "vs29915j";
}

inline bool SettingExcluded(const Game &game, Setting setting)
{
  // The native Australian operator catalogue has no Country selector.
  // Difficulty, Cabinet Type and networking use directly verified native fields.
  if (game.name == "scudau")
    return setting == Setting::Country;
  if (game.name == "scuddx" || game.name == "scuddxo")
    return setting == Setting::LinkMode || setting == Setting::LinkId ||
           setting == Setting::Cabinet;
  if (game.name == "srally2dx")
    return setting == Setting::LinkMode || setting == Setting::Cabinet;
  return false;
}

#define NVRAM_VALUES(name, ...) \
  static const Value name[] = { __VA_ARGS__ }
#define NVRAM_INFO(name, setting_, label_, description_, values_) \
  static const SettingInfo name = { setting_, label_, description_, values_, \
                                    sizeof(values_) / sizeof(values_[0]) }

NVRAM_VALUES(kCountryJuea0,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "export", "Export", 2 }, { "australia", "Australia", 3 });
NVRAM_VALUES(kCountryJuea1,
  { "japan", "Japan", 1 }, { "usa", "USA", 2 },
  { "export", "Export", 3 }, { "australia", "Australia", 4 });
NVRAM_VALUES(kCountryJueak1,
  { "japan", "Japan", 1 }, { "usa", "USA", 2 },
  { "export", "Export", 3 }, { "australia", "Australia", 4 },
  { "korea", "Korea", 5 });
NVRAM_VALUES(kCountryJueka0,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "export", "Export", 2 }, { "korea", "Korea", 3 },
  { "australia", "Australia", 4 });
NVRAM_VALUES(kCountryJuekaHarley,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "export", "Export", 2 }, { "korea", "Korea", 3 },
  { "australia", "Australia", 4 });
NVRAM_VALUES(kCountryJue0,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "export", "Export", 2 });
NVRAM_VALUES(kCountryUea1,
  { "usa", "USA", 1 }, { "export", "Export", 2 },
  { "australia", "Australia", 3 });
NVRAM_VALUES(kCountryEu0,
  { "export", "Export", 0 }, { "usa", "USA", 1 });
NVRAM_VALUES(kCountryJuae0,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "australia", "Australia", 2 }, { "export", "Export", 3 });
NVRAM_VALUES(kCountryUe1,
  { "usa", "USA", 1 }, { "export", "Export", 2 });
NVRAM_VALUES(kCountryJueAsia0,
  { "japan", "Japan", 0 }, { "usa", "USA", 1 },
  { "export", "Export", 2 }, { "asia", "Asia", 3 });
NVRAM_VALUES(kCountryRally,
  { "japan", "Japan", 0x8000 }, { "usa", "USA", 0x9000 },
  { "australia", "Australia", 0xa000 }, { "export", "Export", 0xc000 });
NVRAM_VALUES(kCountryStarWars,
  { "japan", "Japan", 0x8000 }, { "usa", "USA", 0x9000 },
  { "australia", "Australia", 0xa000 }, { "korea", "Korea", 0xb000 },
  { "export", "Export", 0xc000 });

NVRAM_VALUES(kDifficultyBass,
  { "very_easy", "Very Easy", 0 }, { "medium_easy", "Medium Easy", 1 },
  { "normal", "Normal", 2 }, { "medium_hard", "Medium Hard", 3 },
  { "very_hard", "Very Hard", 4 });
NVRAM_VALUES(kDifficultyEasyHarder,
  { "easy", "Easy", 0 }, { "normal", "Normal", 1 },
  { "hard", "Hard", 2 }, { "harder", "Harder", 3 });
NVRAM_VALUES(kDifficultyVeryEasy,
  { "very_easy", "Very Easy", 0 }, { "easy", "Easy", 1 },
  { "normal", "Normal", 2 }, { "hard", "Hard", 3 },
  { "very_hard", "Very Hard", 4 });
NVRAM_VALUES(kDifficultyEasyHardest,
  { "easy", "Easy", 0 }, { "normal", "Normal", 1 },
  { "hard", "Hard", 2 }, { "hardest", "Hardest", 3 });
NVRAM_VALUES(kDifficultyEasiest,
  { "easiest", "Easiest", 0 }, { "easy", "Easy", 1 },
  { "normal", "Normal", 2 }, { "hard", "Hard", 3 },
  { "hardest", "Hardest", 4 });
NVRAM_VALUES(kDifficultyHarley,
  { "normal", "Normal", 0x00 }, { "plus_05", "+5 Seconds", 0x05 },
  { "plus_10", "+10 Seconds", 0x0a }, { "plus_15", "+15 Seconds", 0x0f },
  { "plus_20", "+20 Seconds", 0x14 }, { "minus_10", "-10 Seconds", 0xf6 },
  { "minus_05", "-5 Seconds", 0xfb });
NVRAM_VALUES(kDifficultyRally,
  { "normal", "Normal", 0x00 }, { "hard", "Hard", 0x08 },
  { "very_hard", "Very Hard", 0x10 }, { "very_easy", "Very Easy", 0x18 },
  { "easy", "Easy", 0x20 });
NVRAM_VALUES(kDifficultyStarWars,
  { "very_easy", "Very Easy", 0x00 }, { "easy", "Easy", 0x08 },
  { "normal", "Normal", 0x10 }, { "hard", "Hard", 0x18 },
  { "very_hard", "Very Hard", 0x20 });
NVRAM_VALUES(kDifficulty8,
  { "level_1", "Level 1", 0x00 }, { "level_2", "Level 2", 0x20 },
  { "level_3", "Level 3", 0x40 }, { "level_4", "Level 4", 0x60 },
  { "level_5", "Level 5", 0x80 }, { "level_6", "Level 6", 0xa0 },
  { "level_7", "Level 7", 0xc0 }, { "level_8", "Level 8", 0xe0 });
NVRAM_VALUES(kDifficulty16,
  { "level_1", "Level 1", 0x00 }, { "level_2", "Level 2", 0x10 },
  { "level_3", "Level 3", 0x20 }, { "level_4", "Level 4", 0x30 },
  { "level_5", "Level 5", 0x40 }, { "level_6", "Level 6", 0x50 },
  { "level_7", "Level 7", 0x60 }, { "level_8", "Level 8", 0x70 },
  { "level_9", "Level 9", 0x80 }, { "level_10", "Level 10", 0x90 },
  { "level_11", "Level 11", 0xa0 }, { "level_12", "Level 12", 0xb0 },
  { "level_13", "Level 13", 0xc0 }, { "level_14", "Level 14", 0xd0 },
  { "level_15", "Level 15", 0xe0 }, { "level_16", "Level 16", 0xf0 });

NVRAM_VALUES(kCabinetTwinSpecial,
  { "twin", "Twin", 0 }, { "special", "Special", 1 });

NVRAM_VALUES(kLinkDaytona,
  { "single", "Single", 0 }, { "master", "Master", 1 },
  { "slave", "Slave", 2 }, { "live", "Live", 3 });
NVRAM_VALUES(kLinkSingle,
  { "single", "Single", 0 }, { "master", "Master", 1 },
  { "slave", "Slave", 2 });
NVRAM_VALUES(kLinkA3Four,
  { "master", "Master", 0 }, { "slave", "Slave", 1 },
  { "satellite", "Satellite", 2 }, { "no_link", "No Link", 3 });
NVRAM_VALUES(kLinkLeMans,
  { "master", "Master", 0 }, { "slave", "Slave", 1 },
  { "no_link", "No Link", 2 });
NVRAM_VALUES(kLinkStandAlone,
  { "stand_alone", "Stand Alone", 0 }, { "master", "Master", 1 },
  { "slave", "Slave", 2 });
NVRAM_VALUES(kLinkRally,
  { "stand_alone", "Stand Alone", 0x000 },
  { "master_car_1", "Master, Car 1", 0x040 },
  { "slave_car_2", "Slave, Car 2", 0x080 },
  { "slave_car_3", "Slave, Car 3", 0x0c0 },
  { "slave_car_4", "Slave, Car 4", 0x100 },
  { "relay", "Relay", 0x140 });

NVRAM_VALUES(kCar16,
  { "1", "Car 1", 0 }, { "2", "Car 2", 1 }, { "3", "Car 3", 2 },
  { "4", "Car 4", 3 }, { "5", "Car 5", 4 }, { "6", "Car 6", 5 },
  { "7", "Car 7", 6 }, { "8", "Car 8", 7 }, { "9", "Car 9", 8 },
  { "10", "Car 10", 9 }, { "11", "Car 11", 10 },
  { "12", "Car 12", 11 }, { "13", "Car 13", 12 },
  { "14", "Car 14", 13 }, { "15", "Car 15", 14 },
  { "16", "Car 16", 15 });
NVRAM_VALUES(kCar8,
  { "1", "Car 1", 0 }, { "2", "Car 2", 1 }, { "3", "Car 3", 2 },
  { "4", "Car 4", 3 }, { "5", "Car 5", 4 }, { "6", "Car 6", 5 },
  { "7", "Car 7", 6 }, { "8", "Car 8", 7 });
NVRAM_VALUES(kMachine4,
  { "1", "Machine 1", 0 }, { "2", "Machine 2", 1 },
  { "3", "Machine 3", 2 }, { "4", "Machine 4", 3 });
NVRAM_VALUES(kCabinet4,
  { "1", "Cabinet 1", 0 }, { "2", "Cabinet 2", 1 },
  { "3", "Cabinet 3", 2 }, { "4", "Cabinet 4", 3 });
NVRAM_VALUES(kLeMansLinkId,
  { "1", "Cabinet 1", 0 }, { "2", "Cabinet 2", 1 },
  { "3", "Cabinet 3", 2 }, { "4", "Cabinet 4", 3 },
  { "satellite", "Satellite", 4 });
NVRAM_VALUES(kVonSeats,
  { "seat_a", "Seat A (Good)", 0 }, { "seat_b", "Seat B (Evil)", 1 },
  { "seat_c", "Seat C (Good)", 2 }, { "seat_d", "Seat D (Evil)", 3 });

NVRAM_VALUES(kCabinetDeluxeTwin,
  { "deluxe", "Deluxe", 0 }, { "twin", "Twin", 1 });
NVRAM_VALUES(kCabinetStandardDeluxe,
  { "standard", "Standard", 0 }, { "deluxe", "Deluxe", 1 });
NVRAM_VALUES(kCabinetDeluxeStandard,
  { "deluxe", "Deluxe", 0 }, { "standard", "Standard", 1 });
NVRAM_VALUES(kCabinetLeMans,
  { "twin_lemans", "Twin Le Mans", 0 },
  { "twin_normal", "Twin Normal", 1 });
NVRAM_VALUES(kCabinetRally,
  { "twin", "Twin", 0 }, { "deluxe", "Deluxe", 0x400 });
NVRAM_VALUES(kCabinetStarWars,
  { "upright", "Upright", 0 }, { "deluxe", "Deluxe", 0x400 });

NVRAM_VALUES(kSpecialCar,
  { "mclaren", "McLaren", 0 }, { "skyline", "Skyline", 1 },
  { "ferrari", "Ferrari", 2 }, { "porsche", "Porsche", 3 },
  { "mazda", "Mazda", 4 }, { "sauber", "Sauber", 5 },
  { "none", "None", 6 });
NVRAM_VALUES(kDisplayType,
  { "crt", "CRT", 0 }, { "projector", "Projector", 1 });
NVRAM_VALUES(kOffOn,
  { "off", "Off", 0 }, { "on", "On", 1 });
NVRAM_VALUES(kVocal,
  { "dennis", "Dennis", 0 }, { "mitsuyoshi", "Mitsuyoshi", 1 });
NVRAM_VALUES(kLeverFeedback,
  { "enable", "ON", 1 }, { "disable", "Disable", 0 });
NVRAM_VALUES(kDefaultView,
  { "driver", "Driver", 0 }, { "behind", "Behind", 0x0004 });
NVRAM_VALUES(kProvideCalibrationData,
  { "disabled", "OFF", 0 }, { "enabled", "ON", 1 });

NVRAM_INFO(kCountryInfoJuea0, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJuea0);
NVRAM_INFO(kCountryInfoJuea1, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJuea1);
NVRAM_INFO(kCountryInfoJueak1, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJueak1);
NVRAM_INFO(kCountryInfoJueka0, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJueka0);
NVRAM_INFO(kCountryInfoHarley, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJuekaHarley);
NVRAM_INFO(kCountryInfoJue0, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJue0);
NVRAM_INFO(kCountryInfoUea1, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryUea1);
NVRAM_INFO(kCountryInfoEu0, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryEu0);
NVRAM_INFO(kCountryInfoJuae0, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJuae0);
NVRAM_INFO(kCountryInfoUe1, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryUe1);
NVRAM_INFO(kCountryInfoAsia, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryJueAsia0);
NVRAM_INFO(kCountryInfoRally, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryRally);
NVRAM_INFO(kCountryInfoStarWars, Setting::Country, "Country", "Set the machine country stored in NVRAM.", kCountryStarWars);

NVRAM_INFO(kDifficultyInfoBass, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyBass);
NVRAM_INFO(kDifficultyInfoEasyHarder, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyEasyHarder);
NVRAM_INFO(kDifficultyInfoVeryEasy, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyVeryEasy);
NVRAM_INFO(kDifficultyInfoEasyHardest, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyEasyHardest);
NVRAM_INFO(kDifficultyInfoEasiest, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyEasiest);
NVRAM_INFO(kDifficultyInfoHarley, Setting::Difficulty, "Difficulty", "Set Harley-Davidson's time adjustment stored in NVRAM.", kDifficultyHarley);
NVRAM_INFO(kDifficultyInfoRally, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyRally);
NVRAM_INFO(kDifficultyInfoStarWars, Setting::Difficulty, "Difficulty", "Set the operator difficulty stored in NVRAM.", kDifficultyStarWars);
NVRAM_INFO(kDifficultyInfo8, Setting::Difficulty, "Difficulty", "Set the operator difficulty level stored in NVRAM.", kDifficulty8);
NVRAM_INFO(kDifficultyInfo16, Setting::Difficulty, "Difficulty", "Set the operator difficulty level stored in NVRAM.", kDifficulty16);
NVRAM_INFO(kCabinetInfoTwinSpecial, Setting::Cabinet, "Cabinet Type", "Select Daytona USA 2 Power Edition's cabinet type stored in NVRAM.", kCabinetTwinSpecial);

static constexpr char kLinkDescription[] =
  "Set the link mode stored in NVRAM (the game's own role on a linked "
  "network, separate from the Networking settings).";

NVRAM_INFO(kLinkInfoDaytona, Setting::LinkMode, "Link Mode", kLinkDescription, kLinkDaytona);
NVRAM_INFO(kLinkInfoSingle, Setting::LinkMode, "Link Mode", kLinkDescription, kLinkSingle);
NVRAM_INFO(kLinkInfoA3, Setting::LinkMode, "Link Mode", kLinkDescription, kLinkA3Four);
NVRAM_INFO(kLinkInfoLeMans, Setting::LinkMode, "Link Mode", kLinkDescription, kLinkLeMans);
NVRAM_INFO(kLinkInfoStandAlone, Setting::LinkMode, "Link Mode", kLinkDescription, kLinkStandAlone);
NVRAM_INFO(kLinkInfoRally, Setting::LinkMode, "Link Configuration", kLinkDescription, kLinkRally);
NVRAM_INFO(kCarInfo16, Setting::LinkId, "Car Number", "Set the linked-cabinet car number stored in NVRAM.", kCar16);
NVRAM_INFO(kCarInfo8, Setting::LinkId, "Car Number", "Set the linked-cabinet car number stored in NVRAM.", kCar8);
NVRAM_INFO(kMachineInfo4, Setting::LinkId, "Machine Number", "Set the linked machine number stored in NVRAM.", kMachine4);
NVRAM_INFO(kCabinetInfo4, Setting::LinkId, "Cabinet Number", "Set the linked cabinet number stored in NVRAM.", kCabinet4);
NVRAM_INFO(kLeMansLinkIdInfo, Setting::LinkId, "Cabinet Number", "Set the linked cabinet or satellite identity stored in NVRAM.", kLeMansLinkId);
NVRAM_INFO(kVonSeatInfo, Setting::LinkId, "Seat", "Set the linked Virtual On seat stored in NVRAM.", kVonSeats);

NVRAM_INFO(kCabinetInfoDeluxeTwin, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetDeluxeTwin);
NVRAM_INFO(kCabinetInfoStandardDeluxe, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetStandardDeluxe);
NVRAM_INFO(kCabinetInfoDeluxeStandard, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetDeluxeStandard);
NVRAM_INFO(kCabinetInfoLeMans, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetLeMans);
NVRAM_INFO(kCabinetInfoRally, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetRally);
NVRAM_INFO(kCabinetInfoStarWars, Setting::Cabinet, "Cabinet Type", "Set the cabinet type stored in NVRAM.", kCabinetStarWars);
NVRAM_INFO(kSpecialCarInfo, Setting::SpecialCar, "Special Car", "Set the special car selection stored in NVRAM.", kSpecialCar);
NVRAM_INFO(kDisplayTypeInfo, Setting::DisplayType, "Display Type", "Set the cabinet display type stored in NVRAM.", kDisplayType);
NVRAM_INFO(kChristmasInfo, Setting::ChristmasMode, "Christmas Mode", "Set Magical Truck Adventure's Christmas mode stored in NVRAM.", kOffOn);
NVRAM_INFO(kVocalInfo, Setting::Vocal, "Vocal", "Select the Daytona USA 2 vocal version stored in NVRAM.", kVocal);
NVRAM_INFO(kLeverFeedbackInfo, Setting::LeverFeedback, "Lever Feedback", "Enable or disable Star Wars Trilogy Arcade's Deluxe cabinet lever feedback stored in NVRAM.", kLeverFeedback);
NVRAM_INFO(kDefaultViewInfo, Setting::DefaultView, "Default View", "Select Sega Rally 2's default driving view stored in NVRAM.", kDefaultView);
NVRAM_INFO(kProvideCalibrationDataInfo, Setting::ProvideCalibrationData,
  "Provide Calibration Data",
  "Apply validated title-specific calibration data at startup, overwriting calibration stored by the Service Menu.",
  kProvideCalibrationData);

#undef NVRAM_INFO
#undef NVRAM_VALUES

inline const SettingInfo *GetSettingInfo(const Game *game, Setting setting)
{
  const Family family = GetFamily(game);
  if (family == Family::None || !game ||
      SettingExcluded(*game, setting) ||
      (setting == Setting::Country && CountryExcluded(*game)))
    return nullptr;

  switch (setting)
  {
  case Setting::Country:
    if (game->name == "vf3a")
      return &kCountryInfoJue0;
    switch (family)
    {
    case Family::Bass:
    case Family::DirtDevils:
    case Family::LeMans24:
    case Family::LostWorld:
    case Family::VirtualOn2:       return &kCountryInfoJuea0;
    case Family::Daytona2:
    case Family::OceanHunter:      return &kCountryInfoJueak1;
    case Family::FightingVipers2:
    case Family::VirtuaFighter3:   return &kCountryInfoAsia;
    case Family::Harley:           return &kCountryInfoHarley;
    case Family::Lamachin:
    case Family::Spikeout:         return &kCountryInfoJuea1;
    case Family::MagTruck:         return &kCountryInfoUea1;
    case Family::Scud:             return &kCountryInfoJue0;
    case Family::SkiChamp:         return &kCountryInfoJueka0;
    case Family::SegaRally2:       return &kCountryInfoRally;
    case Family::StarWars:         return &kCountryInfoStarWars;
    case Family::Vs2:              return &kCountryInfoEu0;
    case Family::Vs298:            return &kCountryInfoJuae0;
    case Family::Vs299:            return &kCountryInfoUe1;
    default:                       return nullptr;
    }
  case Setting::Difficulty:
    switch (family)
    {
    case Family::Bass:             return &kDifficultyInfoBass;
    case Family::Daytona2:         return &kDifficultyInfoEasyHarder;
    case Family::DirtDevils:
    case Family::LeMans24:
    case Family::LostWorld:
    case Family::MagTruck:
    case Family::VirtualOn2:       return &kDifficultyInfo16;
    case Family::Eca:
    case Family::FightingVipers2:  return &kDifficultyInfoVeryEasy;
    case Family::Harley:           return &kDifficultyInfoHarley;
    case Family::Lamachin:         return &kDifficultyInfo8;
    case Family::OceanHunter:
    case Family::Scud:
    case Family::VirtuaFighter3:
    case Family::Vs2:
    case Family::Vs298:
    case Family::Vs299:            return &kDifficultyInfoEasyHardest;
    case Family::Spikeout:         return &kDifficultyInfoEasiest;
    case Family::SegaRally2:       return &kDifficultyInfoRally;
    case Family::StarWars:         return &kDifficultyInfoStarWars;
    default:                       return nullptr;
    }
  case Setting::LinkMode:
    switch (family)
    {
    case Family::Daytona2:         return &kLinkInfoDaytona;
    case Family::DirtDevils:
    case Family::VirtualOn2:       return &kLinkInfoA3;
    case Family::Harley:
    case Family::SkiChamp:         return &kLinkInfoStandAlone;
    case Family::LeMans24:         return &kLinkInfoLeMans;
    case Family::Scud:
    case Family::Spikeout:         return &kLinkInfoSingle;
    case Family::SegaRally2:       return &kLinkInfoRally;
    default:                       return nullptr;
    }
  case Setting::LinkId:
    switch (family)
    {
    case Family::Daytona2:         return &kCarInfo16;
    case Family::DirtDevils:       return &kMachineInfo4;
    case Family::Harley:
    case Family::SkiChamp:         return &kCabinetInfo4;
    case Family::LeMans24:         return &kLeMansLinkIdInfo;
    case Family::Scud:             return &kCarInfo8;
    case Family::VirtualOn2:       return &kVonSeatInfo;
    default:                       return nullptr;
    }
  case Setting::Cabinet:
    if (game->name == "dayto2pe")
      return &kCabinetInfoTwinSpecial;
    switch (family)
    {
    case Family::Daytona2:
    case Family::Scud:             return &kCabinetInfoDeluxeTwin;
    case Family::Harley:
    case Family::OceanHunter:      return &kCabinetInfoDeluxeStandard;
    case Family::Lamachin:         return &kCabinetInfoStandardDeluxe;
    case Family::LeMans24:         return &kCabinetInfoLeMans;
    case Family::SegaRally2:       return &kCabinetInfoRally;
    case Family::StarWars:         return &kCabinetInfoStarWars;
    default:                       return nullptr;
    }
  case Setting::SpecialCar:
    return family == Family::LeMans24 ? &kSpecialCarInfo : nullptr;
  case Setting::DisplayType:
    return family == Family::VirtualOn2 ? &kDisplayTypeInfo : nullptr;
  case Setting::ChristmasMode:
    return family == Family::MagTruck ? &kChristmasInfo : nullptr;
  case Setting::Vocal:
    return family == Family::Daytona2 ? &kVocalInfo : nullptr;
  case Setting::LeverFeedback:
    return family == Family::StarWars ? &kLeverFeedbackInfo : nullptr;
  case Setting::DefaultView:
    return family == Family::SegaRally2 ? &kDefaultViewInfo : nullptr;
  case Setting::ProvideCalibrationData:
    switch (family)
    {
    case Family::Lamachin:
    case Family::LostWorld:
    case Family::OceanHunter:
    case Family::StarWars:         return &kProvideCalibrationDataInfo;
    default:                       return nullptr;
    }
  default:
    return nullptr;
  }
}

inline bool SelectedValue(const Game &game, Setting setting,
                          const Selection &selection, uint16_t &stored)
{
  const char *selected = selection.Get(setting);
  const SettingInfo *info = GetSettingInfo(&game, setting);
  if (!selected || !info)
    return false;
  for (size_t i = 0; i < info->valueCount; ++i)
  {
    if (std::strcmp(selected, info->values[i].key) == 0)
    {
      stored = info->values[i].stored;
      return true;
    }
  }
  return false;
}

inline uint16_t Crc16(const uint16_t *words, unsigned firstWord,
                      unsigned lastWord, uint16_t initial, uint16_t finalXor)
{
  uint16_t crc = initial;
  for (unsigned word = firstWord; word <= lastWord; ++word)
  {
    const uint8_t bytes[] = {
      static_cast<uint8_t>(words[word] >> 8),
      static_cast<uint8_t>(words[word] & 0xff),
    };
    for (uint8_t byte : bytes)
    {
      crc ^= static_cast<uint16_t>(byte) << 8;
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc & 0x8000)
          ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
          : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc ^ finalXor;
}

inline uint16_t CrcTableEntry(unsigned index)
{
  uint16_t crc = static_cast<uint16_t>(index << 8);
  for (unsigned bit = 0; bit < 8; ++bit)
    crc = (crc & 0x8000)
      ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
      : static_cast<uint16_t>(crc << 1);
  return crc;
}

inline uint32_t RotateLeft32Value(uint32_t value, unsigned count)
{
  return (value << count) | (value >> (32 - count));
}

inline uint16_t SegaA3Checksum(const uint16_t *words)
{
  uint32_t state = 0xdebdeb00;
  for (unsigned word = 6; word <= 34; ++word)
  {
    const uint8_t bytes[] = {
      static_cast<uint8_t>(words[word] >> 8),
      static_cast<uint8_t>(words[word] & 0xff),
    };
    for (uint8_t byte : bytes)
    {
      const unsigned index = (RotateLeft32Value(state, 9) & 0x1fe) >> 1;
      state = ((state ^ (static_cast<uint32_t>(CrcTableEntry(index)) << 8)) |
               byte) << 8;
    }
  }
  const unsigned index = (RotateLeft32Value(state, 9) & 0x1fe) >> 1;
  state ^= static_cast<uint32_t>(CrcTableEntry(index)) << 8;
  return static_cast<uint16_t>(RotateLeft32Value(state, 24));
}

inline bool IsM3Sega(const uint16_t *words)
{
  return words && words[0] == 0x4d33 && words[1] == 0x5345 &&
         words[2] == 0x4741;
}

inline bool IsGenibus(const uint16_t *words, Family family)
{
  if (!IsM3Sega(words))
    return false;
  if (family == Family::SegaRally2)
    return words[3] == 0x5352 && words[4] == 0x4332; // "SRC2"
  if (family == Family::StarWars)
    return words[3] == 0x5357 && words[4] == 0x5441; // "SWTA"
  return false;
}

} // namespace Detail

inline const char *OptionSuffix(Setting setting)
{
  static const char *suffixes[] = {
    "country",
    "difficulty",
    "link_mode",
    "car_number",
    "cabinet",
    "special_car",
    "display_type",
    "christmas_mode",
    "vocal",
    "lever_feedback",
    "default_view",
    "provide_calibration_data",
  };
  return suffixes[static_cast<unsigned>(setting)];
}

inline const SettingInfo *GetSettingInfo(const Game *game, Setting setting)
{
  return Detail::GetSettingInfo(game, setting);
}

inline bool IsValueSupported(const Game *game, Setting setting,
                             const char *value)
{
  const SettingInfo *info = GetSettingInfo(game, setting);
  if (!info || !value)
    return false;
  for (size_t i = 0; i < info->valueCount; ++i)
    if (std::strcmp(value, info->values[i].key) == 0)
      return true;
  return false;
}


inline unsigned GetCapabilities(const Game *game)
{
  unsigned capabilities = None;
  for (unsigned i = 0; i < static_cast<unsigned>(Setting::Count); ++i)
  {
    const Setting setting = static_cast<Setting>(i);
    if (GetSettingInfo(game, setting))
      capabilities |= CapabilityFor(setting);
  }
  return capabilities;
}


inline ApplyResult Apply(const Game &game, uint16_t *words,
                         uint8_t *backupRam, size_t backupRamSize,
                         const Selection &selection)
{
  using namespace Detail;
  const Family family = GetFamily(&game);
  if (family == Family::None)
    return ApplyResult::Unsupported;
  if (!words || !backupRam || backupRamSize < 0x20000)
    return ApplyResult::InvalidLayout;

  const bool backupLayout = family == Family::FightingVipers2 ||
                            family == Family::VirtuaFighter3;
  const bool genibusLayout = family == Family::SegaRally2 ||
                             family == Family::StarWars;
  const bool a3Layout = family == Family::DirtDevils ||
                        family == Family::Lamachin ||
                        family == Family::LeMans24 ||
                        family == Family::LostWorld ||
                        family == Family::MagTruck ||
                        family == Family::VirtualOn2;
  if (!backupLayout &&
      (genibusLayout ? !IsGenibus(words, family) : !IsM3Sega(words)))
    return ApplyResult::InvalidLayout;
  if (a3Layout && (words[4] & 0xff00) != 0xa300)
    return ApplyResult::InvalidLayout;

  bool changed = false;
  auto replaceMasked = [&changed](uint16_t &word, uint16_t mask,
                                   uint16_t value) {
    const uint16_t updated = static_cast<uint16_t>((word & ~mask) |
                                                    (value & mask));
    changed |= word != updated;
    word = updated;
  };
  auto replaceHigh = [&replaceMasked](uint16_t &word, uint16_t value) {
    replaceMasked(word, 0xff00, static_cast<uint16_t>(value << 8));
  };
  auto replaceLow = [&replaceMasked](uint16_t &word, uint16_t value) {
    replaceMasked(word, 0x00ff, value);
  };
  auto replaceBackup = [&changed, backupRam](size_t offset, uint16_t value) {
    const uint8_t updated = static_cast<uint8_t>(value);
    changed |= backupRam[offset] != updated;
    backupRam[offset] = updated;
  };

  uint16_t value = 0;
  const bool provideCalibrationData =
    SelectedValue(game, Setting::ProvideCalibrationData, selection, value) &&
    value != 0;
  switch (family)
  {
  case Family::Bass:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    break;
  case Family::Daytona2:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceLow(words[16], value), replaceLow(words[45], value);
    if (SelectedValue(game, Setting::LinkMode, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    if (SelectedValue(game, Setting::LinkId, selection, value))
      replaceLow(words[15], value), replaceLow(words[44], value);
    if (SelectedValue(game, Setting::Cabinet, selection, value))
    {
      if (game.name == "dayto2pe")
      {
        replaceBackup(8, value); replaceBackup(14, value);
      }
      else
        replaceHigh(words[16], value), replaceHigh(words[45], value);
    }
    if (SelectedValue(game, Setting::Vocal, selection, value))
      replaceLow(words[19], value), replaceLow(words[48], value);
    break;
  case Family::DirtDevils:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00f0, value);
    if (SelectedValue(game, Setting::LinkMode, selection, value)) replaceLow(words[12], value);
    if (SelectedValue(game, Setting::LinkId, selection, value)) replaceHigh(words[23], value);
    break;
  case Family::Eca:
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceLow(words[20], value), replaceLow(words[49], value);
    break;
  case Family::FightingVipers2:
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceBackup(94, value);
    if (SelectedValue(game, Setting::Country, selection, value)) replaceBackup(111, value);
    break;
  case Family::Harley:
    if (SelectedValue(game, Setting::Country, selection, value))
    {
      replaceHigh(words[12], value); replaceHigh(words[41], value);
      const uint16_t word9 = value == 0 ? 0x0102 : 0x0101;
      const uint16_t word10 = value == 0 ? 0x0101 : 0x0202;
      replaceMasked(words[9], 0xffff, word9); replaceMasked(words[38], 0xffff, word9);
      replaceMasked(words[10], 0xffff, word10); replaceMasked(words[39], 0xffff, word10);
    }
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    if (SelectedValue(game, Setting::LinkMode, selection, value))
    {
      const uint16_t connected = value == 0 ? 0 : 1;
      const uint16_t slave = value == 2 ? 1 : 0;
      replaceLow(words[16], connected); replaceLow(words[45], connected);
      replaceHigh(words[17], slave); replaceHigh(words[46], slave);
    }
    if (SelectedValue(game, Setting::LinkId, selection, value))
      replaceLow(words[17], value), replaceLow(words[46], value);
    if (SelectedValue(game, Setting::Cabinet, selection, value))
      replaceHigh(words[16], value), replaceHigh(words[45], value);
    break;
  case Family::Lamachin:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00e0, value);
    if (SelectedValue(game, Setting::Cabinet, selection, value)) replaceHigh(words[14], value);
    if (selection.normalizeAnalogInputs || provideCalibrationData)
    {
      // Service-menu P1/P2 X/Y calibration: complete 00-FF range in the
      // primary settings block and its redundant copy.
      for (unsigned word = 19; word <= 22; ++word)
        replaceMasked(words[word], 0xffff, 0xff00);
      for (unsigned word = 48; word <= 51; ++word)
        replaceMasked(words[word], 0xffff, 0xff00);
    }
    break;
  case Family::LeMans24:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00f0, value);
    if (SelectedValue(game, Setting::LinkMode, selection, value)) replaceLow(words[12], value);
    if (SelectedValue(game, Setting::LinkId, selection, value)) replaceLow(words[25], value);
    if (SelectedValue(game, Setting::Cabinet, selection, value)) replaceHigh(words[14], value);
    if (SelectedValue(game, Setting::SpecialCar, selection, value)) replaceLow(words[22], value);
    break;
  case Family::LostWorld:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00f0, value);
    if (selection.normalizeAnalogInputs || provideCalibrationData)
    {
      // Five-point Service-menu calibration reduced by the game to center and
      // endpoint values for each axis. Apply the same normalized F4/80/0B
      // calibration to both players; counters and transient records are not
      // copied from the captured samples.
      replaceMasked(words[23], 0xffff, 0x8080); // P1 X/Y center
      replaceMasked(words[24], 0xffff, 0xf40b); // P1 X endpoints
      replaceMasked(words[25], 0xffff, 0xf40b); // P1 Y endpoints
      replaceMasked(words[26], 0xffff, 0x8080); // P2 X/Y center
      replaceMasked(words[27], 0xffff, 0xf40b); // P2 X endpoints
      replaceMasked(words[28], 0xffff, 0xf40b); // P2 Y endpoints
    }
    break;
  case Family::MagTruck:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00f0, value);
    if (SelectedValue(game, Setting::ChristmasMode, selection, value)) replaceLow(words[17], value);
    break;
  case Family::OceanHunter:
    if (SelectedValue(game, Setting::Country, selection, value))
    {
      replaceHigh(words[12], value); replaceHigh(words[41], value);
      const uint16_t region = value == 1 ? 0x0101 : 0x0202;
      replaceMasked(words[10], 0xffff, region); replaceMasked(words[39], 0xffff, region);
    }
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    if (SelectedValue(game, Setting::Cabinet, selection, value))
      replaceHigh(words[16], value), replaceHigh(words[45], value);
    if (selection.normalizeAnalogInputs || provideCalibrationData)
    {
      // Service-menu calibration for P1/P2 X/Y: center 80, complete 00-FF
      // range. Preserve the unused third analog channel in each player group.
      replaceMasked(words[19], 0xffff, 0x80ff);
      replaceMasked(words[20], 0xffff, 0x0080);
      replaceMasked(words[21], 0xffff, 0xff00);
      replaceMasked(words[24], 0xffff, 0xff00);
      replaceMasked(words[25], 0xffff, 0x80ff);
      replaceMasked(words[26], 0xffff, 0x0080);
      replaceMasked(words[48], 0xffff, 0x80ff);
      replaceMasked(words[49], 0xffff, 0x0080);
      replaceMasked(words[50], 0xffff, 0xff00);
      replaceMasked(words[53], 0xffff, 0xff00);
      replaceMasked(words[54], 0xffff, 0x80ff);
      replaceMasked(words[55], 0xffff, 0x0080);
    }
    break;
  case Family::Scud:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceLow(words[16], value), replaceLow(words[45], value);
    if (SelectedValue(game, Setting::LinkMode, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    if (SelectedValue(game, Setting::LinkId, selection, value))
      replaceLow(words[15], value), replaceLow(words[44], value);
    if (SelectedValue(game, Setting::Cabinet, selection, value))
      replaceHigh(words[16], value), replaceHigh(words[45], value);
    break;
  case Family::SkiChamp:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::LinkMode, selection, value))
    {
      const uint16_t connected = value == 0 ? 0 : 1;
      const uint16_t slave = value == 2 ? 1 : 0;
      replaceLow(words[18], connected); replaceLow(words[47], connected);
      replaceHigh(words[19], slave); replaceHigh(words[48], slave);
    }
    if (SelectedValue(game, Setting::LinkId, selection, value))
      replaceLow(words[19], value), replaceLow(words[48], value);
    break;
  case Family::Spikeout:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceLow(words[16], value), replaceLow(words[45], value);
    if (SelectedValue(game, Setting::LinkMode, selection, value))
      replaceHigh(words[15], value), replaceHigh(words[44], value);
    break;
  case Family::SegaRally2:
    if (SelectedValue(game, Setting::Country, selection, value))
    {
      replaceMasked(words[12], 0xf000, value);
      replaceMasked(words[10], 0xffff, value == 0x9000 ? 0x6301 : 0x2701);
    }
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[12], 0x0038, value);
    if (SelectedValue(game, Setting::LinkMode, selection, value)) replaceMasked(words[12], 0x01c0, value);
    if (SelectedValue(game, Setting::Cabinet, selection, value))
    {
      replaceMasked(words[12], 0x0400, value);
      replaceMasked(words[13], 0x0410, value ? 0x0410 : 0x0000);
    }
    if (SelectedValue(game, Setting::DefaultView, selection, value))
      replaceMasked(words[12], 0x0004, value);
    break;
  case Family::StarWars:
    if (SelectedValue(game, Setting::Country, selection, value))
    {
      replaceMasked(words[12], 0xf000, value);
      replaceMasked(words[10], 0xffff, value == 0x9000 ? 0x6301 : 0x2701);
    }
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[12], 0x0038, value);
    if (SelectedValue(game, Setting::Cabinet, selection, value)) replaceMasked(words[12], 0x0400, value);
    if (SelectedValue(game, Setting::LeverFeedback, selection, value)) replaceMasked(words[13], 0x0001, value);
    if (selection.normalizeAnalogInputs || provideCalibrationData)
    {
      // X/Y calibration is stored as minimum, maximum and center values.
      replaceMasked(words[24], 0xffff, 0x00ff);
      replaceMasked(words[25], 0xffff, 0x8000);
      replaceMasked(words[26], 0xffff, 0xff80);
    }
    break;
  case Family::VirtuaFighter3:
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceBackup(122886, value);
    if (SelectedValue(game, Setting::Country, selection, value)) replaceBackup(122910, value);
    break;
  case Family::VirtualOn2:
    if (SelectedValue(game, Setting::Country, selection, value)) replaceHigh(words[12], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value)) replaceMasked(words[15], 0x00f0, value);
    if (SelectedValue(game, Setting::LinkMode, selection, value)) replaceLow(words[12], value);
    if (SelectedValue(game, Setting::LinkId, selection, value)) replaceHigh(words[25], value);
    if (SelectedValue(game, Setting::DisplayType, selection, value)) replaceHigh(words[24], value);
    break;
  case Family::Vs2:
  case Family::Vs298:
  case Family::Vs299:
    if (SelectedValue(game, Setting::Country, selection, value))
      replaceHigh(words[12], value), replaceHigh(words[41], value);
    if (SelectedValue(game, Setting::Difficulty, selection, value))
      replaceLow(words[15], value), replaceLow(words[44], value);
    break;
  default:
    return ApplyResult::Unsupported;
  }

  if (!changed)
    return ApplyResult::Unchanged;
  if (genibusLayout)
    words[5] = Crc16(words, 8, 63, 0xffff, 0xffff);
  else if (a3Layout)
    words[3] = SegaA3Checksum(words);
  else if (!backupLayout)
    words[3] = Crc16(words, 6, 34, 0, 0);
  return ApplyResult::Changed;
}

} // namespace NvramSettings
