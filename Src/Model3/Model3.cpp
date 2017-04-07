/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2016 Bart Trzynadlowski, Nik Henson 
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
 * Model3.cpp
 * 
 * Implementation of the CModel3 class: a complete Model 3 machine.
 *
 * To-Do List
 * ----------
 *  - Save state format has changed slightly. No longer need dmaUnknownRegister
 *    in Real3D.cpp and security board-related variable was added to Model 3 
 *    state. PowerPC timing variables have changed. Before 0.3a release,
 *    important to change format version #.
 *  - ROM sets should probably be handled with a class that manages ROM
 *    loading, the game list, as well as ROM patching
 *  - Wrap up CPU emulation inside a class.
 *  - Update the to-do list! I forgot lots of other stuff here :)
 *
 * PowerPC Address Map (may be slightly out of date/incomplete)
 * -------------------
 * 00000000-007FFFFF  RAM
 * 84000000-8400003F  Real3D Status Registers
 * 88000000-88000007  Real3D Command Port
 * 8C000000-8C3FFFFF  Real3D Culling RAM (Low)
 * 8E000000-8E0FFFFF  Real3D Culling RAM (High)
 * 90000000-9000000B  Real3D VROM Texture Port
 * 94000000-940FFFFF  Real3D Texture FIFO
 * 98000000-980FFFFF  Real3D Polygon RAM
 * C0000000-C00000FF  SCSI (Step 1.x)
 * C1000000-C10000FF  SCSI (Step 1.x) (Lost World expects it here)
 * C2000000-C20000FF  Real3D DMA (Step 2.x)
 * F0040000-F004003F  Input (Controls) Registers
 * F0080000-F0080007  Sound Board Registers
 * F00C0000-F00DFFFF  Backup RAM
 * F0100000-F010003F  System Registers
 * F0140000-F014003F  Real-Time Clock
 * F0180000-F019FFFF  Security Board RAM
 * F01A0000-F01A003F  Security Board Registers
 * F0800CF8-F0800CFF  MPC105 CONFIG_ADDR (Step 1.x)
 * F0C00CF8-F0800CFF  MPC105 CONFIG_DATA (Step 1.x)
 * F1000000-F10F7FFF  Tile Generator Pattern Table
 * F10F8000-F10FFFFF  Tile Generator Name Table
 * F1100000-F111FFFF  Tile Generator Palette
 * F1180000-F11800FF  Tile Generator Registers
 * F8FFF000-F8FFF0FF  MPC105 (Step 1.x) or MPC106 (Step 2.x) Registers
 * F9000000-F90000FF  NCR 53C810 Registers (Step 1.x?)
 * FEC00000-FEDFFFFF  MPC106 CONFIG_ADDR (Step 2.x)
 * FEE00000-FEFFFFFF  MPC106 CONFIG_DATA (Step 2.x)
 * FF000000-FF7FFFFF  Banked CROM (CROMxx)
 * FF800000-FFFFFFFF  Fixed CROM
 *
 * Endianness
 * ----------
 * We assume a little endian machine and so for speed, PowerPC RAM and ROM
 * regions are byte reversed, which means that aligned words can be read and
 * written without any conversion. Problems arise when the PowerPC accesses 
 * little endian devices, like the tile generator, MPC10x, or Real3D. Then, the
 * access must be carried out carefully one byte at a time or by manually byte
 * reversing first (because the PowerPC will already have byte reversed it).
 *
 * System Registers
 * ----------------
 *
 * F0100014: IRQ Enable
 *   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+
 * | ? |SND| ? |NET|VD3|VD2|VBL|VD0|
 * +---+---+---+---+---+---+---+---+
 *    SND   SCSP (sound)
 *    NET   Network
 *    VD3   Unknown video-related
 *    VD2   Unknown video-related
 *    VBL   VBlank start
 *    VD0   Unknown video-related (?)
 *    0 = Disable, 1 = Enable
 *
 * Game Buttons
 * ------------
 *
 * For further information, see ReadInputs().
 *
 * Offset 0x04, bank 0:
 *
 *    7   6   5   4   3   2   1   0
 *  +---+---+---+---+---+---+---+---+
 *  | ? | ? |ST2|ST1|SVA|TSA|CN2|CN1|
 *  +---+---+---+---+---+---+---+---+
 *    CNx   Coin 1, Coin 2
 *    TSA   Test Button A
 *    SVA   Service Button A
 *    STx   Start 1, Start 2
 *
 * Offset 0x04, bank 1:
 *
 *    7   6   5   4   3   2   1   0
 *  +---+---+---+---+---+---+---+---+
 *  |TSB|SVB|EEP| ? | ? | ? | ? | ? |
 *  +---+---+---+---+---+---+---+---+
 *    EEP   Mapped to EEPROM (values written here are ignored)
 *    SVB   Service Button B
 *    TSB   Test Button B
 *
 * Offset 0x08:
 *
 *    7   6   5   4   3   2   1   0
 *  +---+---+---+---+---+---+---+---+
 *  |G27|G26|G25|G24|G23|G22|G21|G20|
 *  +---+---+---+---+---+---+---+---+
 *    G2x   Game-specific
 *
 * Offset 0x0C:
 *
 *    7   6   5   4   3   2   1   0
 *  +---+---+---+---+---+---+---+---+
 *  |G37|G36|G35|G34|G33|G32|G31|G30|
 *  +---+---+---+---+---+---+---+---+
 *    G3x   Game-specific
 *
 * Game-specific buttons:
 *
 *  Scud Race:
 *    G27   ---
 *    G26   Shift 2 when combined w/ G25, Shift 1 when combined w/ G24
 *    G25   Shift 4
 *    G24   Shift 3
 *    G23   VR4 Green
 *    G22   VR3 Yellow 
 *    G21   VR2 Blue 
 *    G20   VR1 Red
 *
 *  Virtua Fighter 3, Fighting Vipers 2:
 *    G27   Right
 *    G26   Left
 *    G25   Down
 *    G24   Up
 *    G23   Punch
 *    G22   Kick
 *    G21   Guard
 *    G20   Escape (VF3 only)
 *
 *  Sega Rally 2:
 *    G21   Hand Brake
 *    G20   View Change
 *
 *  Lost World, LA Machineguns:
 *    G20   Gun trigger
 *
 *  Star Wars Trilogy:
 *    G20   Event button
 *    G25   Trigger
 *
 *  Virtual On 2:
 *    G27   Left Lever Left
 *    G26   Left Lever Right
 *    G25   Left Lever Up
 *    G24   Left Lever Down
 *    G23   ---
 *    G22   ---
 *    G21   Left Turbo 
 *    G20   Left Shot Trigger
 *    G37   Right Lever Left
 *    G36   Right Lever Right
 *    G35   Right Lever Up
 *    G34   Right Lever Down
 *    G33   ---
 *    G32   ---
 *    G31   Right Turbo 
 *    G30   Right Shot Trigger
 *
 * Misc. Notes
 * -----------
 * 
 * daytona2:
 *  - Base address of program in CROM: 0x600000
 *  - 0x10019E is the location in RAM which contains link type.
 *  - Region menu can be accessed by entering test mode, holding start, and
 *    pressing: green, green, blue, yellow, red, yellow, blue 
 *   (VR4,4,2,3,1,3,2).
 */

#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Supermodel.h"
#include "Game.h"
#include "ROMSet.h"
#include "Util/Format.h"
#include "Util/ByteSwap.h"
#include <functional>
#include <set>
#include <iostream>

/******************************************************************************
 Model 3 Inputs
 
 Game controls. The EEPROM is mapped here as well.
******************************************************************************/

