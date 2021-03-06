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
// Copyright (c) 1995-1999 by Bradford W. Mott
//
// See the file "license" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: FrameBufferSDL.cxx,v 1.1 2004/05/24 17:18:22 stephena Exp $
//============================================================================

#include <SDL.h>
#include <SDL_syswm.h>
#include <sstream>

#include "Console.hxx"
#include "FrameBuffer.hxx"
#include "FrameBufferSDL.hxx"
#include "MediaSrc.hxx"
#include "Settings.hxx"

#include "stella.xpm"   // The Stella icon

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FrameBufferSDL::FrameBufferSDL()
   :  x11Available(false),
      theZoomLevel(1),
      theMaxZoomLevel(1),
      theAspectRatio(1.0),
	  newPC(0),
      myPauseStatus(false)
{
  for(uInt32 addr = 0; addr < 4096; addr++)
  {
    myImage[addr] = pcImage[addr] = 0x000000FF;
	pcImageInfo[addr] = 0;
  }
  for(uInt32 addr = 0; addr < 1024; addr++)
  {
	  tiaImage[addr] = 0x000000FF;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FrameBufferSDL::~FrameBufferSDL()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::pauseEvent(bool status)
{
  myPauseStatus = status;
  setupPalette();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::setupPalette()
{
  // Shade the palette to 75% normal value in pause mode
  float shade = 1.0;
  if(myPauseStatus)
    shade = 0.75;

  const uInt32* gamePalette = myMediaSource->palette();
  for(uInt32 i = 0; i < 256; ++i)
  {
    Uint8 r, g, b;

    r = (Uint8) (((gamePalette[i] & 0x00ff0000) >> 16) * shade);
    g = (Uint8) (((gamePalette[i] & 0x0000ff00) >> 8) * shade);
    b = (Uint8) ((gamePalette[i] & 0x000000ff) * shade);

    myPalette[i] = mapRGB(r, g, b);
  }

  theRedrawEntireFrameIndicator = true;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::toggleFullscreen()
{
  bool isFullscreen = !myConsole->settings().getBool("fullscreen");

  // Update the settings
  myConsole->settings().setBool("fullscreen", isFullscreen);

  if(isFullscreen)
    mySDLFlags |= SDL_FULLSCREEN;
  else
    mySDLFlags &= ~SDL_FULLSCREEN;

  if(!createScreen())
    return;

  if(isFullscreen)  // now in fullscreen mode
  {
    grabMouse(true);
    showCursor(false);
  }
  else    // now in windowed mode
  {
    grabMouse(myConsole->settings().getBool("grabmouse"));
    showCursor(!myConsole->settings().getBool("hidecursor"));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int FrameBufferSDL::setImage(const unsigned char* img) {
  for(uInt32 addr = 0; addr < 4096; addr++)
  {
    //myImage[addr] = (img[addr] << 29) | (img[addr] << 17) |(img[addr] << 10) | 0x000000ff;
	  myImage[addr] = (img[addr] << 16) | (img[addr] << 8)| (img[addr] << 24)| 0x000000Ff;
  }
  return 1;
}

void FrameBufferSDL::doSomething(uInt16 newPC) {
	pcImage[newPC] = 0xFF0000FF;
}

void FrameBufferSDL::doTia(bool peek, uInt16 addr, uInt16 value) {
	if(peek) {
		uInt8 indexMultiplier = 0;
		addr = addr & 0x000F;
		uInt8 v = addr % 4;
		if(addr < 4) {
		} else if(addr < 8) {
			indexMultiplier = 1;
		} else if(addr < 12) {
			indexMultiplier = 2;
		} else {
			indexMultiplier = 3;
		}
		tiaImage[(indexMultiplier + 5) * 32 + v + 3] = 0x0000FFFF;

	} else{
		uInt8 indexMultiplier = 0;
		//addr = addr & 0x000F;
		uInt8 v = addr % 7;
		if(addr < 8) {
		} else if(addr < 16) {
			indexMultiplier = 1;
		} else if(addr < 24) {
			indexMultiplier = 2;
		} else if(addr < 32) {
			indexMultiplier = 3;
		} else if(addr < 40) {
			indexMultiplier = 4;
		} else if(addr < 48) {
			indexMultiplier = 5;
		} else if(addr < 56) {
			indexMultiplier = 6;
		} else if(addr < 64) {
			indexMultiplier = 7;
		}
		tiaImage[(indexMultiplier +15) * 32 + v + 12] = (value << 16) | 0x000000FF;
	}
	

}


void FrameBufferSDL::resize(int mode)
{
  // reset size to that given in properties
  // this is a special case of allowing a resize while in fullscreen mode
  if(mode == 0)
  {
    myWidth  = myMediaSource->width() << 1;
    myHeight = myMediaSource->height();
  }
  else if(mode == 1)   // increase size
  {
    if(myConsole->settings().getBool("fullscreen"))
      return;

    if(theZoomLevel == theMaxZoomLevel)
      theZoomLevel = 1;
    else
      theZoomLevel++;
  }
  else if(mode == -1)   // decrease size
  {
    if(myConsole->settings().getBool("fullscreen"))
      return;

    if(theZoomLevel == 1)
      theZoomLevel = theMaxZoomLevel;
    else
      theZoomLevel--;
  }

  if(!createScreen())
    return;

  // Update the settings
  myConsole->settings().setInt("zoom", theZoomLevel);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::showCursor(bool show)
{
  if(show)
    SDL_ShowCursor(SDL_ENABLE);
  else
    SDL_ShowCursor(SDL_DISABLE);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::grabMouse(bool grab)
{
  if(grab)
    SDL_WM_GrabInput(SDL_GRAB_ON);
  else
    SDL_WM_GrabInput(SDL_GRAB_OFF);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool FrameBufferSDL::fullScreen()
{
  return myConsole->settings().getBool("fullscreen");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 FrameBufferSDL::maxWindowSizeForScreen()
{
  if(!x11Available)
    return 4;

#ifdef UNIX
  // Otherwise, lock the screen and get the width and height
  myWMInfo.info.x11.lock_func();
  Display* theX11Display = myWMInfo.info.x11.display;
  myWMInfo.info.x11.unlock_func();

  int screenWidth  = DisplayWidth(theX11Display, DefaultScreen(theX11Display));
  int screenHeight = DisplayHeight(theX11Display, DefaultScreen(theX11Display));

  uInt32 multiplier = screenWidth / myWidth;
  bool found = false;

  while(!found && (multiplier > 0))
  {
    // Figure out the desired size of the window
    int width  = (int) (myWidth * multiplier * theAspectRatio);
    int height = myHeight * multiplier;

    if((width < screenWidth) && (height < screenHeight))
      found = true;
    else
      multiplier--;
  }

  if(found)
    return multiplier;
  else
    return 1;
#endif

  return 4;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void FrameBufferSDL::setWindowAttributes()
{
  // Set the window title
  ostringstream name;
  name << "Stella: \"" << myConsole->properties().get("Cartridge.Name") << "\"";
  SDL_WM_SetCaption(name.str().c_str(), "stella");

#ifndef MAC_OSX
  // Set the window icon
  uInt32 w, h, ncols, nbytes;
  uInt32 rgba[256], icon[32 * 32];
  uInt8  mask[32][4];

  sscanf(stella_icon[0], "%d %d %d %d", &w, &h, &ncols, &nbytes);
  if((w != 32) || (h != 32) || (ncols > 255) || (nbytes > 1))
  {
    cerr << "ERROR: Couldn't load the icon.\n";
    return;
  }

  for(uInt32 i = 0; i < ncols; i++)
  {
    unsigned char  code;
	char color[32];
    uInt32 col;

    sscanf(stella_icon[1 + i], "%c c %s", &code, color);
    if(!strcmp(color, "None"))
      col = 0x00000000;
    else if(!strcmp(color, "black"))
      col = 0xFF000000;
    else if (color[0] == '#')
    {
      sscanf(color + 1, "%06x", &col);
      col |= 0xFF000000;
    }
    else
    {
      cerr << "ERROR: Couldn't load the icon.\n";
      return;
    }
    rgba[code] = col;
  }

  memset(mask, 0, sizeof(mask));
  for(h = 0; h < 32; h++)
  {
    const char* line = stella_icon[1 + ncols + h];
    for(w = 0; w < 32; w++)
    {
      icon[w + 32 * h] = rgba[(int)line[w]];
      if(rgba[(int)line[w]] & 0xFF000000)
        mask[h][w >> 3] |= 1 << (7 - (w & 0x07));
    }
  }

  SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(icon, 32, 32, 32,
                         32 * 4, 0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000);
  SDL_WM_SetIcon(surface, (unsigned char *) mask);
  SDL_FreeSurface(surface);
#endif
}
