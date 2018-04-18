/*
	NixieDriver.h
	Written by Thomas Cousins for http://doayee.co.uk
	
	Library to accompany the Nixie Tube Driver board from Doayee.
	
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef NixieDriver_h
#define NixieDriver_h

#define DEBUG

#define IN15A 1
#define IN15B 2
#define NUMBER 0

#define MU 0
#define MICRO 0
#define NANO 1
#define PERCENT 2
#define CAP_PI 3
#define KILO 4
#define KELVIN 4
#define MEGA 5
#define METERS 5
#define MINUTES 5
#define MILLI 6
#define PLUS 7
#define MINUS 8
#define PICO 9
#define WATTS 10
#define AMPS 11
#define OHMS 12
#define SIEMENS 14
#define SECONDS 14
#define VOLTS 15
#define HENRYS 16
#define HERTZ 17
#define FARADS 19
#define BLANK 99

#define RESOLUTION 65536    // Timer1 is 16 bit

#define BLACK 0,0,0 
#define WHITE 255,255,255
#define RED 255,0,0
#define GREEN 0,255,0
#define BLUE 0,0,255
#define YELLOW 255,175,0
#define DIMWHITE 100,100,100
#define AQUA 0,255,255
#define MAGENTA 255,0,100
#define PURPLE 190,0,255
#define ENDCYCLE 0,0,0,0

class nixie
{
	private:

		uint8_t _dataPin;
		uint8_t _clockPin;
		uint8_t _outputEnablePin;
		uint8_t _strobePin;
		uint16_t _dpMask = 0x0;
		bool _clockModeEnable = 0;
		uint8_t _symbolMask[6] = {0,0,0,0,0,0};
		uint8_t _symbols[6] = {BLANK,BLANK,BLANK,BLANK,BLANK,BLANK};
		uint8_t _symbolDivisor = 1;

		//static nixie *activate_object;
		void transmit(bool data);
		void shift(uint16_t data[]);
		void setDataPin(uint8_t data);
		void setClk(uint8_t clk);
		void setOE(uint8_t oe);
		void setSrb(uint8_t srb);
		void disp(uint32_t num);
		void startupTransmission(void);


	public:
	
		
		volatile long hours;
		volatile long minutes;
		volatile long seconds;
		
		nixie(int data, int clk, int oe, int srb);
		nixie(int data, int clk, int oe);
		nixie(int data, int clk);
		~nixie();
		
		void displayDigits(int a, int b, int c, int d, int e, int f);
		void display(float num);
		void display(long num);
		void display(int num);
		void setDecimalPoint(int segment, bool state);
		void blank(bool state);
		void setClockMode(bool state);
		void setTime(int h, int m, int s);
		void setTime(char* strTime);
		void setHours(int h);
		void setMinutes(int m);
		void setSeconds(int s);
		bool updateTime(void);
		void setSegment(int segment, int symbolType);
		void setSymbol(int segment, int symbol);
		
};

class backlight
{
	public:

		struct CycleType_t {
			uint8_t colour[3];
			uint16_t duration;
			CycleType_t *next;
		};

		static int black[3];
		static int white[3];
		static int red[3];
		static int green[3];
		static int blue[3];
		static int yellow[3];
		static int dimWhite[3];
		static int magenta[3];
		static int aqua[3];
		static int purple[3];
		
		volatile uint8_t currentColour[3];
		
		backlight(int redPin, int bluePin, int greenPin);
		
		void setColour(int colour[]);
		void crossFade(int startColour[], int endColour[], int duration);
		void fadeIn(int colour[], int duration);
		void fadeOut(int duration);
		bool setFade(int setup[][4], int fadeInTime);
		void stopFade(int stopColour[], int duration);
		void isr(void);

	private:

		void timerSetup(void);
		void updateAnalogPin(volatile uint8_t pin, uint8_t val);
		CycleType_t *buildLoop(CycleType_t *working, uint16_t setup[][4], uint8_t index, uint8_t max);
		void freeLoop(CycleType_t *node, CycleType_t *endNode);
		void swapNode(void);
};

// Arduino 0012 workaround
#undef int
#undef char
#undef long
#undef byte
#undef float
#undef abs
#undef round

#endif	