UINT8 CModel3::ReadInputs(unsigned reg)
{
  UINT8 adc[8];
  UINT8 data;
  
  reg &= 0x3F;
  switch (reg)
  {
  case 0x00:  // input bank
    return inputBank;

  case 0x04:  // current input bank

    data = 0xFF;

    if ((inputBank&1) == 0)
    {
      data &= ~(Inputs->coin[0]->value);        // Coin 1
      data &= ~(Inputs->coin[1]->value<<1);     // Coin 2
      data &= ~(Inputs->test[0]->value<<2);     // Test A
      data &= ~(Inputs->service[0]->value<<3);  // Service A
      data &= ~(Inputs->start[0]->value<<4);    // Start 1
      data &= ~(Inputs->start[1]->value<<5);    // Start 2
    }
    else
    {
      data &= ~(Inputs->service[1]->value<<6);  // Service B
      data &= ~(Inputs->test[1]->value<<7);     // Test B
      data = (data&0xDF)|(EEPROM.Read()<<5);    // bank 1 contains EEPROM data bit
    }

    if ((m_game.inputs & Game::INPUT_SKI))
    {
      if ((inputBank&1) == 0)
      {
        data &= ~(Inputs->skiPollLeft->value<<5);
        data &= ~(Inputs->skiSelect1->value<<6);
        data &= ~(Inputs->skiSelect2->value<<7);
        data &= ~(Inputs->skiSelect3->value<<4);
      }
    }

    return data;

  case 0x08:  // game-specific inputs

    data = 0xFF;

    if ((m_game.inputs & Game::INPUT_SKI))
    {
      data &= ~(Inputs->skiPollRight->value<<0);
    }

    if ((m_game.inputs & Game::INPUT_JOYSTICK1))
    {
      data &= ~(Inputs->up[0]->value<<5);     // P1 Up
      data &= ~(Inputs->down[0]->value<<4);   // P1 Down
      data &= ~(Inputs->left[0]->value<<7);   // P1 Left
      data &= ~(Inputs->right[0]->value<<6);  // P1 Right
    }

    if ((m_game.inputs & Game::INPUT_FIGHTING))
    {
      data &= ~(Inputs->escape[0]->value<<3); // P1 Escape
      data &= ~(Inputs->guard[0]->value<<2);  // P1 Guard
      data &= ~(Inputs->kick[0]->value<<1);   // P1 Kick
      data &= ~(Inputs->punch[0]->value<<0);  // P1 Punch
    }
    
    if ((m_game.inputs & Game::INPUT_SPIKEOUT))
    {
      data &= ~(Inputs->shift->value<<2);     // Shift
      data &= ~(Inputs->beat->value<<0);      // Beat
      data &= ~(Inputs->charge->value<<1);    // Charge
      data &= ~(Inputs->jump->value<<3);      // Jump
    }
    
    if ((m_game.inputs & Game::INPUT_SOCCER))
    {
      data &= ~(Inputs->shortPass[0]->value<<2);  // P1 Short Pass
      data &= ~(Inputs->longPass[0]->value<<0);   // P1 Long Pass
      data &= ~(Inputs->shoot[0]->value<<1);      // P1 Shoot
    }

    if ((m_game.inputs & Game::INPUT_VR4))
    {
      data &= ~(Inputs->vr[0]->value<<0); // VR1 Red
      data &= ~(Inputs->vr[1]->value<<1); // VR2 Blue
      data &= ~(Inputs->vr[2]->value<<2); // VR3 Yellow
      data &= ~(Inputs->vr[3]->value<<3); // VR4 Green
    }
  
    if ((m_game.inputs & Game::INPUT_VIEWCHANGE))
    {
      // Harley is wired slightly differently
      if ((m_game.inputs & Game::INPUT_HARLEY))
        data &= ~(Inputs->viewChange->value<<1);  // View change
      else
        data &= ~(Inputs->viewChange->value<<0);  // View change
    }
    
    if ((m_game.inputs & Game::INPUT_SHIFT4))
    {
      if (Inputs->gearShift4->value == 2)       // Shift 2
        data &= ~0x60;
      else if (Inputs->gearShift4->value == 4)  // Shift 4
        data &= ~0x20;
      if (Inputs->gearShift4->value == 1)       // Shift 1
        data &= ~0x50;
      else if (Inputs->gearShift4->value == 3)  // Shift 3
        data &= ~0x10;
    }

    if ((m_game.inputs & Game::INPUT_SHIFTUPDOWN))
    {
      // Harley is wired slightly differently
      if ((m_game.inputs & Game::INPUT_HARLEY))
      {
        if (Inputs->gearShiftUp->value)         // Shift up 
          data &= ~0x20;
        else if (Inputs->gearShiftDown->value)  // Shift down
          data &= ~0x10;
      }
      else
      {
        if (Inputs->gearShiftUp->value)         // Shift up
          data &= ~0x50;
        else if (Inputs->gearShiftDown->value)  // Shift down
          data &= ~0x60;
      }
    }
    
    if ((m_game.inputs & Game::INPUT_HANDBRAKE))
      data &= ~(Inputs->handBrake->value<<1);   // Hand brake
    
    if ((m_game.inputs & Game::INPUT_HARLEY))
      data &= ~(Inputs->musicSelect->value<<0); // Music select
    
    if ((m_game.inputs & Game::INPUT_GUN1))
      data &= ~(Inputs->trigger[0]->value<<0);  // P1 Trigger
    
    if ((m_game.inputs & Game::INPUT_ANALOG_JOYSTICK))
    {
      data &= ~(Inputs->analogJoyTrigger1->value<<5); // Trigger 1
      data &= ~(Inputs->analogJoyTrigger2->value<<4); // Trigger 2
      data &= ~(Inputs->analogJoyEvent1->value<<0);   // Event Button 1
      data &= ~(Inputs->analogJoyEvent2->value<<1);   // Event Button 2
    }
    
    if ((m_game.inputs & Game::INPUT_TWIN_JOYSTICKS)) // First twin joystick
    {
      /*
       * Process left joystick inputs first
       */
       
      // Shot trigger and Turbo
      data &= ~(Inputs->twinJoyShot1->value<<0);
      data &= ~(Inputs->twinJoyTurbo1->value<<1);
      
      // Stick
      data &= ~(Inputs->twinJoyLeft1->value<<7);
      data &= ~(Inputs->twinJoyRight1->value<<6);
      data &= ~(Inputs->twinJoyUp1->value<<5);
      data &= ~(Inputs->twinJoyDown1->value<<4);
      
      /*
       * Next, process twin joystick macro inputs (higher level inputs
       * that map to actions on both joysticks simultaneously).
       */
       
      /*
       * Forward/reverse/turn are mutually exclusive.
       *
       * Turn Left:   1D 2U
       * Turn Right:  1U 2D
       * Forward:     1U 2U
       * Reverse:     1D 2D
       */
      if (Inputs->twinJoyTurnLeft->value)
        data &= ~0x10;
      else if (Inputs->twinJoyTurnRight->value)
        data &= ~0x20;
      else if (Inputs->twinJoyForward->value)
        data &= ~0x20;
      else if (Inputs->twinJoyReverse->value)
        data &= ~0x10;
        
      /*
       * Strafe/crouch/jump are mutually exclusive.
       *
       * Strafe Left:   1L 2L
       * Strafe Right:  1R 2R
       * Jump:          1L 2R
       * Crouch:        1R 2L
       */
      if (Inputs->twinJoyStrafeLeft->value)
        data &= ~0x80;
      else if (Inputs->twinJoyStrafeRight->value)
        data &= ~0x40;
      else if (Inputs->twinJoyJump->value)
        data &= ~0x80;
      else if (Inputs->twinJoyCrouch->value)
        data &= ~0x40;
    }

    if ((m_game.inputs & Game::INPUT_ANALOG_GUN1))
    {
      data &= ~(Inputs->analogTriggerLeft[0]->value<<0);
      data &= ~(Inputs->analogTriggerRight[0]->value<<1);
    }

    if ((m_game.inputs & Game::INPUT_MAGTRUCK))
      data &= ~(Inputs->magicalPedal1->value << 0);

    if ((m_game.inputs & Game::INPUT_FISHING))
    {
      data &= ~(Inputs->fishingCast->value << 0);
      data &= ~(Inputs->fishingSelect->value << 1);
    }
    return data;

  case 0x0C:  // game-specific inputs

    data = 0xFF;

    if (DriveBoard.IsAttached())
      data = DriveBoard.Read();
    
    if ((m_game.inputs & Game::INPUT_JOYSTICK2))
    {
      data &= ~(Inputs->up[1]->value<<5);     // P2 Up
      data &= ~(Inputs->down[1]->value<<4);   // P2 Down
      data &= ~(Inputs->left[1]->value<<7);   // P2 Left
      data &= ~(Inputs->right[1]->value<<6);  // P2 Right
    }

    if ((m_game.inputs & Game::INPUT_FIGHTING))
    {
      data &= ~(Inputs->escape[1]->value<<3); // P2 Escape
      data &= ~(Inputs->guard[1]->value<<2);  // P2 Guard
      data &= ~(Inputs->kick[1]->value<<1);   // P2 Kick
      data &= ~(Inputs->punch[1]->value<<0);  // P2 Punch
    }
    
    if ((m_game.inputs & Game::INPUT_SOCCER))
    {
      data &= ~(Inputs->shortPass[1]->value<<2);  // P2 Short Pass
      data &= ~(Inputs->longPass[1]->value<<0);   // P2 Long Pass
      data &= ~(Inputs->shoot[1]->value<<1);      // P2 Shoot
    }
    
    if ((m_game.inputs & Game::INPUT_TWIN_JOYSTICKS)) // Second twin joystick (see register 0x08 for comments)
    {
            
      data &= ~(Inputs->twinJoyShot2->value<<0);
      data &= ~(Inputs->twinJoyTurbo2->value<<1);
      
      data &= ~(Inputs->twinJoyLeft2->value<<7);
      data &= ~(Inputs->twinJoyRight2->value<<6);
      data &= ~(Inputs->twinJoyUp2->value<<5);
      data &= ~(Inputs->twinJoyDown2->value<<4);

      if (Inputs->twinJoyTurnLeft->value)
        data &= ~0x20;
      else if (Inputs->twinJoyTurnRight->value)
        data &= ~0x10;
      else if (Inputs->twinJoyForward->value)
        data &= ~0x20;
      else if (Inputs->twinJoyReverse->value)
        data &= ~0x10;
      
      if (Inputs->twinJoyStrafeLeft->value)
        data &= ~0x80;
      else if (Inputs->twinJoyStrafeRight->value)
        data &= ~0x40;
      else if (Inputs->twinJoyJump->value)
        data &= ~0x40;
      else if (Inputs->twinJoyCrouch->value)
        data &= ~0x80;
    }
    
    if ((m_game.inputs & Game::INPUT_GUN2))
      data &= ~(Inputs->trigger[1]->value<<0);  // P2 Trigger
    
    if ((m_game.inputs & Game::INPUT_ANALOG_GUN2))
    {
      data &= ~(Inputs->analogTriggerLeft[1]->value<<0);
      data &= ~(Inputs->analogTriggerRight[1]->value<<1);
    }

    if ((m_game.inputs & Game::INPUT_MAGTRUCK))
      data &= ~(Inputs->magicalPedal2->value << 0);

    return data;

  case 0x2C:  // Serial FIFO 1
    return serialFIFO1;
    
  case 0x30:  // Serial FIFO 2
    return serialFIFO2;
    
  case 0x34:  // Serial FIFO full/empty flags
    return 0x0C;  
    
  case 0x3C:  // ADC

    // Load ADC channels with input data
    memset(adc, 0, sizeof(adc));
    if ((m_game.inputs & Game::INPUT_VEHICLE))
    {
      adc[0] = (UINT8)Inputs->steering->value;
      adc[1] = (UINT8)Inputs->accelerator->value;
      adc[2] = (UINT8)Inputs->brake->value;
      if ((m_game.inputs & Game::INPUT_HARLEY))
        adc[3] = (UINT8)Inputs->rearBrake->value;
    }

    if ((m_game.inputs & Game::INPUT_ANALOG_JOYSTICK))
    {
      adc[0] = (UINT8)Inputs->analogJoyY->value;
      adc[1] = (UINT8)Inputs->analogJoyX->value;
    }

    if ((m_game.inputs & Game::INPUT_ANALOG_GUN1)||(m_game.inputs & Game::INPUT_ANALOG_GUN2))
    { 
      adc[0] = (UINT8)Inputs->analogGunX[0]->value;
      adc[2] = (UINT8)Inputs->analogGunY[0]->value;
      adc[1] = (UINT8)Inputs->analogGunX[1]->value;
      adc[3] = (UINT8)Inputs->analogGunY[1]->value;
    }
    
    if ((m_game.inputs & Game::INPUT_SKI))
    {
      adc[0] = (UINT8)Inputs->skiY->value;
      adc[1] = (UINT8)Inputs->skiX->value;
    }

    if ((m_game.inputs & Game::INPUT_MAGTRUCK))
    {
      adc[0] = uint8_t(Inputs->magicalLever1->value);
      adc[1] = uint8_t(Inputs->magicalLever2->value);
    }
      
    if ((m_game.inputs & Game::INPUT_FISHING))
    {
      adc[0] = uint8_t(Inputs->fishingRodY->value);
      adc[1] = uint8_t(Inputs->fishingRodX->value);
      adc[3] = uint8_t(Inputs->fishingReel->value);
      adc[5] = uint8_t(Inputs->fishingStickX->value);
      adc[4] = uint8_t(Inputs->fishingStickY->value);
    }

    // Read out appropriate channel
    data = adc[adcChannel&7];
    ++adcChannel;
    return data;
  
  default:
    break;
  }
  
  return 0xFF;  // controls are active low
}

void CModel3::WriteInputs(unsigned reg, UINT8 data)
{
  switch (reg&0x3F)
  {
  case 0:
    EEPROM.Write((data>>6)&1,(data>>7)&1,(data>>5)&1);
    inputBank = data;
    break;

  case 0x10:  // Drive board
    if (DriveBoard.IsAttached())
      DriveBoard.Write(data);
    if (NULL != Outputs) // TODO - check gameInputs
      Outputs->SetValue(OutputRawDrive, data);
    break;

  case 0x14:  // Lamp outputs (Daytona/Scud Race/Sega Rally/Le Mans 24)
    if (NULL != Outputs) // TODO - check gameInputs
    {
      Outputs->SetValue(OutputLampStart, !!(data&0x04));
      Outputs->SetValue(OutputLampView1, !!(data&0x08));
      Outputs->SetValue(OutputLampView2, !!(data&0x10));
      Outputs->SetValue(OutputLampView3, !!(data&0x20));
      Outputs->SetValue(OutputLampView4, !!(data&0x40));
      Outputs->SetValue(OutputLampLeader, !!(data&0x80));
      Outputs->SetValue(OutputRawLamps, data);
    }
    break;

  case 0x24:  // Serial FIFO 1
    switch (data) // Command
    {
    case 0x00:    // Light gun register select
      gunReg = serialFIFO2;
      break;
    case 0x87:    // Read light gun register
      serialFIFO1 = 0;  // clear serial FIFO 1
      serialFIFO2 = 0;
      if ((m_game.inputs & Game::INPUT_GUN1||m_game.inputs & Game::INPUT_GUN2))
      {
        switch (gunReg)
        {
        case 0: // Player 1 gun Y (low 8 bits)
          serialFIFO2 = Inputs->gunY[0]->value&0xFF;
          break;
        case 1: // Player 1 gun Y (high 2 bits)
          serialFIFO2 = (Inputs->gunY[0]->value>>8)&3;
          break;
        case 2: // Player 1 gun X (low 8 bits)
          serialFIFO2 = Inputs->gunX[0]->value&0xFF;
          break;
        case 3: // Player 1 gun X (high 2 bits)
          serialFIFO2 = (Inputs->gunX[0]->value>>8)&3;
          break;
        case 4: // Player 2 gun Y (low 8 bits)
          serialFIFO2 = Inputs->gunY[1]->value&0xFF;
          break;
        case 5: // Player 2 gun Y (high 2 bits)
          serialFIFO2 = (Inputs->gunY[1]->value>>8)&3;
          break;
        case 6: // Player 2 gun X (low 8 bits)
          serialFIFO2 = Inputs->gunX[1]->value&0xFF;
          break;
        case 7: // Player 2 gun X (high 2 bits)
          serialFIFO2 = (Inputs->gunX[1]->value>>8)&3;
          break;
        case 8: // Off-screen indicator (bit 0 = player 1, bit 1 = player 2, set indicates off screen)
          serialFIFO2 = (Inputs->trigger[1]->offscreenValue<<1)|Inputs->trigger[0]->offscreenValue;
          break;
        default:
          DebugLog("Unknown gun register: %X\n", gunReg);
          break;
        }
      }
      break;
    default:
      DebugLog("Uknown command to serial FIFO: %02X\n", data);
      break;
    }
    break;
  case 0x28:  // Serial FIFO 2
    serialFIFO2 = data;
    break;
  case 0x3C:
    adcChannel = data&7;
    break;
  default:
    break;
  }
  //printf("Controls: %X=%02X\n", reg, data);
}


