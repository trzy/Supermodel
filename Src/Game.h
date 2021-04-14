#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include <string>
#include <memory>

struct Game
{
  std::string name;
  std::string parent;
  std::string title;
  std::string version;
  std::string manufacturer;
  unsigned year = 0;
  std::string stepping;
  std::string mpeg_board;
  std::string pci_bridge;               // overrides default PCI bridge type for stepping (empty string for default)
  uint32_t real3d_pci_id = 0;           // overrides default Real3D PCI ID for stepping (0 for default)
  float real3d_status_bit_set_percent_of_frame = 0; // overrides default status bit timing (0 for default)
  uint32_t encryption_key = 0;
  bool netboard_present;

  enum Inputs
  {
    INPUT_UI              = 0,          // special code reserved for Supermodel UI inputs
    INPUT_COMMON          = 0x00000001, // common controls (coins, service, test)
    INPUT_VEHICLE         = 0x00000002, // vehicle controls
    INPUT_JOYSTICK1       = 0x00000004, // joystick 1
    INPUT_JOYSTICK2       = 0x00000008, // joystick 2
    INPUT_FIGHTING        = 0x00000010, // fighting game controls
    INPUT_VR4             = 0x00000020, // four VR view buttons
    INPUT_VIEWCHANGE      = 0x00000040, // single view change button
    INPUT_SHIFT4          = 0x00000080, // 4-speed shifter
    INPUT_SHIFTUPDOWN     = 0x00000100, // up/down shifter
    INPUT_HANDBRAKE       = 0x00000200, // handbrake
    INPUT_HARLEY          = 0x00000400, // Harley Davidson controls
    INPUT_GUN1            = 0x00000800, // light gun 1
    INPUT_GUN2            = 0x00001000, // light gun 2
    INPUT_ANALOG_JOYSTICK = 0x00002000, // analog joystick
    INPUT_TWIN_JOYSTICKS  = 0x00004000, // twin joysticks
    INPUT_SOCCER          = 0x00008000, // soccer controls
    INPUT_SPIKEOUT        = 0x00010000, // Spikeout buttons
    INPUT_ANALOG_GUN1     = 0x00020000, // analog gun 1
    INPUT_ANALOG_GUN2     = 0x00040000, // analog gun 2
    INPUT_SKI             = 0x00080000, // ski controls
    INPUT_MAGTRUCK        = 0x00100000, // magical truck controls
    INPUT_FISHING         = 0x00200000, // fishing controls
    INPUT_ALL             = 0x003FFFFF
  };
  uint32_t inputs = 0;

  enum DriveBoardType
  {
    DRIVE_BOARD_NONE = 0,
    DRIVE_BOARD_WHEEL,
    DRIVE_BOARD_JOYSTICK,
    DRIVE_BOARD_SKI,
    DRIVE_BOARD_BILLBOARD
  };
  DriveBoardType driveboard_type = DriveBoardType::DRIVE_BOARD_NONE;
};

#endif  // INCLUDED_GAME_H
