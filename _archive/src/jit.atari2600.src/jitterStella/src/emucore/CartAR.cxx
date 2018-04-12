//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2002 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: CartAR.cxx,v 1.5 2002/05/14 18:29:44 stephena Exp $
//============================================================================

#include <assert.h>
#include <string.h>
#include "CartAR.hxx"
#include "M6502Hi.hxx"
#include "Random.hxx"
#include "System.hxx"
#include "jit.common.h"
#include <iostream>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeAR::CartridgeAR(const uInt8* image, uInt32 size)
    : my6502(0)
{
  uInt32 i;

  cartSize = size;
  ramSize = 6 * 1024;
  post("Creating CartAR");

  // Create a load image buffer and copy the given image
  myLoadImages = new uInt8[size];
  myNumberOfLoadImages = size / 8448;
  memcpy(myLoadImages, image, size);

  // Initialize RAM with random values
  Random random;
  for(i = 0; i < 6 * 1024; ++i)
  {
    myImage[i] = random.next();
  }

  // Initialize SC BIOS ROM
  initializeROM();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeAR::~CartridgeAR()
{
  delete[] myLoadImages;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* CartridgeAR::name() const
{
  return "CartridgeAR";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::reset()
{
  myPower = true;
  myPowerRomCycle = mySystem->cycles();
  myWriteEnabled = false;

  myDataHoldRegister = 0;
  myNumberOfDistinctAccesses = 0;
  myWritePending = false;

  // Set bank configuration upon reset so ROM is selected and powered up
  bankConfiguration(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::systemCyclesReset()
{
  // Get the current system cycle
  uInt32 cycles = mySystem->cycles();

  // Adjust cycle values
  myPowerRomCycle -= cycles;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  my6502 = &(M6502High&)mySystem->m6502();

  // Make sure the system we're being installed in has a page size that'll work
  assert((0x1000 & mask) == 0);

  System::PageAccess access;
  for(uInt32 i = 0x1000; i < 0x2000; i += (1 << shift))
  {
    access.directPeekBase = 0;
    access.directPokeBase = 0;
    access.device = this;
    mySystem->setPageAccess(i >> shift, access);
  }

  bankConfiguration(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeAR::peek(uInt16 addr)
{
  // Is the "dummy" SC BIOS hotspot for reading a load being accessed?
  if(((addr & 0x1FFF) == 0x1850) && (myImageOffset[1] == (3 * 2048)))
  {
    // Get load that's being accessed (BIOS places load number at 0x80)
    uInt8 load = mySystem->peek(0x0080);

    // Read the specified load into RAM
    loadIntoRAM(load);

    return myImage[(addr & 0x07FF) + myImageOffset[1]];
  }

  // Cancel any pending write if more than 5 distinct accesses have occurred
  // TODO: Modify to handle when the distinct counter wraps around...
  if(myWritePending && 
      (my6502->distinctAccesses() > myNumberOfDistinctAccesses + 5))
  {
    myWritePending = false;
  }

  // Is the data hold register being set?
  if(!(addr & 0x0F00) && (!myWriteEnabled || !myWritePending))
  {
    myDataHoldRegister = addr;
    myNumberOfDistinctAccesses = my6502->distinctAccesses();
    myWritePending = true;
  }
  // Is the bank configuration hotspot being accessed?
  else if((addr & 0x1FFF) == 0x1FF8)
  {
    // Yes, so handle bank configuration
    myWritePending = false;
    bankConfiguration(myDataHoldRegister);
  }
  // Handle poke if writing enabled
  else if(myWriteEnabled && myWritePending && 
      (my6502->distinctAccesses() == (myNumberOfDistinctAccesses + 5)))
  {
    if((addr & 0x0800) == 0)
      myImage[(addr & 0x07FF) + myImageOffset[0]] = myDataHoldRegister;
    else if(myImageOffset[1] != 3 * 2048)    // Can't poke to ROM :-)
      myImage[(addr & 0x07FF) + myImageOffset[1]] = myDataHoldRegister;
    myWritePending = false;
  }

  mySystem->fb.doSomething((addr & 0x07FF) + myImageOffset[(addr & 0x0800) ? 1 : 0]);
  return myImage[(addr & 0x07FF) + myImageOffset[(addr & 0x0800) ? 1 : 0]];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::poke(uInt16 addr, uInt8)
{
  // Cancel any pending write if more than 5 distinct accesses have occurred
  // TODO: Modify to handle when the distinct counter wraps around...
  if(myWritePending && 
      (my6502->distinctAccesses() > myNumberOfDistinctAccesses + 5))
  {
    myWritePending = false;
  }

  // Is the data hold register being set?
  if(!(addr & 0x0F00) && (!myWriteEnabled || !myWritePending))
  {
    myDataHoldRegister = addr;
    myNumberOfDistinctAccesses = my6502->distinctAccesses();
    myWritePending = true;
  }
  // Is the bank configuration hotspot being accessed?
  else if((addr & 0x1FFF) == 0x1FF8)
  {
    // Yes, so handle bank configuration
    myWritePending = false;
    bankConfiguration(myDataHoldRegister);
  }
  // Handle poke if writing enabled
  else if(myWriteEnabled && myWritePending && 
      (my6502->distinctAccesses() == (myNumberOfDistinctAccesses + 5)))
  {
    if((addr & 0x0800) == 0)
      myImage[(addr & 0x07FF) + myImageOffset[0]] = myDataHoldRegister;
    else if(myImageOffset[1] != 3 * 2048)    // Can't poke to ROM :-)
      myImage[(addr & 0x07FF) + myImageOffset[1]] = myDataHoldRegister;
    myWritePending = false;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::bankConfiguration(uInt8 configuration)
{
  // D7-D5 of this byte: Write Pulse Delay (n/a for emulator)
  //
  // D4-D0: RAM/ROM configuration:
  //       $F000-F7FF    $F800-FFFF Address range that banks map into
  //  000wp     2            ROM
  //  001wp     0            ROM
  //  010wp     2            0      as used in Commie Mutants and many others
  //  011wp     0            2      as used in Suicide Mission
  //  100wp     2            ROM
  //  101wp     1            ROM
  //  110wp     2            1      as used in Killer Satellites
  //  111wp     1            2      as we use for 2k/4k ROM cloning
  // 
  //  w = Write Enable (1 = enabled; accesses to $F000-$F0FF cause writes
  //    to happen.  0 = disabled, and the cart acts like ROM.)
  //  p = ROM Power (0 = enabled, 1 = off.)  Only power the ROM if you're
  //    wanting to access the ROM for multiloads.  Otherwise set to 1.

  // Handle ROM power configuration
  myPower = !(configuration & 0x01);

  if(myPower)
  {
    myPowerRomCycle = mySystem->cycles();
  }

  myWriteEnabled = configuration & 0x02;

  switch((configuration >> 2) & 0x07)
  {
    case 0:
    {
      myImageOffset[0] = 2 * 2048;
      myImageOffset[1] = 3 * 2048;
      break;
    }

    case 1:
    {
      myImageOffset[0] = 0 * 2048;
      myImageOffset[1] = 3 * 2048;
      break;
    }

    case 2:
    {
      myImageOffset[0] = 2 * 2048;
      myImageOffset[1] = 0 * 2048;
      break;
    }

    case 3:
    {
      myImageOffset[0] = 0 * 2048;
      myImageOffset[1] = 2 * 2048;
      break;
    }

    case 4:
    {
      myImageOffset[0] = 2 * 2048;
      myImageOffset[1] = 3 * 2048;
      break;
    }

    case 5:
    {
      myImageOffset[0] = 1 * 2048;
      myImageOffset[1] = 3 * 2048;
      break;
    }

    case 6:
    {
      myImageOffset[0] = 2 * 2048;
      myImageOffset[1] = 1 * 2048;
      break;
    }

    case 7:
    {
      myImageOffset[0] = 1 * 2048;
      myImageOffset[1] = 2 * 2048;
      break;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::initializeROM()
{
  static uInt8 dummyROMCode[] = {
    0xa5, 0xfa, 0x85, 0x80, 0x4c, 0x18, 0xf8, 0xff, 
    0xff, 0xff, 0x78, 0xd8, 0xa0, 0x0, 0xa2, 0x0, 
    0x94, 0x0, 0xe8, 0xd0, 0xfb, 0x4c, 0x50, 0xf8, 
    0xa2, 0x0, 0xbd, 0x6, 0xf0, 0xad, 0xf8, 0xff, 
    0xa2, 0x0, 0xad, 0x0, 0xf0, 0xea, 0xbd, 0x0, 
    0xf7, 0xca, 0xd0, 0xf6, 0x4c, 0x50, 0xf8, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xa2, 0x3, 0xbc, 0x1d, 0xf9, 0x94, 0xfa, 0xca, 
    0x10, 0xf8, 0xa0, 0x0, 0xa2, 0x28, 0x94, 0x4, 
    0xca, 0x10, 0xfb, 0xa2, 0x1c, 0x94, 0x81, 0xca, 
    0x10, 0xfb, 0xa9, 0x0, 0x85, 0x1b, 0x85, 0x1c, 
    0x85, 0x1d, 0x85, 0x1e, 0x85, 0x1f, 0x85, 0x19, 
    0x85, 0x1a, 0x85, 0x8, 0x85, 0x1, 0xa9, 0x10, 
    0x85, 0x21, 0x85, 0x2, 0xa2, 0x7, 0xca, 0xca, 
    0xd0, 0xfd, 0xa9, 0x0, 0x85, 0x20, 0x85, 0x10, 
    0x85, 0x11, 0x85, 0x2, 0x85, 0x2a, 0xa9, 0x5, 
    0x85, 0xa, 0xa9, 0xff, 0x85, 0xd, 0x85, 0xe, 
    0x85, 0xf, 0x85, 0x84, 0x85, 0x85, 0xa9, 0xf0, 
    0x85, 0x83, 0xa9, 0x74, 0x85, 0x9, 0xa9, 0xc, 
    0x85, 0x15, 0xa9, 0x1f, 0x85, 0x17, 0x85, 0x82, 
    0xa9, 0x7, 0x85, 0x19, 0xa2, 0x8, 0xa0, 0x0, 
    0x85, 0x2, 0x88, 0xd0, 0xfb, 0x85, 0x2, 0x85, 
    0x2, 0xa9, 0x2, 0x85, 0x2, 0x85, 0x0, 0x85, 
    0x2, 0x85, 0x2, 0x85, 0x2, 0xa9, 0x0, 0x85, 
    0x0, 0xca, 0x10, 0xe4, 0x6, 0x83, 0x66, 0x84, 
    0x26, 0x85, 0xa5, 0x83, 0x85, 0xd, 0xa5, 0x84, 
    0x85, 0xe, 0xa5, 0x85, 0x85, 0xf, 0xa6, 0x82, 
    0xca, 0x86, 0x82, 0x86, 0x17, 0xe0, 0xa, 0xd0, 
    0xc3, 0xa9, 0x2, 0x85, 0x1, 0xa2, 0x1c, 0xa0, 
    0x0, 0x84, 0x19, 0x84, 0x9, 0x94, 0x81, 0xca, 
    0x10, 0xfb, 0xa6, 0x80, 0xdd, 0x0, 0xf0, 0xa5, 
    0x80, 0x45, 0xfe, 0x45, 0xff, 0xa2, 0xff, 0xa0, 
    0x0, 0x9a, 0x4c, 0xfa, 0x0, 0xcd, 0xf8, 0xff, 
    0x4c
  };

  uInt32 size = sizeof(dummyROMCode);

  // Initialize ROM with illegal 6502 opcode that causes a real 6502 to jam
  for(uInt32 i = 0; i < 2048; ++i)
  {
    myImage[3 * 2048 + i] = 0x02; 
  }

  // Copy the "dummy" Supercharger BIOS code into the ROM area
  for(uInt32 j = 0; j < size; ++j)
  {
    myImage[3 * 2048 + j] = dummyROMCode[j];
  }

  // Finally set 6502 vectors to point to initial load code at 0xF80A of BIOS
  myImage[3 * 2048 + 2044] = 0x0A;
  myImage[3 * 2048 + 2045] = 0xF8;
  myImage[3 * 2048 + 2046] = 0x0A;
  myImage[3 * 2048 + 2047] = 0xF8;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeAR::checksum(uInt8* s, uInt16 length)
{
  uInt8 sum = 0;

  for(uInt32 i = 0; i < length; ++i)
  {
    sum += s[i];
  }

  return sum;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeAR::loadIntoRAM(uInt8 load)
{
  uInt16 image;

  // Scan through all of the loads to see if we find the one we're looking for
  for(image = 0; image < myNumberOfLoadImages; ++image)
  {
    // Is this the correct load?
    if(myLoadImages[(image * 8448) + 8192 + 5] == load)
    {
      // Copy the load's header
      memcpy(myHeader, myLoadImages + (image * 8448) + 8192, 256);

      // Verify the load's header 
      if(checksum(myHeader, 8) != 0x55)
      {
        cerr << "WARNING: The Supercharger header checksum is invalid...\n";
      }

      // Load all of the pages from the load
      bool invalidPageChecksumSeen = false;
      for(uInt32 j = 0; j < myHeader[3]; ++j)
      {
        uInt32 bank = myHeader[16 + j] & 0x03;
        uInt32 page = (myHeader[16 + j] >> 2) & 0x07;
        uInt8* src = myLoadImages + (image * 8448) + (j * 256);
        uInt8 sum = checksum(src, 256) + myHeader[16 + j] + myHeader[64 + j];

        if(!invalidPageChecksumSeen && (sum != 0x55))
        {
          cerr << "WARNING: Some Supercharger page checksums are invalid...\n";
          invalidPageChecksumSeen = true;
        }

        // Copy page to Supercharger RAM (don't allow a copy into ROM area)
        if(bank < 3)
        {
          memcpy(myImage + (bank * 2048) + (page * 256), src, 256);
        }
      }

      // Copy the bank switching byte and starting address into the 2600's
      // RAM for the "dummy" SC BIOS to access it
      mySystem->poke(0xfe, myHeader[0]);
      mySystem->poke(0xff, myHeader[1]);
      mySystem->poke(0x80, myHeader[2]);

      return;
    }
  }

  // TODO: Should probably switch to an internal ROM routine to display
  // this message to the user...
  cerr << "ERROR: Supercharger load is missing from ROM image...\n";
}