/******************************************************************************
 Model 3 Security Device
 
 The security device is present in some games. Virtual On and Dirt Devils read
 tile pattern data from it. Spikeout calls a routine at PC=0x6FAC8 that writes/
 reads the security device and, if the return value in R3 is 0, prints "ILLEGAL
 ROM" and locks the game. Our habit of returning all 1's for unknown reads 
 seems to help avoid this.
******************************************************************************/

uint16_t CModel3::ReadSecurityRAM(uint32_t addr)
{
  if (addr < 0x8000)
    return (*(uint32_t *) &securityRAM[addr * 4]) >> 16;
  return 0;
}
  
UINT32 CModel3::ReadSecurity(unsigned reg)
{
  switch (reg)
  {
  case 0x00:  // Status
    return 0;
  case 0x1C:  // Data
    if (m_securityFirstRead)
    {
      m_securityFirstRead = false;
      return 0xFFFF0000;
    }
    else
    {
      uint8_t *base_ptr;
      return m_cryptoDevice.Decrypt(&base_ptr) << 16;
    }
  default:
    DebugLog("Security read: reg=%X\n", reg);
    break;
  }
  
  return 0xFFFFFFFF;
}

void CModel3::WriteSecurity(unsigned reg, UINT32 data)
{
  switch (reg)
  {
  case 0x10:
  case 0x14:
    m_cryptoDevice.SetAddressLow(0);
    m_cryptoDevice.SetAddressHigh(0);
    m_securityFirstRead = true;
    break;
  case 0x18:
  {
    uint16_t subKey = data >> 16;
    subKey = ((subKey & 0xFF00) >> 8) | ((subKey & 0x00FF) << 8);
    m_cryptoDevice.SetSubKey(subKey);
    break;
  }
  default:
    DebugLog("Security write: reg=%X, data=%08X (PC=%08X, LR=%08X)\n", reg, data, ppc_get_pc(), ppc_get_lr()); 
    break;
  }
}


/******************************************************************************
 PCI Devices
 
 Unknown PCI devices are handled here. 
******************************************************************************/

UINT32 CModel3::ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset)
{ 
  if ((bits==8) || (bits==16))
  {
    DebugLog("Model 3: %d-bit PCI read request for reg=%02X\n", bits, reg);
    return 0;
  }
  
  switch (device)
  {
  case 16:  // Used by Daytona 2
    switch (reg)
    {
    case 0: // PCI vendor and device ID
      return 0x182711DB;
    default:
      break;
    }
  default:
    break;
  }

  DebugLog("Model 3: PCI %d-bit write request for device=%d, reg=%02X\n", bits, device, reg);
  return 0;
}
  
void CModel3::WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset, UINT32 data)
{
  DebugLog("Model 3: PCI %d-bit write request for device=%d, reg=%02X, data=%08X\n", bits, device, reg, data);
}


/******************************************************************************
 Model 3 System Registers
 
 NOTE: Proper IRQ handling requires a "deassert" function in the PowerPC core,
 which the interpreter presently lacks. This is because different modules that
 generate IRQs, like the tilegen, Real3D, and SCSP, should each call
 IRQ.Assert() on their own, which will assert the CPU IRQ line. Right now,
 the CPU processes an interrupt and clears the line by itself, which means that
 if multiple interrupts are asserted simultaneously, depending on the IRQ
 handler code, only one may be processed. Keep an eye on this!
******************************************************************************/

// Set the CROM bank index (active low logic)
void CModel3::SetCROMBank(unsigned idx)
{
  cromBankReg = idx;
  idx = (~idx) & 0xF;
  cromBank = &crom[0x800000 + (idx*0x800000)];
  DebugLog("CROM bank setting: %d (%02X), PC=%08X, LR=%08X\n", idx, cromBankReg, ppc_get_pc(), ppc_get_lr());
}

UINT8 CModel3::ReadSystemRegister(unsigned reg)
{
  switch (reg&0x3F)
  {
  case 0x08:  // CROM bank
    return cromBankReg;
  case 0x14:  // IRQ enable
    return IRQ.ReadIRQEnable();
  case 0x18:  // IRQ pending
    return IRQ.ReadIRQState();
  case 0x1C:  // unknown (apparently expects some or all bits set)
    //DebugLog("System register %02X read\n", reg);
    return 0xFF;
  case 0x10:  // JTAG Test Access Port
    return (GPU.ReadTAP()<< 5);
  default:    
    //DebugLog("System register %02X read\n", reg);
    break;
  }
  
  return 0xFF;
}

void CModel3::WriteSystemRegister(unsigned reg, UINT8 data)
{
  switch (reg&0x3F)
  {
  case 0x08:  // CROM bank
    SetCROMBank(data);
    break;
  case 0x14:  // IRQ enable
    IRQ.WriteIRQEnable(data);
    DebugLog("IRQ ENABLE=%02X\n", data);
    break;
  case 0x18:  // IRQ acknowledge
  {
    // MAME: Clear bits are ack. Reverse order from other IRQ regs.
    //uint8_t ack = 0;
    //for (int bit = 0; bit < 8; bit++)
    //{
    //  if (!(data & (1 << bit)))
    //    ack |= (1 << (7-bit));
    //}
    //IRQ.Deassert(ack);
    DebugLog("IRQ ACK? %02X=%02X\n", reg, data);
    break;
  }
  case 0x0C:  // JTAG Test Access Port
    GPU.WriteTAP((data>>6)&1,(data>>2)&1,(data>>5)&1,(data>>7)&1);  // TCK, TMS, TDI, TRST
    break;
  case 0x0D:
  case 0x0E:
  case 0x0F:
  case 0x1C:  // LED control?
    break;
  default:
    //DebugLog("System register %02X=%02X\n", reg, data);
    break;
  }
}


/******************************************************************************
 Address Space Access Handlers
 
 NOTE: Testing of some of the address ranges is not strict enough, especially
 for the MPC10x. Write32() handles the MPC10x most correctly.
******************************************************************************/

/*
 * CModel3::Read8(addr):
 * CModel3::Read16(addr):
 * CModel3::Read32(addr):
 * CModel3::Read64(addr):
 *
 * Read handlers.
 */
UINT8 CModel3::Read8(UINT32 addr)
{
  // RAM (most frequently accessed)
  if (addr<0x00800000)
    return ram[addr^3];
  
  // Other
  switch ((addr>>24))
  {
  // CROM
  case 0xFF:
    if (addr < 0xFF800000)
      return cromBank[(addr&0x7FFFFF)^3];
    else
      return crom[(addr&0x7FFFFF)^3];

  // Real3D DMA
  case 0xC2:
    return GPU.ReadDMARegister8(addr&0xFF);

  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Inputs
    case 0x04:
      return ReadInputs(addr&0x3F);
    
    // Sound Board
    case 0x08:
      if ((addr & 0xF) == 4)  // MIDI control port
        return 0x83;          // magtruck country check
      else
        return 0;
      break;

    // System registers
    case 0x10:    
      return ReadSystemRegister(addr&0x3F);

    // RTC    
    case 0x14:
      if ((addr&3)==1)  // battery voltage test
        return 0x03;
      else if ((addr&3)==0)
        return RTC.ReadRegister((addr>>2)&0xF);
      return 0;

    // Unknown
    default:
      break;
    }

    break;

  // Tile generator
  case 0xF1:
    if (addr < 0xF1120000)
    {
      // Tile generator accesses its RAM as little endian, no adjustment needed here
      return TileGen.ReadRAM8(addr&0x1FFFFF);
    }
    break;

  // 53C810 SCSI
  case 0xC0:  // only on Step 1.0
    if (m_game.stepping != "1.0")
      break;
  case 0xF9:
  case 0xC1:
    return SCSI.ReadRegister(addr&0xFF);

  // Unknown  
  default:
    break;
  }

  DebugLog("PC=%08X\tread8 : %08X\n", ppc_get_pc(), addr);
  return 0xFF;
}

UINT16 CModel3::Read16(UINT32 addr)
{
  UINT16  data;
  
  if ((addr&1))
  {
    data =  Read8(addr+0)<<8;
    data |= Read8(addr+1);
    return data;
  }

  // RAM (most frequently accessed) 
  if (addr<0x00800000)
    return *(UINT16 *) &ram[addr^2];

  // Other
  switch ((addr>>24))
  {
  // CROM
  case 0xFF:
    if (addr < 0xFF800000)
      return *(UINT16 *) &cromBank[(addr&0x7FFFFF)^2];
    else
      return *(UINT16 *) &crom[(addr&0x7FFFFF)^2];
  
  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Backup RAM
    case 0x0C:
    case 0x0D:
      return *(UINT16 *) &backupRAM[(addr&0x1FFFF)^2];
      
    // Sound Board
    case 0x08:
      //printf("PPC: Read16 %08X\n", addr);
      break;

    // MPC105
    case 0xC0:  // F0C00CF8
      return PCIBridge.ReadPCIConfigData(16,addr&3);

    // MPC106
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
    case 0xE7:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
      return PCIBridge.ReadPCIConfigData(16,addr&3);

    // Unknown
    default:
      break;
    }

    break;

  // Tile generator
  case 0xF1:
    if (addr < 0xF1120000)
    {
      // Tile generator accesses its RAM as little endian, no adjustment needed here
      uint16_t data = TileGen.ReadRAM16(addr&0x1FFFFF);
      return FLIPENDIAN16(data);
    }
    break;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread16: %08X\n", ppc_get_pc(), addr);
  return 0xFFFF;
}

