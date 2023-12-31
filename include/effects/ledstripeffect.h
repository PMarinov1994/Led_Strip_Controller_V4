//+--------------------------------------------------------------------------
//
// File:        LEDStripEffect.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.  
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//   
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//   
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
// Description:
//
//    Defines the base class for effects that run locally on the chip
//
// History:     Apr-13-2019         Davepl      Created for NightDriverStrip
//              
//---------------------------------------------------------------------------

#pragma once

#include <sys/types.h>
#include <errno.h>
#include <iostream>
#include <vector>
#include <math.h>
#include "globals.h"
#include "colorutils.h"
#include "ledmatrixgfx.h"
#include "ntptimeclient.h"

#include <deque>
#include <memory>

// LEDStripEffect
//
// Base class for an LED strip effect.  At a minimum they must draw themselves and provide a unique name.

class LEDStripEffect
{
  protected:

	size_t _cLEDs;
	String _friendlyName;

    std::shared_ptr<LEDMatrixGFX> _GFX;

    inline static double randomDouble(double lower, double upper)
    {
        double result = (lower + ((upper - lower) * rand()) / RAND_MAX);
        return result;
    }

  public:

	LEDStripEffect(const char * pszName)
	{
		if (pszName)
			_friendlyName = pszName;
	}

	virtual ~LEDStripEffect()
	{
	}

    virtual bool Init(std::shared_ptr<LEDMatrixGFX> gfx)			
	{
		// There are up to 8 channel in play per effect and when we
		//   start up, we are given copies to their graphics interfaces
		//   so that we can call them directly later from other calls

		_GFX = gfx;    
        _cLEDs = _GFX->GetLEDCount();      
		//Serial.printf("Init Effect %s with %d LEDs\n", _friendlyName.c_str(), _cLEDs);
		return true;  
    }
	virtual void Draw() = 0;										// Your effect must implement these
	
	virtual const char *FriendlyName() const
	{
		if (_friendlyName.length() > 0)
			return _friendlyName.c_str();
		return "Unnamed Effect";
	}

	static inline CRGB RandomRainbowColor()
	{
		static const CRGB colors[] =
			{
				CRGB::Green,
				CRGB::Red,
				CRGB::Blue,
				CRGB::Orange,
				CRGB::Indigo,
				CRGB::Violet
            };
		int randomColorIndex = (int)randomDouble(0, ARRAYSIZE(colors));
		return colors[randomColorIndex];
	}

	static inline CRGB RandomSaturatedColor()
	{
		CRGB c;
		c.setHSV((uint8_t)randomDouble(0, 255), 255, 255);
		return c;
	}

	void fillSolidOnAllChannels(CRGB color, size_t iStart = 0, size_t numToFill = 0,  unsigned int everyN = 1)
	{
		if (numToFill == 0)
			numToFill = _cLEDs-iStart;

		if (iStart + numToFill > _cLEDs)
		{
			Println("Boundary Exceeded in FillRainbow");
			return;
		}
			
		for (size_t i = iStart; i < numToFill; i+= everyN)
			setPixel(i, color);
	}

	void fillRainbowAllChannels(size_t iStart, size_t numToFill, uint8_t initialhue, uint8_t deltahue, uint8_t everyNth = 1) const
	{
		if (iStart + numToFill > _cLEDs)
		{
			Println("Boundary Exceeded in FillRainbow");
			return;
		}

		CHSV hsv;
		hsv.hue = initialhue;
		hsv.val = 255;
		hsv.sat = 240;
		for (size_t i = 0; i < numToFill; i+=everyNth)
		{
			CRGB rgb;
			hsv2rgb_rainbow(hsv, rgb);
            setPixel(iStart + i, rgb);
			hsv.hue += deltahue;
			for (int q = 1; q < everyNth; q++)
				setPixel(iStart + i + q, CRGB::Black);
		}
	}

	// Helper functions to make it easier to port common strip effects

	inline void fadePixelToBlackOnAllChannelsBy(int pixel, uint8_t fadeValue) const
	{
		CRGB crgb = _GFX->getPixel(pixel);
		crgb.fadeToBlackBy(fadeValue);
		_GFX->GetLEDBuffer()[pixel] = crgb;
	}

	inline void fadeAllChannelsToBlackBy(uint8_t fadeValue) const
	{
		for (size_t i = 0; i < _cLEDs; i++)
			fadePixelToBlackOnAllChannelsBy(i, fadeValue);
	}

	inline void setAllOnAllChannels(uint8_t r, uint8_t g, uint8_t b) const
	{
		for (size_t i = 0; i < _cLEDs; i++)
			setPixel(i, r, g, b);
	}

	// setPixel
	//
	// Takes the call and replays it on all 8 (or however many) of the spokes

	inline void setPixel(size_t pixel, uint8_t r, uint8_t g, uint8_t b) const
	{
		#if STRAND && MIRROR_ALL_PIXELS
            _GFX[0]->drawPixel(STRAND_LEDS/2 + pixel, CRGB(r, g, b));
            _GFX[0]->drawPixel(STRAND_LEDS/2 - pixel, CRGB(r, g, b));
		#else
			if (pixel < 0 || pixel >= _cLEDs)
			{
				Serial.printf("Bad pixel index: %d\n", pixel);
				return;
			}
			setPixels(pixel, 1, CRGB(r, g, b), false);
		#endif
	}

	inline void setPixel(int pixel, const CRGB & color) const
	{
		setPixels(pixel, 1, color, false);
	}

	inline CRGB getPixel(int pixel) const
	{
		return _GFX->getPixel(pixel, 0);
	}

	// setPixels - Draw pixels with floating point accuracy by dimming/fading the lead/exit pixels

	inline void setPixels(float fPos, float count, CRGB c, bool bMerge = false) const
	{
		_GFX->setPixels(fPos, count, c, bMerge);
	}
};
