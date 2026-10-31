#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include <string>
#include <memory>
#include <cstdint>

struct Game
{
  std::string name;
  std::string parent;
  std::string title;
  std::string version;
  std::string manufacturer;
  unsigned year = 0;
  unsigned polygon_memory = 0;  // Real3D Pro-1000: polygon memory in MB (0 = not given)
  enum Platform
  {
    PLATFORM_MODEL3 = 0,  // Sega Model 3 arcade hardware
    PLATFORM_PRO1000,     // Real3D Pro-1000 image generator
    PLATFORM_MODEL1       // Sega Model 1 arcade hardware
  };
  Platform platform = Platform::PLATFORM_MODEL3;
  // Hardware stepping. Games.xml: "1.0", "1.5", "2.0", or "2.1" (Model 3) and
  // "1100", "1200", or "1400" (Pro-1000 model). GameLoader makes sure that it
  // is a stepping of the platform. Ordered comparisons are permitted only
  // between steppings of the same platform.
  enum class Stepping
  {
    UNKNOWN = 0,
    MODEL3_1_0,     // Sega Model 3 Step 1.0
    MODEL3_1_5,     // Sega Model 3 Step 1.5
    MODEL3_2_0,     // Sega Model 3 Step 2.0
    MODEL3_2_1,     // Sega Model 3 Step 2.1
    PRO1000_1100,   // Real3D PRO-1100 (1 pixel processor)
    PRO1000_1200,   // Real3D PRO-1200 (2 pixel processors)
    PRO1000_1400,   // Real3D PRO-1400 (4 pixel processors)
    MODEL1          // Sega Model 1 (one stepping)
  };
  Stepping stepping = Stepping::UNKNOWN;

  // Returns the name of the stepping for display ("Model 3 Step 1.5",
  // "Pro-1100", ...)
  static const char *SteppingName(Stepping stepping)
  {
    switch (stepping)
    {
    case Stepping::MODEL3_1_0:    return "Model 3 Step 1.0";
    case Stepping::MODEL3_1_5:    return "Model 3 Step 1.5";
    case Stepping::MODEL3_2_0:    return "Model 3 Step 2.0";
    case Stepping::MODEL3_2_1:    return "Model 3 Step 2.1";
    case Stepping::PRO1000_1100:  return "Pro-1100";
    case Stepping::PRO1000_1200:  return "Pro-1200";
    case Stepping::PRO1000_1400:  return "Pro-1400";
    case Stepping::MODEL1:        return "Model 1";
    default:                      return "Unknown";
    }
  }

  // Returns true if the stepping is a Real3D Pro-1000 model
  static bool IsPro1000Stepping(Stepping stepping)
  {
    return stepping == Stepping::PRO1000_1100 || stepping == Stepping::PRO1000_1200 || stepping == Stepping::PRO1000_1400;
  }

  enum MpegBoardType
  {
    MPEG_BOARD_NONE = 0,
    MPEG_BOARD_DSB1,
    MPEG_BOARD_DSB2
  };
  MpegBoardType mpeg_board = MpegBoardType::MPEG_BOARD_NONE;
  enum AudioTypes
  {
      MONO = 0,             // Merge DSB+SCSP1+SCSP2 to 1 channel mono
      STEREO_LR,            // Merge DSP+SCSP1+SCSP2 to 2 channels stereo Left/Right (most common)
      STEREO_RL,            // Merge DSP+SCSP1+SCSP2 to 2 channels stereo reversed Right/Left
      QUAD_1_FLR_2_RLR,     // Split DSB+SCSP1 to FrontLeft/FrontRight and SCSP2 to RearLeft/RearRight (Daytona2)
      QUAD_1_FRL_2_RRL,     // Split DSB+SCSP1 to FrontRight/FrontLeft and SCSP2 to RearRight/RearLeft
      QUAD_1_RLR_2_FLR,     // Split DSB+SCSP1 to RearLeft/RearRight and SCSP2 to FrontLeft/FrontRight 
      QUAD_1_RRL_2_FRL,     // Split DSB+SCSP1 to RearRight/RearLeft and SCSP2 to FrontRight/FrontLeft
      QUAD_1_LR_2_FR_MIX,   // Specific srally2: Split SCSP2 and mix first channel to DSB+SCP11 Front Left/Right and second to Read Left/Right
  };
  AudioTypes audio = STEREO_LR;
  uint32_t encryption_key = 0;
  bool netboard_present = false;

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
    INPUT_THROTTLE        = 0x00400000, // throttle lever (Model 1 flight games)
    INPUT_ANALOG_JOYSTICK2= 0x00800000, // second analog joystick (Star Wars Arcade gunner)
    INPUT_FLIGHT_VIEW4    = 0x01000000, // four view buttons (Wing War)
    INPUT_ALL             = 0x01FFFFFF
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