UINT32 CModel3::Read32(UINT32 addr)
{
  UINT32  data;

  if ((addr&3))
  {
    data =  Read16(addr+0)<<16;
    data |= Read16(addr+2);
    return data;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
    return *(UINT32 *) &ram[addr];

  // Other
  switch ((addr>>24))
  {
  // CROM
  case 0xFF:    
    if (addr < 0xFF800000)
      return *(UINT32 *) &cromBank[(addr&0x7FFFFF)];
    else
      return *(UINT32 *) &crom[(addr&0x7FFFFF)];

  // Real3D registers
  case 0x84:
    return GPU.ReadRegister(addr&0x3F);
    
  // Real3D DMA
  case 0xC2:
    data = GPU.ReadDMARegister32(addr&0xFF);
    return FLIPENDIAN32(data);

  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Inputs   
    case 0x04:
      data =  ReadInputs((addr&0x3F)+0) << 24;
      data |= ReadInputs((addr&0x3F)+1) << 16;
      data |= ReadInputs((addr&0x3F)+2) << 8;
      data |= ReadInputs((addr&0x3F)+3) << 0;
      return data;
    
    // Sound Board
    case 0x08:
      //printf("PPC: Read32 %08X\n", addr);
      break;

    // Backup RAM
    case 0x0C:
    case 0x0D:
      return *(UINT32 *) &backupRAM[(addr&0x1FFFF)];

    // System registers
    case 0x10:
      data =  ReadSystemRegister((addr&0x3F)+0) << 24;
      data |= ReadSystemRegister((addr&0x3F)+1) << 16;
      data |= ReadSystemRegister((addr&0x3F)+2) << 8;
      data |= ReadSystemRegister((addr&0x3F)+3) << 0;
      return data;

    // MPC105
    case 0xC0:  // F0C00CF8
      return PCIBridge.ReadPCIConfigData(32,0);

    // MPC106
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
    case 0xE7:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
      return PCIBridge.ReadPCIConfigData(32,0);

    // RTC
    case 0x14:
      data = (RTC.ReadRegister((addr>>2)&0xF) << 24);
      data |= 0x00030000; // set these bits to pass battery voltage test
      return data;

    // Security board RAM
    case 0x18:
    case 0x19:
      return *(UINT32 *) &securityRAM[(addr&0x1FFFF)];  // so far, only 32-bit access observed, so we use little endian access

    // Security board registers
    case 0x1A:
      return ReadSecurity(addr&0x3F);
    
    // Unknown
    default:
      break;
    }

    break;

  // Tile generator
  case 0xF1:
    // Tile generator accesses its RAM as little endian, must flip for big endian PowerPC
    if (addr < 0xF1120000)
    {
      data = TileGen.ReadRAM32(addr&0x1FFFFF);
      return FLIPENDIAN32(data);
    }
    else if ((addr>=0xF1180000) && (addr<0xF1180100))
    {
      data = TileGen.ReadRegister(addr & 0xFF);
      return FLIPENDIAN32(data);
    }

    break;

  // 53C810 SCSI
  case 0xC0:  // only on Step 1.0
    if (m_game.stepping != "1.0") // check for Step 1.0
      break;
  case 0xF9:
  case 0xC1:
    data =  (SCSI.ReadRegister((addr+0)&0xFF) << 24);
    data |= (SCSI.ReadRegister((addr+1)&0xFF) << 16);
    data |= (SCSI.ReadRegister((addr+2)&0xFF) << 8);
    data |= (SCSI.ReadRegister((addr+3)&0xFF) << 0);
    return data;

  // Unknown
  default:
    break;
  }

  DebugLog("PC=%08X\tread32: %08X\n", ppc_get_pc(), addr);
  return 0xFFFFFFFF;
}

UINT64 CModel3::Read64(UINT32 addr)
{
  UINT64  data;

  data = Read32(addr+0);
  data <<= 32;
  data |= Read32(addr+4);

  return data;
}

/*
 * CModel3::Write8(addr, data):
 * CModel3::Write16(addr, data):
 * CModel3::Write32(addr, data):
 * CModel3::Write64(addr, data):
 *
 * Write handlers.
 */
void CModel3::Write8(UINT32 addr, UINT8 data)
{
  // RAM (most frequently accessed)
  if (addr < 0x00800000)
  {
    ram[addr^3] = data;
    return;
  }

  // Other
  switch ((addr>>24))
  {
  // Real3D DMA
  case 0xC2:
    GPU.WriteDMARegister8(addr&0xFF,data);
    break;

  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Inputs   
    case 0x04:  
      WriteInputs(addr&0x3F,data);
      break;
      
    // Sound Board
    case 0x08:
      //printf("PPC: %08X=%02X * (PC=%08X, LR=%08X)\n", addr, data, ppc_get_pc(), ppc_get_lr());
      if ((addr&0xF) == 0)      // MIDI data port
        SoundBoard.WriteMIDIPort(data);
      else if ((addr&0xF) == 4) // MIDI control port
        midiCtrlPort = data;
      break;
  
    // Backup RAM
    case 0x0C:
    case 0x0D:
      backupRAM[(addr&0x1FFFF)^3] = data;
      break;

    // System registers
    case 0x10:
      WriteSystemRegister(addr&0x3F,data);
      break;

    // RTC
    case 0x14:
      if ((addr&3)==0)
        RTC.WriteRegister((addr>>2)&0xF,data);
      break;
    
    // Unknown
    default:
      break;
    }

    DebugLog("PC=%08X\twrite8 : %08X=%02X\n", ppc_get_pc(), addr, data);    
    break;

  // Tile generator
  case 0xF1:
    if (addr < 0xF1120000)
    {
      // Tile generator accesses its RAM as little endian, no adjustment needed here
      TileGen.WriteRAM8(addr&0x1FFFFF, data);
      break;
    }
    goto Unknown8;

  // MPC105/106
  case 0xF8:
    PCIBridge.WriteRegister(addr&0xFF,data);
    break;

  // 53C810 SCSI
  case 0xC0:  // only on Step 1.0
    if (m_game.stepping != "1.0")
      goto Unknown8;
  case 0xF9:
  case 0xC1:
    SCSI.WriteRegister(addr&0xFF,data);
    break;

  // Unknown:
  default:
  Unknown8:
    DebugLog("PC=%08X\twrite8 : %08X=%02X\n", ppc_get_pc(), addr, data);
    break;
  }
}

void CModel3::Write16(UINT32 addr, UINT16 data)
{
  if ((addr&1))
  {
    Write8(addr+0,data>>8);
    Write8(addr+1,data&0xFF);
    return;
  }

  // RAM (most frequently accessed)
  if (addr < 0x00800000)
  {
    *(UINT16 *) &ram[addr^2] = data;
    return;
  }

  // Other
  switch ((addr>>24))
  {
  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Sound Board
    case 0x08:
      //printf("%08X=%04X\n", addr, data);
      break;
      
    // Backup RAM
    case 0x0C:
    case 0x0D:
      *(UINT16 *) &backupRAM[(addr&0x1FFFF)^2] = data;
      break;
    
    // MPC105
    case 0xC0:  // F0C00CF8
      PCIBridge.WritePCIConfigData(16,addr&2,data);
      break;

    // Unknown
    default:
      break;
    }

    DebugLog("PC=%08X\twrite16 : %08X=%04X\n", ppc_get_pc(), addr, data);   
    break;

  // Tile generator
  case 0xF1:
    if (addr < 0xF1120000)
    {
      // Tile generator accesses its RAM as little endian, no adjustment needed here
      TileGen.WriteRAM16(addr&0x1FFFFF, FLIPENDIAN16(data));
    }
    goto Unknown16;
    
  // MPC105/106
  case 0xF8:
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr&0xFF)+0,data>>8);
    PCIBridge.WriteRegister((addr&0xFF)+1,data&0xFF);
    break;

  // Unknown
  default:
  Unknown16:
    DebugLog("PC=%08X\twrite16: %08X=%04X\n", ppc_get_pc(), addr, data);
    break;
  }
} 

void CModel3::Write32(UINT32 addr, UINT32 data)
{
  if ((addr&3))
  {
    Write16(addr+0,data>>16);
    Write16(addr+2,data);
    return;
  }

  // RAM (most frequently accessed)
  if (addr<0x00800000)
  {
    *(UINT32 *) &ram[addr] = data;
    return;
  }

  // Other
  switch ((addr>>24))
  {
  // Real3D trigger
  case 0x88:  // 88000000
    GPU.Flush();
    break;
  
  // Real3D low culling RAM
  case 0x8C:  // 8C000000-8C400000
    GPU.WriteLowCullingRAM(addr&0x3FFFFF,FLIPENDIAN32(data));
    break;

  // Real3D high culling RAM
  case 0x8E:  // 8E000000-8E100000
    GPU.WriteHighCullingRAM(addr&0xFFFFF,FLIPENDIAN32(data));
    break;

  // Real3D texture port
  case 0x90:  // 90000000-90??????
    GPU.WriteTexturePort(addr&0xFF,FLIPENDIAN32(data));
    break;

  // Real3D texture FIFO
  case 0x94:  // 94000000-94100000
    GPU.WriteTextureFIFO(FLIPENDIAN32(data));
    break;

  // Real3D polygon RAM
  case 0x98:  // 98000000-98400000
    GPU.WritePolygonRAM(addr&0x3FFFFF,FLIPENDIAN32(data));
    break;

  // Real3D configuration registers
  case 0x9C:  // 9Cxxxxxx
    //printf("%08X=%08X\n", addr, data);  //TODO: flip endian?
    break;

  // Real3D DMA
  case 0xC2:  // C2000000-C2000100
    GPU.WriteDMARegister32(addr&0xFF,FLIPENDIAN32(data));
    break;

  // Various
  case 0xF0:
  case 0xFE:  // mirror
    
    switch ((addr>>16)&0xFF)
    {
    // Inputs   
    case 0x04:
      WriteInputs((addr&0x3F)+0,(data>>24)&0xFF);
      WriteInputs((addr&0x3F)+1,(data>>16)&0xFF);
      WriteInputs((addr&0x3F)+2,(data>>8)&0xFF);
      WriteInputs((addr&0x3F)+3,(data>>0)&0xFF);
      break;

    // Sound Board
    case 0x08:
      //printf("PPC: %08X=%08X\n", addr, data);
      break;
      
    // Backup RAM
    case 0x0C:
    case 0x0D:
      *(UINT32 *) &backupRAM[(addr&0x1FFFF)] = data;
      break;

    // MPC105
    case 0x80:  // F0800CF8 (never observed at 0xFExxxxxx)
      PCIBridge.WritePCIConfigAddress(data);
      break;

    // MPC105/106
    case 0xC0: case 0xD0: case 0xE0: 
    case 0xC1: case 0xD1: case 0xE1: 
    case 0xC2: case 0xD2: case 0xE2: 
    case 0xC3: case 0xD3: case 0xE3: 
    case 0xC4: case 0xD4: case 0xE4: 
    case 0xC5: case 0xD5: case 0xE5: 
    case 0xC6: case 0xD6: case 0xE6: 
    case 0xC7: case 0xD7: case 0xE7: 
    case 0xC8: case 0xD8: case 0xE8: 
    case 0xC9: case 0xD9: case 0xE9: 
    case 0xCA: case 0xDA: case 0xEA: 
    case 0xCB: case 0xDB: case 0xEB: 
    case 0xCC: case 0xDC: case 0xEC: 
    case 0xCD: case 0xDD: case 0xED: 
    case 0xCE: case 0xDE: case 0xEE: 
    case 0xCF: case 0xDF: case 0xEF: 
      if ((addr>=0xF0C00CF8) && (addr<0xF0C00D00))    // MPC105
        PCIBridge.WritePCIConfigData(32,0,data);
      else if ((addr>=0xFEC00000) && (addr<0xFEE00000)) // MPC106
        PCIBridge.WritePCIConfigAddress(data);
      else if ((addr>=0xFEE00000) && (addr<0xFEF00000)) // MPC106
        PCIBridge.WritePCIConfigData(32,0,data);
      break;

    // System registers
    case 0x10:
      WriteSystemRegister((addr&0x3F)+0,(data>>24)&0xFF);
      WriteSystemRegister((addr&0x3F)+1,(data>>16)&0xFF);
      WriteSystemRegister((addr&0x3F)+2,(data>>8)&0xFF);
      WriteSystemRegister((addr&0x3F)+3,(data>>0)&0xFF);
      break;
    
    // RTC
    case 0x14:
      RTC.WriteRegister((addr>>2)&0xF,data);
      break;

    // Security board RAM
    case 0x18:
    case 0x19:
      *(UINT32 *) &securityRAM[(addr&0x1FFFF)] = data;
      break;

    // Security board registers
    case 0x1A:
      WriteSecurity(addr&0x3F,data);
      break;
    
    // Unknown
    default:
      break;
    }

    DebugLog("PC=%08X\twrite32: %08X=%08X\n", ppc_get_pc(), addr, data);    
    break;

  // Tile generator
  case 0xF1:
    if (addr < 0xF1120000)
    {
      // Tile generator accesses its RAM as little endian, must flip for big endian PowerPC
      data = FLIPENDIAN32(data);
      TileGen.WriteRAM32(addr&0x1FFFFF,data);
      break;
    }
    else if ((addr>=0xF1180000) && (addr<0xF1180100))
    {
      TileGen.WriteRegister(addr&0xFF,FLIPENDIAN32(data));
      break;
    }

    goto Unknown32;

  // MPC105/106
  case 0xF8:  // F8FFF000-F8FFF100
    // Write in big endian order, like a real PowerPC
    PCIBridge.WriteRegister((addr&0xFF)+0,(data>>24)&0xFF);
    PCIBridge.WriteRegister((addr&0xFF)+1,(data>>16)&0xFF);
    PCIBridge.WriteRegister((addr&0xFF)+2,(data>>8)&0xFF);
    PCIBridge.WriteRegister((addr&0xFF)+3,data&0xFF);
    break;

  // 53C810 SCSI
  case 0xC0:  // step 1.0 only
    if (m_game.stepping != "1.0")
      goto Unknown32;
  case 0xF9:
  case 0xC1:
    SCSI.WriteRegister((addr&0xFF)+0,(data>>24)&0xFF);
    SCSI.WriteRegister((addr&0xFF)+1,(data>>16)&0xFF);
    SCSI.WriteRegister((addr&0xFF)+2,(data>>8)&0xFF);
    SCSI.WriteRegister((addr&0xFF)+3,data&0xFF);
    break;

  // Unknown
  default:
  Unknown32:
    //printf("PC=%08X\twrite32: %08X=%08X\n", ppc_get_pc(), addr, data);
    DebugLog("PC=%08X\twrite32: %08X=%08X\n", ppc_get_pc(), addr, data);
    break;
  }
}

void CModel3::Write64(UINT32 addr, UINT64 data)
{
    Write32(addr+0, (UINT32) (data>>32));
    Write32(addr+4, (UINT32) data);
}

 
/******************************************************************************
 Emulation and Interface Functions
******************************************************************************/

void CModel3::SaveState(CBlockFile *SaveState)
{
  // Write Model 3 state
  SaveState->NewBlock("Model 3", __FILE__);
  SaveState->Write(&inputBank, sizeof(inputBank));
  SaveState->Write(&serialFIFO1, sizeof(serialFIFO1));
  SaveState->Write(&serialFIFO2, sizeof(serialFIFO2));
  SaveState->Write(&gunReg, sizeof(gunReg));
  SaveState->Write(&adcChannel, sizeof(adcChannel));
  SaveState->Write(&cromBankReg, sizeof(cromBankReg));
  SaveState->Write(&securityPtr, sizeof(securityPtr));
  SaveState->Write(ram, 0x800000);
  SaveState->Write(backupRAM, 0x20000);
  SaveState->Write(securityRAM, 0x20000);
  SaveState->Write(&midiCtrlPort, sizeof(midiCtrlPort));
  int32_t securityFirstRead = m_securityFirstRead;
  SaveState->Write(&securityFirstRead, sizeof(securityFirstRead));
  
  // All devices...
  ppc_save_state(SaveState);
  IRQ.SaveState(SaveState);
  PCIBridge.SaveState(SaveState);
  SCSI.SaveState(SaveState);
  EEPROM.SaveState(SaveState);
  TileGen.SaveState(SaveState);
  GPU.SaveState(SaveState);
  SoundBoard.SaveState(SaveState);  // also saves DSB state
  DriveBoard.SaveState(SaveState);
  m_cryptoDevice.SaveState(SaveState);
}

void CModel3::LoadState(CBlockFile *SaveState)
{
  // Load Model 3 state
  if (OKAY != SaveState->FindBlock("Model 3"))
  {
    ErrorLog("Unable to load Model 3 core state. Save state file is corrupt.");
    return;
  }
  
  SaveState->Read(&inputBank, sizeof(inputBank));
  SaveState->Read(&serialFIFO1, sizeof(serialFIFO1));
  SaveState->Read(&serialFIFO2, sizeof(serialFIFO2));
  SaveState->Read(&gunReg, sizeof(gunReg));
  SaveState->Read(&adcChannel, sizeof(adcChannel));
  SaveState->Read(&cromBankReg, sizeof(cromBankReg));
  SetCROMBank(cromBankReg); // update CROM bank
  SaveState->Read(&securityPtr, sizeof(securityPtr));
  SaveState->Read(ram, 0x800000);
  SaveState->Read(backupRAM, 0x20000);
  SaveState->Read(securityRAM, 0x20000);
  SaveState->Read(&midiCtrlPort, sizeof(midiCtrlPort));
  int32_t securityFirstRead;
  SaveState->Write(&securityFirstRead, sizeof(securityFirstRead));
  m_securityFirstRead = securityFirstRead != 0;
  
  // All devices...
  GPU.LoadState(SaveState);
  TileGen.LoadState(SaveState);
  EEPROM.LoadState(SaveState);
  SCSI.LoadState(SaveState);
  PCIBridge.LoadState(SaveState);
  IRQ.LoadState(SaveState);
  ppc_load_state(SaveState);
  SoundBoard.LoadState(SaveState);
  DriveBoard.LoadState(SaveState);
  m_cryptoDevice.LoadState(SaveState);
}

void CModel3::SaveNVRAM(CBlockFile *NVRAM)
{
  // Load EEPROM
  EEPROM.SaveState(NVRAM);

  // Save backup RAM
  NVRAM->NewBlock("Backup RAM", __FILE__);
  NVRAM->Write(backupRAM, 0x20000);
}

void CModel3::LoadNVRAM(CBlockFile *NVRAM)
{
  // Load EEPROM
  EEPROM.LoadState(NVRAM);
  
  // Load backup RAM
  if (OKAY != NVRAM->FindBlock("Backup RAM"))
  {
    ErrorLog("Unable to load Model 3 backup RAM. NVRAM file is corrupt.");
    return;
  }
  NVRAM->Read(backupRAM, 0x20000);
}

void CModel3::ClearNVRAM(void)
{
  memset(backupRAM, 0, 0x20000);
  EEPROM.Clear();
}

void CModel3::RunFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // See if currently running multi-threaded
  if (m_multiThreaded)
  {
    // If so, check all threads are up and running
    if (!StartThreads())
      goto ThreadError;

    // Wake threads for PPC main board (if multi-threading GPU), sound board (if sync'd) and drive board (if attached) so they can process a frame
    if ((m_gpuMultiThreaded       && !ppcBrdThreadSync->Post()) || 
        (syncSndBrdThread         && !sndBrdThreadSync->Post()) || 
        (DriveBoard.IsAttached()  && !drvBrdThreadSync->Post()))
      goto ThreadError;

    // If not multi-threading GPU, then run PPC main board for a frame and sync GPUs now in this thread
    if (!m_gpuMultiThreaded)
    {
      RunMainBoardFrame();
      SyncGPUs();
    }

    // Render frame
    RenderFrame();

    // Enter notify wait critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Wait for PPC main board, sound board and drive board threads to finish their work (if they are running and haven't finished already)
    while ((m_gpuMultiThreaded      && !ppcBrdThreadDone) || 
           (syncSndBrdThread        && !sndBrdThreadDone) || 
           (DriveBoard.IsAttached() && !drvBrdThreadDone))
    {
      if (!notifySync->Wait(notifyLock))
        goto ThreadError;
    }
    ppcBrdThreadDone = false;
    sndBrdThreadDone = false;
    drvBrdThreadDone = false;
    
    // Leave notify wait critical section
    if (!notifyLock->Unlock())
      goto ThreadError;

    // If multi-threading GPU, then sync GPUs last while PPC main board thread is waiting
    if (m_gpuMultiThreaded)
      SyncGPUs();
  }
  else
  {
    // If not multi-threaded, then just process and render a single frame for PPC main board, sound board and drive board in turn in this thread
    RunMainBoardFrame();
    SyncGPUs();
    RenderFrame();
    RunSoundBoardFrame();
    if (DriveBoard.IsAttached())
      RunDriveBoardFrame();
  }

  timings.frameTicks = CThread::GetTicks() - start;

  return;

ThreadError:
  ErrorLog("Threading error in CModel3::RunFrame: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
}

void CModel3::RunMainBoardFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // Compute display and VBlank timings
  unsigned ppcCycles   = m_config["PowerPCFrequency"].ValueAs<unsigned>() * 1000000;
  unsigned frameCycles = ppcCycles / 60;
  unsigned vblCycles   = (unsigned)((float) frameCycles * 2.5f/100.0f); // 2.5% vblank (ridiculously short and wrong but bigger values cause flicker in Daytona)
  unsigned dispCycles  = frameCycles - vblCycles;
  
  // Scale PPC timer ratio according to speed at which the PowerPC is being emulated so that the observed running frequency of the PPC timer
  // registers is more or less correct.  This is needed to get the Virtua Striker 2 series of games running at the right speed (they are 
  // too slow otherwise).  Other games appear to not be affected by this ratio so much as their running speed depends more on the timing of
  // the Real3D status bit below.
  ppc_set_timer_ratio(ppc_get_bus_freq_multipler() * 2 * ppcCycles / ppc_get_cycles_per_sec());
  
  // Compute timing of the Real3D status bit.  This value directly affects the speed at which all the games except Virtua Stiker 2 run.
  // Currently it is not known exactly what this bit represents nor why such wildly varying values are needed for the different step models.
  // The values below were arrived at by trial and error and clearly more investigation is required.  If it turns out that the status bit is
  // connected to the end of VBlank then the code below should be removed and the timing handled via GPU.VBlankEnd() instead.  
  unsigned statusCycles;
  if (m_game.stepping == "2.0" || m_game.stepping == "2.1")
  {
    // For some reason, Fighting Vipers 2 and Daytona USA 2 require completely different timing to the rest of the step 2.x games
    if (m_game.name == "daytona2" || (m_game.stepping == "2.0" && m_game.name == "fvipers2"))
      statusCycles = (unsigned)((float)frameCycles * 24.0f/100.0f);
    else
      statusCycles = (unsigned)((float)frameCycles * 9.12f/100.0f);
  }
  else if (m_game.stepping == "1.5")
    statusCycles = (unsigned)((float)frameCycles * 5.5f/100.0f);
  else
    statusCycles = (unsigned)((float)frameCycles * 48.0f/100.0f);

  // VBlank
  if (gpusReady)
  {
    TileGen.BeginVBlank();
    GPU.BeginVBlank(statusCycles);
    IRQ.Assert(0x02);
    ppc_execute(vblCycles);
    //printf("PC=%08X LR=%08X\n", ppc_get_pc(), ppc_get_lr());
  
    /*
     * Sound:
     *
     * Bit 0x20 of the MIDI control port appears to enable periodic interrupts,
     * which are used to send MIDI commands. Often games will write 0x27, send
     * a series of commands, and write 0x06 to stop. Other games, like Star
     * Wars Trilogy and Sega Rally 2, will enable interrupts at the beginning
     * by writing 0x37 and will disable/enable interrupts to control command
     * output.
     */
    //printf("\t-- BEGIN (Ctrl=%02X, IRQEn=%02X, IRQPend=%02X) --\n", midiCtrlPort, IRQ.ReadIRQEnable()&0x40, IRQ.ReadIRQState());
    int irqCount = 0;
    while ((midiCtrlPort&0x20))
    //while (midiCtrlPort == 0x27)  // 27 triggers IRQ sequence, 06 stops it
    {
      // Don't waste time firing MIDI interrupts if game has disabled them
      if ((IRQ.ReadIRQEnable()&0x40) == 0)
        break;
        
      // Process MIDI interrupt
      IRQ.Assert(0x40);
      ppc_execute(200); // give PowerPC time to acknowledge IRQ
      IRQ.Deassert(0x40);
      ppc_execute(200); // acknowledge that IRQ was deasserted (TODO: is this really needed?)
      dispCycles -= 400;

      ++irqCount;
      if (irqCount > 128)
      {
        //printf("\tMIDI FIFO OVERFLOW! (IRQEn=%02X, IRQPend=%02X)\n", IRQ.ReadIRQEnable()&0x40, IRQ.ReadIRQState());
        break;
      }
    }
    //printf("\t-- END --\n");
    //printf("PC=%08X LR=%08X\n", ppc_get_pc(), ppc_get_lr());
    
    // End VBlank
    GPU.EndVBlank();
    TileGen.EndVBlank();
    IRQ.Assert(0x0D);
  }

  // Run the PowerPC for the active display part of the frame
  ppc_execute(dispCycles);
  // MAME believes 0x0C should occur on every scanline 
  //for (int i = 0; i < 384; i++)
  //{
  //  ppc_execute(dispCycles / 384);
  //  IRQ.Assert(0x0C);
  //}
  //printf("PC=%08X LR=%08X\n", ppc_get_pc(), ppc_get_lr());

  timings.ppcTicks = CThread::GetTicks() - start;
}

void CModel3::SyncGPUs(void)
{
  UINT32 start = CThread::GetTicks();

  timings.syncSize = GPU.SyncSnapshots() + TileGen.SyncSnapshots();
  gpusReady = true;

  timings.syncTicks = CThread::GetTicks() - start;
}

void CModel3::RenderFrame(void)
{
  UINT32 start = CThread::GetTicks();

  // Call OSD video callbacks
  if (BeginFrameVideo() && gpusReady)
  {
    // Render frame
    TileGen.BeginFrame();
    GPU.BeginFrame();
    TileGen.PreRenderFrame();
    TileGen.RenderFrameBottom();
    GPU.RenderFrame();
    TileGen.RenderFrameTop();
    GPU.EndFrame();
    TileGen.EndFrame();
  }

  EndFrameVideo();

  timings.renderTicks = CThread::GetTicks() - start;
}

bool CModel3::RunSoundBoardFrame(void)
{
  UINT32 start = CThread::GetTicks();
  bool bufferFull = SoundBoard.RunFrame();
  timings.sndTicks = CThread::GetTicks() - start;
  return bufferFull;
}

void CModel3::RunDriveBoardFrame(void)
{
  UINT32 start = CThread::GetTicks();
  DriveBoard.RunFrame();
  timings.drvTicks = CThread::GetTicks() - start;
}

bool CModel3::StartThreads(void)
{
  if (startedThreads)
    return true;
      
  // Create synchronization objects
  if (m_gpuMultiThreaded)
  {
    ppcBrdThreadSync = CThread::CreateSemaphore(0);
    if (ppcBrdThreadSync == NULL)
      goto ThreadError;
  }
  sndBrdThreadSync = CThread::CreateSemaphore(0);
  if (sndBrdThreadSync == NULL)
    goto ThreadError;
  sndBrdNotifyLock = CThread::CreateMutex();
  if (sndBrdNotifyLock == NULL)
    goto ThreadError;
  sndBrdNotifySync = CThread::CreateCondVar();
  if (sndBrdNotifySync == NULL)
    goto ThreadError;
  if (DriveBoard.IsAttached())
  {
    drvBrdThreadSync = CThread::CreateSemaphore(0);
    if (drvBrdThreadSync == NULL)
      goto ThreadError;
  }
  notifyLock = CThread::CreateMutex();
  if (notifyLock == NULL)
    goto ThreadError;
  notifySync = CThread::CreateCondVar();
  if (notifySync == NULL)
    goto ThreadError;

  // Reset thread flags
  pauseThreads = false;
  stopThreads = false;

  // Create PPC main board thread, if multi-threading GPU
  if (m_gpuMultiThreaded)
  {
    ppcBrdThread = CThread::CreateThread(StartMainBoardThread, this);
    if (ppcBrdThread == NULL)
      goto ThreadError;
  }

  // Create sound board thread (sync'd or unsync'd)
  if (syncSndBrdThread)
    sndBrdThread = CThread::CreateThread(StartSoundBoardThreadSyncd, this);
  else
    sndBrdThread = CThread::CreateThread(StartSoundBoardThread, this);
  if (sndBrdThread == NULL)
    goto ThreadError;

  // Create drive board thread, if drive board is attached
  if (DriveBoard.IsAttached())
  {
    drvBrdThread = CThread::CreateThread(StartDriveBoardThread, this);
    if (drvBrdThread == NULL)
      goto ThreadError;
  }

  // Set audio callback if sound board thread is unsync'd
  if (!syncSndBrdThread)
    SetAudioCallback(AudioCallback, this);
  
  startedThreads = true;
  return true;

ThreadError:
  ErrorLog("Unable to create threads and/or synchronization objects: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  DeleteThreadObjects();
  m_multiThreaded = false;
  return false;
}

bool CModel3::PauseThreads(void)
{
  if (!startedThreads)
    return true;
  
  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning || sndBrdThreadRunning || drvBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CModel3::PauseThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CModel3::ResumeThreads(void)
{ 
  if (!startedThreads)
    return true;

  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let all threads know that they can continue running
  pauseThreads = false;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  return true;

ThreadError:
  ErrorLog("Threading error in CModel3::ResumeThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

bool CModel3::StopThreads(void)
{
  if (!startedThreads)
    return true;

  // If sound board thread is unsync'd then remove audio callback
  if (!syncSndBrdThread)
    SetAudioCallback(NULL, NULL);
  
  // Enter notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // Let threads know that they should pause and wait for all of them to do so
  pauseThreads = true;
  while (ppcBrdThreadRunning || sndBrdThreadRunning || drvBrdThreadRunning)
  {
    if (!notifySync->Wait(notifyLock))
      goto ThreadError;
  }

  // Now let threads know that they should exit
  stopThreads = true;

  // Leave notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  
  // Resume each thread in turn and wait for them to exit
  if (ppcBrdThread != NULL)
  {
    if (ppcBrdThreadSync->Post())
      ppcBrdThread->Wait();
  }
  if (sndBrdThread != NULL)
  {
    if (syncSndBrdThread)
    {
      if (sndBrdThreadSync->Post())
        sndBrdThread->Wait();
    }
    else
    {
      if (WakeSoundBoardThread())
        sndBrdThread->Wait();
    }
  }
  if (drvBrdThread != NULL)
  {
    if (drvBrdThreadSync->Post())
      drvBrdThread->Wait();
  }

  // Delete all thread and synchronization objects
  DeleteThreadObjects();
  startedThreads = false;
  return true;

ThreadError:
  ErrorLog("Threading error in CModel3::StopThreads: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

void CModel3::DeleteThreadObjects(void)
{
  // Delete PPC main board, sound board and drive board threads
  if (ppcBrdThread != NULL)
  {
    delete ppcBrdThread;
    ppcBrdThread = NULL;
  }
  if (sndBrdThread != NULL)
  {
    delete sndBrdThread;
    sndBrdThread = NULL;
  }
  if (drvBrdThread != NULL)
  {
    delete drvBrdThread;
    drvBrdThread = NULL;
  }

  // Delete synchronization objects
  if (ppcBrdThreadSync != NULL)
  {
    delete ppcBrdThreadSync;
    ppcBrdThreadSync = NULL;
  }
  if (sndBrdThreadSync != NULL)
  {
    delete sndBrdThreadSync;
    sndBrdThreadSync = NULL;
  }
  if (drvBrdThreadSync != NULL)
  {
    delete drvBrdThreadSync;
    drvBrdThreadSync = NULL;
  }
  if (sndBrdNotifyLock != NULL)
  {
    delete sndBrdNotifyLock;
    sndBrdNotifyLock = NULL;
  }
  if (sndBrdNotifySync != NULL)
  {
    delete sndBrdNotifySync;
    sndBrdNotifySync = NULL;
  }
  if (notifyLock != NULL)
  {
    delete notifyLock;
    notifyLock = NULL;
  }
  if (notifySync != NULL)
  {
    delete notifySync;
    notifySync = NULL;
  }
}

void CModel3::DumpTimings(void)
{
  printf("PPC:%3ums%c render:%3ums%c sync:%4uK%c%3ums%c snd:%3ums%c drv:%3ums%c frame:%3ums%c\n",
    timings.ppcTicks, (timings.ppcTicks > timings.renderTicks ? '!' : ','),
    timings.renderTicks, (timings.renderTicks > timings.ppcTicks ? '!' : ','), 
    timings.syncSize / 1024, (timings.syncSize / 1024 > 128 ? '!' : ','), 
    timings.syncTicks, (timings.syncTicks > 1 ? '!' : ','),
    timings.sndTicks, (timings.sndTicks > 10 ? '!' : ','),
    timings.drvTicks, (timings.drvTicks > 10 ? '!' : ','),
    timings.frameTicks, (timings.frameTicks > 16 ? '!' : ' '));
}

FrameTimings CModel3::GetTimings(void)
{
  return timings;
}

int CModel3::StartMainBoardThread(void *data)
{
  // Call method on CModel3 to run PPC main board thread
  CModel3 *model3 = (CModel3*)data;
  return model3->RunMainBoardThread();
}

int CModel3::StartSoundBoardThread(void *data)
{
  // Call method on CModel3 to run sound board thread (unsync'd)
  CModel3 *model3 = (CModel3*)data;
  return model3->RunSoundBoardThread();
}

int CModel3::StartSoundBoardThreadSyncd(void *data)
{
  // Call method on CModel3 to run sound board thread (sync'd)
  CModel3 *model3 = (CModel3*)data;
  return model3->RunSoundBoardThreadSyncd();
}

int CModel3::StartDriveBoardThread(void *data)
{
  // Call method on CModel3 to run drive board thread
  CModel3 *model3 = (CModel3*)data;
  return model3->RunDriveBoardThread();
}

int CModel3::RunMainBoardThread(void)
{
  for (;;)
  {
    bool wait = true;
    bool exit = false;
    while (wait && !exit)
    {
      // Wait on PPC main board thread semaphore
      if (!ppcBrdThreadSync->Wait())
        goto ThreadError;

      // Enter notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        ppcBrdThreadRunning = true;
      }
  
      // Leave notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;
    }
    if (exit)
      return 0;

    // Process a single frame for PPC main board
    RunMainBoardFrame();

    // Enter notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    ppcBrdThreadRunning = false;
    ppcBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunMainBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

void CModel3::AudioCallback(void *data)
{
  // Call method on CModel3 to wake sound board thread
  CModel3 *model3 = (CModel3*)data;
  model3->WakeSoundBoardThread();
}

bool CModel3::WakeSoundBoardThread(void)
{
  // Enter sound board notify critical section
  bool wake;
  if (!sndBrdNotifyLock->Lock())
    goto ThreadError;

  // Enter main notify critical section
  if (!notifyLock->Lock())
    goto ThreadError;

  // See if sound board thread is currently running
  wake = !sndBrdThreadRunning;
  
  // Leave main notify critical section
  if (!notifyLock->Unlock())
    goto ThreadError;
  
  // Only send wake notification to sound board thread if it was not running
  if (wake)
  {
    // Signal to sound board thread that it should start processing again
    sndBrdWakeNotify = true;
    if (!sndBrdNotifySync->Signal())
      goto ThreadError;
  }

  // Leave sound board notify critical section
  if (!sndBrdNotifyLock->Unlock())
    goto ThreadError;
  return wake;

ThreadError:
  ErrorLog("Threading error in WakeSoundBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return false;
}

int CModel3::RunSoundBoardThread(void)
{
  for (;;)
  {
    bool wait = true;
    bool exit = false;
    while (wait && !exit)
    {
      // Enter sound board notify critical section
      if (!sndBrdNotifyLock->Lock())
        goto ThreadError;

      // Wait for notification from audio callback
      while (!sndBrdWakeNotify)
      {
        if (!sndBrdNotifySync->Wait(sndBrdNotifyLock))
          goto ThreadError;
      }
      sndBrdWakeNotify = false;
  
      // Enter main notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        sndBrdThreadRunning = true;
      }

      // Leave main notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;

      // Leave sound board notify critical section
      if (!sndBrdNotifyLock->Unlock())
        goto ThreadError;     
    }
    if (exit)
      return 0;

    // Keep processing frames until pausing or audio buffer is full
    while (true)
    {
      // Enter main notify critical section
      bool paused;
      if (!notifyLock->Lock())
        goto ThreadError;

      paused = pauseThreads;
        
      // Leave main notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;

      if (paused || RunSoundBoardFrame())
        break;
      //printf("Rerunning sound board\n");
    }

    // Enter main notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    sndBrdThreadRunning = false;
    sndBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave main notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunSoundBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

int CModel3::RunSoundBoardThreadSyncd(void)
{
  for (;;)
  {
    bool wait = true;
    bool exit = false;
    while (wait && !exit)
    {
      // Wait on sound board thread semaphore
      if (!sndBrdThreadSync->Wait())
        goto ThreadError;
  
      // Enter notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        sndBrdThreadRunning = true;
      }

      // Leave notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;
    }
    if (exit)
      return 0;

    // Process a single frame for sound board
    RunSoundBoardFrame();

    // Enter notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    sndBrdThreadRunning = false;
    sndBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunSoundBoardThreadSyncd: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

int CModel3::RunDriveBoardThread(void)
{
  for (;;)
  {
    bool wait = true; 
    bool exit = false;
    while (wait && !exit)
    {
      // Wait on drive board thread semaphore
      if (!drvBrdThreadSync->Wait())
        goto ThreadError;

      // Enter notify critical section
      if (!notifyLock->Lock())
        goto ThreadError;

      // Check threads are not being stopped or paused
      if (stopThreads)
        exit = true;
      else if (!pauseThreads)
      {
        wait = false;
        drvBrdThreadRunning = true;
      }
  
      // Leave notify critical section
      if (!notifyLock->Unlock())
        goto ThreadError;
    }
    if (exit)
      return 0;

    // Process a single frame for drive board
    RunDriveBoardFrame();

    // Enter notify critical section
    if (!notifyLock->Lock())
      goto ThreadError;

    // Let other threads know processing has finished
    drvBrdThreadRunning = false;
    drvBrdThreadDone = true;
    if (!notifySync->SignalAll())
      goto ThreadError;

    // Leave notify critical section
    if (!notifyLock->Unlock())
      goto ThreadError;
  }

ThreadError:
  ErrorLog("Threading error in RunDriveBoardThread: %s\nSwitching back to single-threaded mode.\n", CThread::GetLastError());
  m_multiThreaded = false;
  return 1;
}

void CModel3::Reset(void)
{
  // Clear memory (but do not modify backup RAM!)
  memset(ram, 0, 0x800000);
  
  // Initial bank is bank 0
  SetCROMBank(0xFF);
  
  // Reset security device
  securityPtr = 0;
  m_securityFirstRead = true;
  
  // Reset inputs
  inputBank = 0;
  serialFIFO1 = 0;
  serialFIFO2 = 0;
  adcChannel = 0;
  
  // MIDI
  midiCtrlPort = 0;
  
  // Reset all devices
  ppc_reset();
  IRQ.Reset();
  PCIBridge.Reset();
  PCIBus.Reset();
  SCSI.Reset();
  RTC.Reset();
  EEPROM.Reset();
  TileGen.Reset();
  GPU.Reset();
  SoundBoard.Reset();

  if (DriveBoard.IsAttached())
    DriveBoard.Reset();
    
  m_cryptoDevice.Reset();

  gpusReady = false;

  timings.ppcTicks = 0;
  timings.syncSize = 0;
  timings.syncTicks = 0;
  timings.renderTicks = 0;
  timings.sndTicks = 0;
  timings.drvTicks = 0;
  timings.frameTicks = 0;
  
  DebugLog("Model 3 reset\n");
}


/******************************************************************************
 Initialization, Shutdown, and ROM Management
******************************************************************************/

// Apply patches to games
static void Patch(uint8_t *crom, const Game &game)
{
  if (game.name == "scudplus" || game.name == "scudplusa") // No patching?
  {
    // Base offset of program in CROM: 0x710000
  }
  else if (game.name == "lemans24")
  {
    // Base offset of program in CROM: 6473C0
    *(UINT32 *) &crom[0x6D8C4C] = 0x00000002; // comm. mode: 00=master, 01=slave, 02=satellite
    *(UINT32 *) &crom[0x73FE38] = 0x38840004; // an actual bug in the game code
    *(UINT32 *) &crom[0x73EB5C] = 0x60000000;
    *(UINT32 *) &crom[0x73EDD0] = 0x60000000;
    *(UINT32 *) &crom[0x73EDC4] = 0x60000000;
  } 
  else if (game.name == "lostwsga")
  {
    *(UINT32 *) &crom[0x7374f4] = 0x38840004; // an actual bug in the game code
  }
  else if (game.name == "vs215" || game.name == "vs215o" || game.name == "vs29815" || game.name == "vs29915")
  {
    // VS215 is a modification of VS2 that runs on Step 1.5 hardware. I
    // suspect the code here is trying to detect the system type but am too
    // lazy to figure it out right now.
    *(UINT32 *) &crom[0x7001A8] = 0x48000000+(0xFFF01630-0xFFF001A8); // force a jump to FFF01630
  }
  else if (game.name == "vs298")
  {
    // Base offset of program in CROM: 600000
    // Inexplicably, at PC=AFC1C, a call is made to FC78, which is right in the middle of some
    // totally unrelated initialization code (ASIC checks). This causes an invalid pointer to be fetched. 
    // Perhaps FC78 should be overwritten with other program data by then? Why is it not?
    // Or, 300138 needs to be written with a non-zero value, it is loaded from EEPROM but is 0.
    *(UINT32 *) &crom[0x6AFC1C] = 0x60000000;
  }
  else if (game.name == "srally2")
  {
    *(UINT32 *) &crom[0x7C0C4] = 0x60000000;
    *(UINT32 *) &crom[0x7C0C8] = 0x60000000;
    *(UINT32 *) &crom[0x7C0CC] = 0x60000000;
  }
  else if (game.name == "harleya")
  {
    *(UINT32 *) &crom[0x50E8D4] = 0x60000000;
    *(UINT32 *) &crom[0x50E8F4] = 0x60000000;
    *(UINT32 *) &crom[0x50FB84] = 0x60000000;
  }
  else if (game.name == "harley")
  {
    *(UINT32 *) &crom[0x50ECB4] = 0x60000000;
    *(UINT32 *) &crom[0x50ECD4] = 0x60000000;
    *(UINT32 *) &crom[0x50FF64] = 0x60000000;
  }
  else if (game.name == "swtrilgy")
  {
    *(UINT32 *) &crom[0xF0E48] = 0x60000000;
    *(UINT32 *) &crom[0x043DC] = 0x48000090;  // related to joystick feedback
    *(UINT32 *) &crom[0x029A0] = 0x60000000;
    *(UINT32 *) &crom[0x02A0C] = 0x60000000;
  }
  else if (game.name == "swtrilgya")
  {
    *(UINT32 *) &crom[0xF6DD0] = 0x60000000;  // from MAME
  }
  else if (game.name == "eca" || game.name == "ecau")
  {
    *(UINT32 *) &crom[0x535580] = 0x60000000;
    //*(UINT32 *) &crom[0x5023B4] = 0x60000000;
    //*(UINT32 *) &crom[0x5023D4] = 0x60000000;
  }
}

// Reverses all aligned 16-bit words, thereby switching their endianness (assumes buffer size is divisible by 2)
static void Reverse16(uint8_t *buf, size_t size)
{
  for (size_t i = 0; i < size; i += 2)
  {
    uint8_t tmp = buf[i+0];
    buf[i+0] = buf[i+1];
    buf[i+1] = tmp;
  }
}

// Reverses all aligned 32-bit words, thereby switching their endianness (assumes buffer size is divisible by 4)
static void Reverse32(uint8_t *buf, size_t size)
{
  for (size_t i = 0; i < size; i += 4)
  {
    uint8_t tmp1 = buf[i+0];
    uint8_t tmp2 = buf[i+1];
    buf[i+0] = buf[i+3];
    buf[i+1] = buf[i+2];
    buf[i+2] = tmp2;
    buf[i+3] = tmp1;
  }
}

// Offsets of memory regions within Model 3's pool
#define OFFSET_RAM          0         // 8 MB
#define OFFSET_CROM         0x800000  // 8 MB (fixed CROM)
#define OFFSET_CROMxx       0x1000000 // 128 MB (banked CROM0-3 must follow fixed CROM)
#define OFFSET_VROM         0x9000000 // 64 MB
#define OFFSET_BACKUPRAM    0xD000000 // 128 KB
#define OFFSET_SECURITYRAM  0xD020000 // 128 KB
#define OFFSET_SOUNDROM     0xD040000 // 512 KB (68K sound board program)
#define OFFSET_SAMPLEROM    0xD0C0000 // 16 MB (sound board samples)
#define OFFSET_DSBPROGROM   0xE0C0000 // 128 KB (DSB program)
#define OFFSET_DSBMPEGROM   0xE0E0000 // 16 MB (DSB MPEG data -- Z80 version only uses 8MB)
#define OFFSET_DRIVEROM     0xF0E0000 // 64 KB
#define MEMORY_POOL_SIZE    (0x800000 + 0x800000 + 0x8000000 + 0x4000000 + 0x20000 + 0x20000 + 0x80000 + 0x1000000 + 0x20000 + 0x1000000 + 0x10000)

// 64-bit magic number used to detect loading of optional ROMs
#define MAGIC_NUMBER  0x4C444D5245505553ULL

const Game &CModel3::GetGame() const
{
  return m_game;
}
  
// Stepping-dependent parameters (MPC10x type, etc.) are initialized here
bool CModel3::LoadGame(const Game &game, const ROMSet &rom_set)
{
  m_game = Game();
 
  /*
   * Copy in ROM data with mirroring as necessary for the following cases:
   *
   *  - VROM: 64MB. If <= 32MB, mirror to high 32MB.
   *  - Banked CROM: 128MB. If <= 64MB, mirror to high 64MB.
   *  - Fixed CROM: 8MB. If < 8MB, loaded only in high part of space and low
   *    part is a mirror of (banked) CROM0.
   *  - Sample ROM: 16MB. If <= 8MB, mirror to high 8MB.
   *
   * ROMs are released after being loaded.
   */
  if (rom_set.get_rom("vrom").size <= 32*0x100000)
  {
    rom_set.get_rom("vrom").CopyTo(&vrom[0], 32*100000);
    rom_set.get_rom("vrom").CopyTo(&vrom[32*0x100000], 32*0x100000);
  }
  else
    rom_set.get_rom("vrom").CopyTo(vrom, 64*0x100000);
  if (rom_set.get_rom("banked_crom").size <= 64*0x100000)
  {
    rom_set.get_rom("banked_crom").CopyTo(&crom[8*0x100000 + 0], 64*0x100000);
    rom_set.get_rom("banked_crom").CopyTo(&crom[8*0x100000 + 64*0x100000], 64*0x100000);
  }
  else
    rom_set.get_rom("banked_crom").CopyTo(&crom[8*0x100000 + 0], 128*0x100000);
  size_t crom_size = rom_set.get_rom("crom").size;
  rom_set.get_rom("crom").CopyTo(&crom[8*0x100000 - crom_size], crom_size);
  if (crom_size < 8*0x100000)
    rom_set.get_rom("banked_crom").CopyTo(&crom[0], 8*0x100000 - crom_size);
  if (rom_set.get_rom("sound_samples").size <= 8*0x100000)
  {
    rom_set.get_rom("sound_samples").CopyTo(&sampleROM[0], 8*0x100000);
    rom_set.get_rom("sound_samples").CopyTo(&sampleROM[8*0x100000], 8*0x100000);
  }
  else
    rom_set.get_rom("sound_samples").CopyTo(sampleROM, 16*0x100000);
  rom_set.get_rom("sound_program").CopyTo(soundROM, 512*1024);
  rom_set.get_rom("mpeg_program").CopyTo(dsbROM, 128*1024);
  rom_set.get_rom("mpeg_music").CopyTo(mpegROM, 16*0x100000);
  rom_set.get_rom("driveboard_program").CopyTo(driveROM, 64*1024);
    
  // Convert PowerPC and 68K ROMs to little endian words 
  Util::FlipEndian32(crom, 8*0x100000 + 128*0x100000);
  Util::FlipEndian16(soundROM, 512*1024);
  Util::FlipEndian16(sampleROM, 16*0x100000);

  // Initialize CPU
  PPC_CONFIG  ppc_config;
  if (game.stepping == "2.0" || game.stepping == "2.1")
  {
    ppc_config.pvr = PPC_MODEL_603R;   // 166 MHz
    ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
    ppc_config.bus_frequency_multiplier = 0x25; // 2.5X multiplier
    PCIBridge.SetModel(0x106);        // MPC106
  } 
  else if (game.stepping == "1.5")
  {
    ppc_config.pvr = PPC_MODEL_603E;   // 100 MHz
    ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
    ppc_config.bus_frequency_multiplier = 0x15; // 1.5X multiplier
    if (game.name == "scudp1")
      PCIBridge.SetModel(0x106);      // some Step 1.x games use MPC106
    else
      PCIBridge.SetModel(0x105);      // MPC105
  }
  else if (game.stepping == "1.0")
  {
    ppc_config.pvr = PPC_MODEL_603R;   // 66 MHz
    ppc_config.bus_frequency = BUS_FREQUENCY_66MHZ;
    ppc_config.bus_frequency_multiplier = 0x10; // 1X multiplier
    if (game.name == "bass" || game.name == "getbass")
      PCIBridge.SetModel(0x106);      // some Step 1.x games use MPC106
    else
      PCIBridge.SetModel(0x105);      // MPC105
  }
  else
  {
    ErrorLog("Cannot configure Model 3 because game uses unrecognized stepping (%s).", game.stepping.c_str());
    return FAIL;
  }
  ppc_init(&ppc_config);
  ppc_attach_bus(this);
  PPCFetchRegions[0].start = 0; 
  PPCFetchRegions[0].end = 0x007FFFFF;
  PPCFetchRegions[0].ptr = (UINT32 *) ram;
  PPCFetchRegions[1].start = 0xFF800000;  
  PPCFetchRegions[1].end = 0xFFFFFFFF;
  PPCFetchRegions[1].ptr = (UINT32 *) crom;
  PPCFetchRegions[2].start = 0;
  PPCFetchRegions[2].end = 0;
  PPCFetchRegions[2].ptr = NULL;
  ppc_set_fetch(PPCFetchRegions);
  
  // Initialize Real3D 
  int stepping = ((game.stepping[0] - '0') << 4) | (game.stepping[2] - '0');
  GPU.SetStepping(stepping);
  
  // MPEG board (if present)
  if (rom_set.get_rom("mpeg_program").size)
  {
    if (game.mpeg_board == "DSB1")
    {
      DSB = new(std::nothrow) CDSB1(m_config);
      if (NULL == DSB)
        return ErrorLog("Insufficient memory for Digital Sound Board object."); 
    }
    else if (game.mpeg_board == "DSB2")
    {
      Util::FlipEndian16(dsbROM, 128*1024); // 68K program needs to be byte swapped
      DSB = new(std::nothrow) CDSB2(m_config);
      if (NULL == DSB)
        return ErrorLog("Insufficient memory for Digital Sound Board object."); 
    }
    else if (game.mpeg_board.empty())
      ErrorLog("No MPEG board type defined in game XML for MPEG ROMs.");
    else
      ErrorLog("Unknown MPEG board type '%s'. Only 'DSB1' and 'DSB2' are supported.", game.mpeg_board.c_str());
    if (DSB && OKAY != DSB->Init(dsbROM, mpegROM))
      return FAIL;
  }
  SoundBoard.AttachDSB(DSB);
  
  // Drive board (if present)
  if (rom_set.get_rom("driveboard_program").size)
  {
    if (DriveBoard.Init(driveROM))
      return FAIL;
  }
  else
    DriveBoard.Init(NULL);

  // Security board encryption device
  m_cryptoDevice.Init(game.encryption_key, std::bind(&CModel3::ReadSecurityRAM, this, std::placeholders::_1));
  
  // Apply ROM patches
  //TODO: place these in XML
  Patch(crom, game);
  
  // Print game information
  std::set<std::string> extra_hw;
  if (DSB)
    extra_hw.insert(Util::Format() << "Digital Sound Board (Type " << game.mpeg_board << ")");
  if (rom_set.get_rom("driveboard_program").size)
    extra_hw.insert("Drive Board");
  if (game.encryption_key)
    extra_hw.insert("Security Board");
  if (!game.version.empty())
    std::cout << "    Title:          " << game.title << " (" << game.version << ")" << std::endl;
  else
    std::cout << "    Title:          " << game.title << std::endl;
  std::cout << "    ROM Set:        " << game.name << std::endl;
  std::cout << "    Developer:      " << game.manufacturer << std::endl;
  std::cout << "    Year:           " << game.year << std::endl;
  std::cout << "    Stepping:       " << game.stepping << std::endl;
  if (!extra_hw.empty())
    std::cout << "    Extra Hardware: " << Util::Format(", ").Join(extra_hw) << std::endl;
  std::cout << std::endl;
  
  m_game = game;
  return OKAY;
}

void CModel3::AttachRenderers(CRender2D *Render2DPtr, IRender3D *Render3DPtr)
{
  TileGen.AttachRenderer(Render2DPtr);
  GPU.AttachRenderer(Render3DPtr);
}

void CModel3::AttachInputs(CInputs *InputsPtr)
{
  Inputs = InputsPtr;

  if (DriveBoard.IsAttached())
    DriveBoard.AttachInputs(Inputs, m_game.inputs);

  DebugLog("Model 3 attached inputs\n");
}

void CModel3::AttachOutputs(COutputs *OutputsPtr)
{
  Outputs = OutputsPtr;
  Outputs->SetGame(m_game);
  Outputs->Attached();

  if (DriveBoard.IsAttached())
    DriveBoard.AttachOutputs(Outputs);

  DebugLog("Model 3 attached outputs\n");
}

// Model 3 initialization. Some initialization is deferred until ROMs are loaded in LoadROMSet()
bool CModel3::Init(void)
{
  float memSizeMB = (float)MEMORY_POOL_SIZE/(float)0x100000;
  
  // Allocate all memory for ROMs and PPC RAM
  memoryPool = new(std::nothrow) UINT8[MEMORY_POOL_SIZE];
  if (NULL == memoryPool)
    return ErrorLog("Insufficient memory for Model 3 object (needs %1.1f MB).", memSizeMB);
  memset(memoryPool, 0, MEMORY_POOL_SIZE);
    
  // Set up pointers
  ram = &memoryPool[OFFSET_RAM];
  crom = &memoryPool[OFFSET_CROM];
  vrom = &memoryPool[OFFSET_VROM];
  soundROM = &memoryPool[OFFSET_SOUNDROM];
  sampleROM = &memoryPool[OFFSET_SAMPLEROM];
  dsbROM = &memoryPool[OFFSET_DSBPROGROM];
  mpegROM = &memoryPool[OFFSET_DSBMPEGROM];
  backupRAM = &memoryPool[OFFSET_BACKUPRAM];
  securityRAM = &memoryPool[OFFSET_SECURITYRAM];
  driveROM = &memoryPool[OFFSET_DRIVEROM];
  SetCROMBank(0xFF);
  
  // Initialize other devices (PowerPC, DSB, and security board initialized after ROMs loaded)
  IRQ.Init();
  PCIBridge.Init();
  PCIBus.Init();
  SCSI.Init(this,&IRQ,0x100); // SCSI is actually a non-maskable interrupt, so we give it a bit number outside of 8-bit range
  RTC.Init();
  EEPROM.Init();
  if (OKAY != TileGen.Init(&IRQ))
    return FAIL;
  if (OKAY != GPU.Init(vrom,this,&IRQ,0x100)) // same for Real3D DMA interrupt
    return FAIL;
  if (OKAY != SoundBoard.Init(soundROM,sampleROM))
    return FAIL;
    
  PCIBridge.AttachPCIBus(&PCIBus);
  PCIBus.AttachDevice(13,&GPU);
  PCIBus.AttachDevice(14,&SCSI);
  PCIBus.AttachDevice(16,this);

  DebugLog("Initialized Model 3 (allocated %1.1f MB)\n", memSizeMB);
  
  return OKAY;
}

CSoundBoard *CModel3::GetSoundBoard(void)
{
  return &SoundBoard;
}
 
CDriveBoard *CModel3::GetDriveBoard(void)
{
  return &DriveBoard;
}

CModel3::CModel3(const Util::Config::Node &config)
  : m_config(config),
    m_multiThreaded(config["MultiThreaded"].ValueAs<bool>()),
    m_gpuMultiThreaded(config["GPUMultiThreaded"].ValueAs<bool>()),
    TileGen(config),
    GPU(config),
    SoundBoard(config),
    DriveBoard(config)
{
  // Initialize pointers so dtor can know whether to free them
  memoryPool = NULL;
  
  // Various uninitialized pointers
  Inputs = NULL;
  Outputs = NULL;
  ram = NULL;
  crom = NULL;
  vrom = NULL;
  soundROM = NULL;
  sampleROM = NULL;
  dsbROM = NULL;
  mpegROM = NULL;
  cromBank = NULL;
  backupRAM = NULL;
  securityRAM = NULL;
  
  DSB = NULL;
  
  securityPtr = 0;
  
  startedThreads = false;
  pauseThreads = false;
  stopThreads = false;
  ppcBrdThread = NULL;
  sndBrdThread = NULL; 
  drvBrdThread = NULL;
  ppcBrdThreadRunning = false;
  ppcBrdThreadDone = false;
  sndBrdThreadRunning = false;
  sndBrdThreadDone = false;
  drvBrdThreadRunning = false;
  drvBrdThreadDone = false;
  syncSndBrdThread = false;
  ppcBrdThreadSync = NULL;
  sndBrdThreadSync = NULL;
  drvBrdThreadSync = NULL;
  notifyLock = NULL;
  notifySync = NULL;
  
  DebugLog("Built Model 3\n");
}

// Dumps a memory region to a file for debugging purposes
/*
static void Dump(const char *file, uint8_t *buf, size_t size, bool reverse32, bool reverse16)
{
  FILE *fp = fopen(file, "wb");
  if (NULL != fp)
  {
    if (reverse32)
      Reverse32(buf, size);
    else if (reverse16)
      Reverse16(buf, size);
    fwrite(buf, sizeof(UINT8), size, fp);
    fclose(fp);
    printf("dumped %s\n", file);
  }
  else
    printf("unable to dump %s\n", file);
}
*/

CModel3::~CModel3(void)
{
  /*
  // Debug: dump some files
  //Dump("ram", ram, 0x800000, true, false);
  //Dump("vrom", vrom, 0x4000000, true, false);
  Dump("crom", crom, 0x800000, true, false);
  //Dump("bankedCrom", &crom[0x800000], 0x7000000, true, false);
  //Dump("soundROM", soundROM, 0x80000, false, true);
  //Dump("sampleROM", sampleROM, 0x800000, false, true);
  */
  
  // Stop all threads
  StopThreads();
  
  // Free memory
  if (memoryPool != NULL)
  {
    delete [] memoryPool;
    memoryPool = NULL;
  }
  
  if (DSB != NULL)
  {
    delete DSB;
    DSB = NULL;
  }
  
  Inputs = NULL;
  Outputs = NULL;
  ram = NULL;
  crom = NULL;
  vrom = NULL;
  soundROM = NULL;
  sampleROM = NULL;
  dsbROM = NULL;
  mpegROM = NULL;
  cromBank = NULL;
  backupRAM = NULL;
  securityRAM = NULL;
  
  DebugLog("Destroyed Model 3\n");
}