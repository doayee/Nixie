/*
	NixieDriver.cpp`
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

/*************************************************************************************
 * Includes
 ************************************************************************************/
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <NixieDriver.h>
#include <util/delay.h>

/*************************************************************************************
 * Definitions
 ************************************************************************************/
#define FADE_RESOLUTION 256 //the number of increments between fades

/*************************************************************************************
 * Macros
 ************************************************************************************/
#define load_timer1_count(val) uint8_t oldSREG = SREG; 	\
							   cli(); 				   	\
							   TCNT1 = (val);    		\
							   SREG = oldSREG

/*************************************************************************************
 * Globals
 ************************************************************************************/

//nixie variables
uint8_t _dataPin;
uint8_t _clockPin;
uint8_t _outputEnablePin;
uint8_t _strobePin;
uint16_t _dpMask = 0x0; //mask for the decimal points
bool _clockModeEnable = 0;
uint8_t _symbolMask[6] = {0,0,0,0,0,0}; //for each nixie (0 = NONE, 1 = IN15A, 2 = IN15B)
uint8_t _symbols[6] = {BLANK,BLANK,BLANK,BLANK,BLANK,BLANK};
uint8_t _symbolDivisor = 1;

//clock mode variables
long hours = 0;
long minutes = 0;
long seconds = 0;

//backlight variables
volatile uint8_t _pins[3]; //holds the pin declarations
volatile uint8_t currentColour[3] = {0,0,0}; //accesable from outside the library
volatile uint8_t currentFadeColour[3] = {0,0,0}; //not accesable from outside the library
volatile uint16_t timerCount; //incremented on each timer call
backlight::CycleType_t *currentNode;
backlight::CycleType_t *entry;
volatile uint16_t TIM1Preload = 0;

void* pt2object; //used to access backlight functions from outside the class
backlight* bl = (backlight*) pt2object;

/* Colours - see NixieDriver.h for values */
int backlight::black[3]  = 		{	BLACK	  };
int backlight::white[3]  = 		{ 	WHITE     };
int backlight::red[3]    = 		{ 	RED  	  };
int backlight::green[3]  = 		{ 	GREEN	  };
int backlight::blue[3]   = 		{	BLUE      };
int backlight::yellow[3] = 		{ 	YELLOW 	  };
int backlight::dimWhite[3] = 	{ 	DIMWHITE  };
int backlight::aqua[3] = 		{ 	AQUA      };
int backlight::magenta[3] = 	{	MAGENTA   };
int backlight::purple[3]= 		{	PURPLE    };


/* Holds the curve y = 32768 * (-cos(pi * x / 256) + 1) between 0 and 256
 *
 * This is useful as if you mix colours using this function over time the
 * fade appears much more natural than a simple linear fade.
 *
 * The PROGMEM keyword forces variable into program memory as opposed to
 * RAM, increasing available RAM for user.
 */
const uint16_t PROGMEM cosFade[256] =
{
    0, 0, 0, 65, 65, 65, 131, 131, 196,
	262, 262, 327, 393, 458, 524, 589, 720,
	786, 851, 983, 1048, 1179, 1245, 1376, 1507,
	1638, 1769, 1900, 2031, 2162, 2293, 2490, 2621,
	2752, 2949, 3145, 3276, 3473, 3669, 3866, 3997,
	4194, 4456, 4652, 4849, 5046, 5242, 5504, 5701,
	5963, 6160, 6422, 6684, 6881, 7143, 7405, 7667,
	7929, 8191, 8454, 8716, 8978, 9305, 9568, 9830,
	10157, 10420, 10747, 11009, 11337, 11665, 11927, 12255,
	12582, 12910, 13238, 13565, 13893, 14221, 14548, 14876,
	15204, 15531, 15859, 16252, 16580, 16908, 17301, 17628,
	18022, 18349, 18743, 19070, 19463, 19857, 20184, 20577,
	20971, 21298, 21692, 22085, 22478, 22871, 23264, 23592,
	23985, 24379, 24772, 25165, 25558, 25951, 26345, 26738,
	27131, 27524, 27917, 28376, 28769, 29163, 29556, 29949,
	30342, 30735, 31129, 31587, 31981, 32374, 32767, 33160,
	33553, 33947, 34405, 34799, 35192, 35585, 35978, 36371,
	36765, 37158, 37617, 38010, 38403, 38796, 39189, 39583,
	39976, 40369, 40762, 41155, 41549, 41942, 42270, 42663,
	43056, 43449, 43842, 44236, 44563, 44957, 45350, 45677,
	46071, 46464, 46791, 47185, 47512, 47906, 48233, 48626,
	48954, 49282, 49675, 50003, 50330, 50658, 50986, 51313,
	51641, 51969, 52296, 52624, 52952, 53279, 53607, 53869,
	54197, 54525, 54787, 55114, 55377, 55704, 55966, 56229,
	56556, 56818, 57080, 57343, 57605, 57867, 58129, 58391,
	58653, 58850, 59112, 59374, 59571, 59833, 60030, 60292,
	60488, 60685, 60882, 61078, 61340, 61537, 61668, 61865,
	62061, 62258, 62389, 62585, 62782, 62913, 63044, 63241,
	63372, 63503, 63634, 63765, 63896, 64027, 64158, 64289,
	64355, 64486, 64551, 64683, 64748, 64814, 64945, 65010,
	65076, 65141, 65207, 65272, 65272, 65338, 65403, 65403,
	65469, 65469, 65469, 65535, 65535, 65535, 65535
};

/*************************************************************************************
 * Nixie Class
 ************************************************************************************/

/*************************************************************************************
 * Name: 	nixie(int data, int clk, int oe, int srb)
 *
 * Params:	int dataPin - the data pin
 * 			int clk - the clock pin
 * 			int oe - the output enable pin
 * 			int srb - the strobe pin
 *
 * Returns: None.
 *
 * Desc:	Initialises the library with the pins specified.
 ************************************************************************************/
nixie::nixie(int data, int clk, int oe, int srb)
{
	setDataPin(data); //set up pins
	setClk(clk);
	setOE(oe);
	setSrb(srb);
	startupTransmission();
}

/*************************************************************************************
 * Name: 	nixie(int data, int clk, int oe)
 *
 * Params:	int dataPin - the data pin
 * 			int clk - the clock pin
 * 			int oe - the output enable pin
 *
 * Returns: None.
 *
 * Desc:	Initialises the library with the pins specified.
 ************************************************************************************/
nixie::nixie(int data, int clk, int oe)
{
	setDataPin(data); //set up pins
	setClk(clk);
	setOE(oe);
	startupTransmission();
}

/*************************************************************************************
 * Name: 	nixie(int data, int clk)
 *
 * Params:	int dataPin - the data pin
 * 			int clk - the clock pin
 *
 * Returns: None.
 *
 * Desc:	Initialises the library with the pins specified.
 ************************************************************************************/
nixie::nixie(int data, int clk)
{
	setDataPin(data); //set up pins
	setClk(clk);
	startupTransmission();
}

/*************************************************************************************
 * Name: 	~nixie()
 *
 * Params:	None.
 *
 * Returns: None.
 *
 * Desc:	Closes instance of library.
 ************************************************************************************/
nixie::~nixie()
{
}

/*************************************************************************************
 * Name: 	startupTransmission(void)
 *
 * Params:	None.
 *
 * Returns: None.
 *
 * Desc:	Clears all the bits in the HV5122s.
 ************************************************************************************/
void nixie::startupTransmission(void)
{
	uint16_t data[6] = {0x0,0x0,0x0,0x0,0x0,0x0};
	shift(data);
	shift(data);
	for(uint8_t i = 0; i < 6; i++)
		setDecimalPoint(i, 0);
}

/*************************************************************************************
 * Name: 	setDataPin(uint8_t data)
 *
 * Params:	data - the data pin.
 *
 * Returns: None.
 *
 * Desc:	Sets the data pin to the pin specified.
 ************************************************************************************/
void nixie::setDataPin(uint8_t data)
{
	pinMode(data, OUTPUT);
	_dataPin = data;
}

/*************************************************************************************
 * Name: 	setClk(uint8_t clk)
 *
 * Params:	clk - the clock pin.
 *
 * Returns: None.
 *
 * Desc:	Sets the clock pin to the pin specified.
 ************************************************************************************/
void nixie::setClk(uint8_t clk)
{
	pinMode(clk, OUTPUT);
	_clockPin = clk;
}

/*************************************************************************************
 * Name: 	setOE(uint8_t oe)
 *
 * Params:	oe - the output enable pin.
 *
 * Returns: None.
 *
 * Desc:	Sets the output enable pin to the pin specified, and writes it HIGH
 ************************************************************************************/
void nixie::setOE(uint8_t oe)
{
	pinMode(oe, OUTPUT);
	digitalWrite(oe, HIGH);
	_outputEnablePin = oe;
}

/*************************************************************************************
 * Name: 	setSrb(uint8_t srb)
 *
 * Params:	oe - the strobe pin.
 *
 * Returns: None.
 *
 * Desc:	Sets the strobe pin to the pin specified, and writes it HIGH
 ************************************************************************************/
void nixie::setSrb(uint8_t srb)
{
	pinMode(srb, OUTPUT);
	digitalWrite(srb, HIGH);
	_strobePin = srb;
}

/*************************************************************************************
 * Name: 	transmit(bool data)
 *
 * Params:	bool data - the value of the bit to transmit
 *
 * Returns: None.
 *
 * Desc:	Transmits a single bit of data to the HV5122s
 ************************************************************************************/
void nixie::transmit(bool data)
{
  digitalWrite(_clockPin, HIGH);
  digitalWrite(_dataPin, data);
  digitalWrite(_clockPin, LOW);
}

/*************************************************************************************
 * Name: 	shift(uint16_t data[])
 *
 * Params:	uint16_t[] data - array containing the bits to shift out (for the numbers
 * 							  only) - must be 6 elements long.
 *
 * Returns: None.
 *
 * Desc:	Shifts out all data to the HV5122s.
 ************************************************************************************/
void nixie::shift(uint16_t data[])
{
  blank(1);
  for(uint8_t i = 0; i < 8; i++)  //for the decimal points
	  transmit(_dpMask & (1 << i));
  for (int8_t i = 5; i >= 0; i--) //for all 6 tubes
  {
    for (int8_t j = 9; j >= 0; j--) //for all 10 segments
    {
      bool toShift = data[i] & (1 << j); //transfer the data into bool
      transmit(toShift); //send the data
    }
  }
  blank(0);
}

/*************************************************************************************
 * Name: 	displayDigits(int a, int b, int c, int d, int e, int f)
 *
 * Params:	int a -> f - the digits to be displayed on each tube
 *
 * Returns: None.
 *
 * Desc:	Displays a number, digits specified individually
 ************************************************************************************/
void nixie::displayDigits(int a, int b, int c, int d, int e, int f)
{
  uint8_t numbers[6] = {(uint8_t)a,(uint8_t)b, (uint8_t)c, (uint8_t)d, (uint8_t)e, (uint8_t)f};
  uint16_t data[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  for (uint8_t i = 0; i < 6; i++) //for each tube
  {
    if(numbers[i] >= 10)
	{
		data[i] = 0x0;
	}
    else if (numbers[i] != 0) 
      data[i] = (1 << ((numbers[i]) - 1)) ; //flip the correct bit
    else
      data[i] = (1 << 9); 
  }
  shift(data);
}

/*************************************************************************************
 * Name: 	disp(uint32_t num)
 *
 * Params:	uint32_t num - number to display
 *
 * Returns: None.
 *
 * Desc:	Displays a number, taking account of any symbols used and shifting the
 * 			number accordingly. This is a private function used in other display
 * 			functions.
 ************************************************************************************/
void nixie::disp(uint32_t num)
{
	uint8_t seg[6];
	seg[0] = num / 100000;        	/*seperate the number into it's component digits*/
	seg[1] = (num / 10000) % 10;
	seg[2] = (num / 1000) % 10;
	seg[3] = (num / 100) % 10;
	seg[4] = (num / 10) % 10;
	seg[5] = (num % 10);

	/* Deals with symbols */
	for(uint8_t i = 0; i < 5; i++)
	{
		if(_symbolMask[i])
		{
			for(uint8_t j = 5; j > i; j--) //move the set down one
				seg[j] = seg[j-1];
		}
	}
	
	/* Adds the symbol in */
	for(uint8_t i = 0; i < 6; i++)
		if(_symbolMask[i])
		{
			if(_symbolMask[i] == 1 && (_symbols[i] < 10))
				seg[i] = _symbols[i];
			else if(_symbolMask[i] == 2 && (_symbols[i] >= 10 && _symbols[i] < 20 ))
				seg[i] = _symbols[i] - 10;
			else seg[i] = BLANK;
		}

	displayDigits(seg[0], seg[1], seg[2], seg[3], seg[4], seg[5]); /*display the number*/
}

/*************************************************************************************
 * Name: 	display(long num)
 *
 * Params:	long num - number to display
 *
 * Returns: None.
 *
 * Desc:	Displays a long number.
 ************************************************************************************/
void nixie::display(long num)
{
	if(num < 0) num *= -1; //can't be dealing with negatives
	if(!_clockModeEnable) _dpMask = 0x0;
	num *= _symbolDivisor; //accounts for if a symbol is being used
	disp(num);
}

/*************************************************************************************
 * Name: 	display(int num)
 *
 * Params:	int num - number to display
 *
 * Returns: None.
 *
 * Desc:	Displays an int.
 ************************************************************************************/
void nixie::display(int num)
{
	if(num < 0) num *= -1;
	long longNum = num;
	if(!_clockModeEnable) _dpMask = 0x0;
	longNum *= _symbolDivisor; //accounts for if a symbol is being used
	disp(longNum);
}

/*************************************************************************************
 * Name: 	display(float num)
 *
 * Params:	float num - number to display
 *
 * Returns: None.
 *
 * Desc:	Displays a float, putting the decimal in the correct place, and taking
 * 			symbols into account.
 ************************************************************************************/
void nixie::display(float num)
{

	/* This is a function I wrote before I'd worked out that documentation was useful
	 * and necessary. The only thing I remember about it is that I thought my method
	 * was was neat and concise - unfortunately passed through the lens of time this
	 * translates to 'difficult to understand and annoying', and frankly I don't have
	 * the time to reverse engineer some small annoying function. If it breaks I
	 * will, but until then this will remain a mystery.
	 */

	if(num < 0) num *= -1;
	if(long(num*1000000) % 10 == 0) num += 0.000001;	/*fixes an issue which occurs when you input 2.1*/
	long mul = 1;	/*multiplier to turn float to int*/
	long i = 1;	
	uint8_t j = 5;	/*order of bit that must be flipped for DP*/
	while(num * i < 1)
		i*=10;
	while(num * mul < (100000/i)) {
		mul *= 10;
		j--;
	}
	if(_clockModeEnable)
	{
		_clockModeEnable = 0;
		_dpMask = 0x0;
	}
	long dispNum = long(num * mul);
	uint8_t k = 0;
	while(_symbolMask[k] && k++ != 5) j++;
	for(uint8_t i = 0; i < 6; i++)
	{
		setDecimalPoint(i, (i == j ? 1 : 0));
	}
	
	disp(dispNum);
}

/*************************************************************************************
 * Name: 	setDecimalPoint(int segment, bool state)
 *
 * Params:	int segment - the segment the decimal should appear before, indexed at 0
 *			bool state - the decimal state
 *
 * Returns: None.
 *
 * Desc:	Sets the state of the decimal point on a specified digit.
 ************************************************************************************/
void nixie::setDecimalPoint(int segment, bool state)
{
	if(segment > 5) return;
	if(state) _dpMask |= (1 << (7 - segment));
	else _dpMask &= (0xFF ^ (1 << (7 - segment)));
}

/*************************************************************************************
 * Name: 	blank(bool state)
 *
 * Params:	bool state - the blanking state
 *
 * Returns: None.
 *
 * Desc:	Sets the blanking.
 ************************************************************************************/
void nixie::blank(bool state) //blank function
{
	digitalWrite(_outputEnablePin, !state);
}

/*************************************************************************************
 * Name: 	setClockMode(bool state)
 *
 * Params:	bool state - the clock mode state
 *
 * Returns: None.
 *
 * Desc:	Enables/disables clock mode.
 ************************************************************************************/
void nixie::setClockMode(bool state)
{
	_clockModeEnable = state;	//sets the clock mode
	for(uint8_t i = 0; i < 6; i++)
		_symbolMask[i] = 0;
	_dpMask = state ? 0x50 : 0x0; //sets the Decimal point mask to 001010000 - i.e. hh.mm.ss
}

/*************************************************************************************
 * Name: 	setSegment(int segment, int symbolType)
 *
 * Params:	int segment - the segment we're setting
 * 			int symbolType - the tube type we're setting it to
 *
 * Returns: None.
 *
 * Desc:	Sets a segment to a specified tube type.
 ************************************************************************************/
void nixie::setSegment(int segment, int symbolType)
{
	if(segment > 5) return;
	if(symbolType > 2) return;
	if(symbolType && !_symbolMask[segment])
		_symbolDivisor *= 10;
	else if (!symbolType && _symbolMask[segment])
		_symbolDivisor /= 10;
	_symbolMask[segment] = symbolType;
}

/*************************************************************************************
 * Name: 	setSegment(int segment, int symbol)
 *
 * Params:	int segment - the segment we're setting
 * 			int symbolType - the symbol we're setting it to
 *
 * Returns: None.
 *
 * Desc:	Sets a segment to a specified symbol.
 ************************************************************************************/
void nixie::setSymbol(int segment, int symbol)
{
	if(segment > 5) return;
	if(!_symbolMask[segment]) return;
	
	_symbols[segment] = symbol;
}

/*************************************************************************************
 * Name: 	setTime(int h, int m, int s)
 *
 * Params:	int h - the hours to set
 * 			int m - the minutes to set
 * 			int s - the seconds to set
 *
 * Returns: None.
 *
 * Desc:	Sets the internal time variables.
 ************************************************************************************/
void nixie::setTime(int h, int m, int s)
{
	hours = h;
	minutes = m;
	seconds = s;
}

/*************************************************************************************
 * Name: 	setTime(char *strTime)
 *
 * Params:	char *strTime - string containing time in format "HH(x)MM(x)SS" where (x)
 * 							means [don't care].
 *
 * Returns: None.
 *
 * Desc:	Sets the internal time variables from a string.
 ************************************************************************************/
void nixie::setTime(char *strTime)
{
	if(strlen(strTime) < 8) return;
	int h, m, s;

	h = (10 * (strTime[0] - '0'));
	h += (strTime[1] - '0');

	m = (10 * (strTime[3] - '0'));
	m += (strTime[4] - '0');

	s = (10 * (strTime[6] - '0'));
	s += (strTime[7] - '0');

	setTime(h, m, s);
}

/*************************************************************************************
 * Name: 	setHours(int h)
 *
 * Params:	int h - the hours to set
 *
 * Returns: None.
 *
 * Desc:	Sets the internal hours variable.
 ************************************************************************************/
void nixie::setHours(int h)
{
	hours = h;
}

/*************************************************************************************
 * Name: 	setMinutes(int m)
 *
 * Params:	int m - the minutes to set
 *
 * Returns: None.
 *
 * Desc:	Sets the internal minutes variable.
 ************************************************************************************/
void nixie::setMinutes(int m)
{
	minutes = m;
}

/*************************************************************************************
 * Name: 	setSeconds(int s)
 *
 * Params:	int s - the seconds to set
 *
 * Returns: None.
 *
 * Desc:	Sets the internal seconds variable.
 ************************************************************************************/
void nixie::setSeconds(int s)
{
	seconds = s;
}

/*************************************************************************************
 * Name: 	updateTime(void)
 *
 * Params:	None.
 *
 * Returns: None.
 *
 * Desc:	Updates the displayed time to the internal variables.
 ************************************************************************************/
bool nixie::updateTime(void)
{
	if (!_clockModeEnable) return 0;

	int a = (hours > 23 ? BLANK : (hours / 10));
	int b = (hours > 23 ? BLANK : (hours % 10));
	int c = (minutes > 59 ? BLANK : (minutes / 10));
	int d = (minutes > 59 ? BLANK : (minutes % 10));
	int e = (seconds > 59 ? BLANK : (seconds / 10));
	int f = (seconds > 59 ? BLANK : (seconds % 10));

	displayDigits(a, b, c, d, e, f);

	return 1;
}

/*-----------------------------------BACKLIGHT--------------------------------------*/


/*************************************************************************************
 * Name: 	backlight(uint8_t redPin, uint8_t greenPin, uint8_t bluePin)
 *
 * Params:	uint8_t redPin - the analog pin for the red cathode
 * 			uint8_t greenPin - the analog pin for the green cathode
 * 			uint8_t bluePin - the analog pin for the blue cathode
 *
 * Returns: None.
 *
 * Desc:	Initialises the backlight object.
 ************************************************************************************/
backlight::backlight(int redPin, int greenPin, int bluePin)
{
	_pins[0] = redPin;
	_pins[1] = greenPin;
	_pins[2] = bluePin;
	for(uint8_t i = 0; i < 3; i++)
		pinMode(_pins[i], 1);
	setColour(black);
}

/*************************************************************************************
 * Name: 	setColour(uint8_t colour[])
 *
 * Params:	uint8_t colour[] - array storing the colour to set
 *
 * Returns: None.
 *
 * Desc:	Sets the backlight to a specified colour.
 ************************************************************************************/
void backlight::setColour(int colour[])
{
	for(uint8_t i = 0; i < 3; i++) {
		analogWrite(_pins[i], 255 - colour[i]);
		currentColour[i] = colour[i];
	}
}

/*************************************************************************************
 * Name: 	crossFade(uint8_t startColour[], uint8_t endColour[], uint8_t duration)
 *
 * Params:	uint8_t startColour[] - array storing the colour to start on
 * 			uint8_t endColour[] - array storing the colour to end on
 * 			uint8_t duration - the duration of the fade
 *
 * Returns: None.
 *
 * Desc:	Fades between two colours over a given time.
 ************************************************************************************/
void backlight::crossFade(int startColour[], int endColour[], int duration)
{
	int displayColour[3];
	setColour(startColour); //start at the start colour
	for (uint16_t i = 0; i < FADE_RESOLUTION; i++) { //for all the entries in the sine table
		uint16_t wordFromProgMem = pgm_read_word_near(cosFade + i);
		float sineFactor = (float)(wordFromProgMem) / 65535;

		for(uint8_t j = 0; j < 3; j++) { //for r, g, and b
			displayColour[j] = startColour[j] + uint8_t(sineFactor * (endColour[j] - startColour[j]));
			//fade along a sine wave (between -90 and +90) between the two colours
		}
		setColour(displayColour); 
		delay(duration/FADE_RESOLUTION); //delay by the correct amount
	}
	setColour(endColour); //end colour to finish
}

/*************************************************************************************
 * Name: 	fadeIn(uint8_t colour[], uint8_t duration)
 *
 * Params:	uint8_t colour[] - array storing the colour to fade in to
 * 			uint8_t duration - the duration of the fade
 *
 * Returns: None.
 *
 * Desc:	Fades from black into a colour.
 ************************************************************************************/
void backlight::fadeIn(int colour[], int duration)
{
	int displayColour[3];
	setColour(black); //start at black
	for(uint8_t i = 4; i < (FADE_RESOLUTION/2 - 1); i++) //for the first half of the fade, increase linearly
	{
		for(uint8_t j = 0; j < 3; j++)
			displayColour[j] = uint8_t(float(i*0.5/(FADE_RESOLUTION/2 - 1)) * colour[j]);
		setColour(displayColour);
		delay(duration/(FADE_RESOLUTION-4));
	}
	for(uint16_t i = (FADE_RESOLUTION/2 - 1); i < FADE_RESOLUTION; i++) //for the second half of the fade, increase according to the sine
	{
		for(uint8_t j = 0; j < 3; j++)
		{
			uint16_t wordFromProgMem = pgm_read_word_near(cosFade + i);
			float sineFactor = (float)(wordFromProgMem) / 65535;
			displayColour[j] = uint8_t(sineFactor * colour[j]);
		}
		setColour(displayColour);
		delay(duration/(FADE_RESOLUTION-4));
	}
	setColour(colour);
}

/*************************************************************************************
 * Name: 	fadeOut(uint8_t duration)
 *
 * Params:	uint8_t duration - the duration of the fade
 *
 * Returns: None.
 *
 * Desc:	Fades from the currentColour to black.
 ************************************************************************************/
void backlight::fadeOut(int duration)
{
	uint8_t colour[3];
	for(uint8_t i = 0; i < 3; i++)
		colour[i] = currentColour[i];
	int displayColour[3];
	for(uint8_t i = FADE_RESOLUTION - 1; i > (FADE_RESOLUTION/2 - 1); i--) //for the first half of the fade, decrease according to the sine
	{
		for(uint8_t j = 0; j < 3; j++)
		{
			uint16_t wordFromProgMem = pgm_read_word_near(cosFade + i);
			float sineFactor = (float)(wordFromProgMem) / 65535;
			displayColour[j] = uint8_t(sineFactor * colour[j]);
		}
		setColour(displayColour);
		delay(duration/(FADE_RESOLUTION-2));
	}
	for(uint8_t i = (FADE_RESOLUTION/2 - 1); i > 0; i--) //for the second half of the fade, decrease linearly
	{
		for(uint8_t j = 0; j < 3; j++)
			displayColour[j] = uint8_t(float(i*0.5/(FADE_RESOLUTION/2 - 1)) * colour[j]);
		setColour(displayColour);
		delay(duration/(FADE_RESOLUTION-2));
	}
	setColour(black);
}

/*************************************************************************************
 *--------------------------- Colour Fading Overview --------------------------------*
 *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
 *
 *	The background fading works by using TIMER1 to trigger a regular interrupt, which
 *	changes the colour. The timer will interrupt 256 timer per complete colour change,
 *	with a minimum (theoretical) colour change speed of 256us and a maximum of
 *	65535ms.
 *
 *  For the setup of the colour fading, we use a 2d array provided by the user, this
 *  should adhere to the following format:
 *
 *		{ Colour_1_Red, Colour_1_Green, Colour_1_Blue, Duration_1 },
 *		{ Colour_2_Red, Colour_2_Green, Colour_2_Blue, Duration_2 },
 *								  ...
 * 		{ Colour_N_Red, Colour_N_Green, Colour_N_Blue, Duration_N }
 *
 * 	Where 'Duration_(X)' refers to the duration of the fade from Colour_(X) to
 * 	Colour_(X+1).
 *
 * 	It takes this data and uses it to build a loop of CycleType_t's. These are a
 * 	custom type which each contain the colour, a duration, and a pointer to the
 * 	next CycleType_t in the loop. The final node in the loop points back to the
 * 	first node (hence making it a loop).
 *
 * 	There are 2 global CycleType_t's, which are:
 *
 * 		CycleType_t *currentNode; - The node we are currently interacting with
 * 		CycleType_t *entryNode;   - A node populated with the current colour at the
 * 							        time of the fade being initialised, which points
 * 							        to the 'first' node of the loop.
 *
 * 	These serve as the basis for all interaction with the loop.
 *
 * 	There are also 2 other globals which are needed for the colour fading, there are:
 *
 *  	uint8_t[3] currentFadeColour - this holds the current colour of the fade.
 *  	uint32_t timerCount 		 - this is incremented on each isr call and is
 *  						  		   used for tracking when we need to swap from one
 *  						  		   colour to the next.
 *
 * 	The fading itself is done in the following manner:
 * 	1) The user calls setFade with the setup array.
 * 	2) The loop is set up and all the types are populated. - setFade()
 * 	3) The timer is set up with a prescaler determined by the duration of the first
 * 	   node of the cycle. It will run at the max speed it can without overflowing
 * 	   the timer - this increases the resolution of the timing and thus the overall
 * 	   accuracy. The timer is then preloaded with a value such that is overflows in
 * 	   the desired time. - timerSetup()
 * 	4) The interrupt is enabled.
 * 	5) On the interrupt, the timer resets the pre-loaded value to interrupt again in
 * 	   the desired time. It then fetches values from the node and the cos fade table
 * 	   which it uses to compute the colour change required. Once this change has been
 * 	   implemented it incrememnts the counter and checks to see if a node swap is
 * 	   needed, if not it returns. - isr()
 * 	6) If a swap is needed then the timer interrupt is disabled, the current node is
 * 	   swapped to the next node, and the timer values are recalculated and applied.
 * 	   - swapNode()
 *
 *
 ************************************************************************************/


/*************************************************************************************
 * Name: 	setFade(uint8_t setup[][4], uint8_t numberOfColours, uint8_t fadeInTime)
 *
 * Params:	uint8_t setup[][4] 		- the setup array for the fade
 * 			uint8_t numberOfColours - the number of colours in the fade
 * 			uint8_t fadeInTime		- the time taken to fade in to the fade
 *
 * Returns: Bool - success or failure (failure usually caused by not enough memory).
 *
 * Desc:	Starts a background colour fade.
 ************************************************************************************/
bool backlight::setFade(int setup[][4], int fadeInTime)
{
	//get the memory for the first node
	backlight::CycleType_t *root = (backlight::CycleType_t *)malloc(sizeof(backlight::CycleType_t));

	//check we have enough
	if(root == NULL) return false;

	/* Determine length of array */
	uint8_t i = 0;
	while(setup[i++][3] != 0)
		if(i == 0) return false; //if it wraps

	//if the array consists of only ENDCYCLE
	if(i == 1) return false;

	//will hold the final pointer we need to wrap the loop
	backlight::CycleType_t *loopNode = buildLoop(root, (uint16_t (*)[4])setup, 0, i-2);

	/* make setup into a loop of cycletype_t */
	if(NULL == loopNode)
	{
		free((void *)root);
		return false;
	}

	/* wrap the loop */
	loopNode->next = root;

	/* Create entry node */
	entry = (backlight::CycleType_t *)malloc(sizeof(backlight::CycleType_t));
	if(entry == NULL)
	{
		freeLoop(root, loopNode);
		return false;
	}

	/* copy current colour to entry colour */
	memcpy((void*)entry->colour, (void *)currentColour, 3);

	entry->duration = fadeInTime;
	entry->next = root;

	currentNode = entry;

	/* reset the globals */
	timerCount = 0;

	timerSetup();

	load_timer1_count(TIM1Preload);


	/* set pwm timers running */
	for(uint8_t i = 0; i < 3; i++)
	{
		if (currentColour[i] == 0)
			analogWrite(_pins[i], 254);
		else if(currentColour[i] == 255)
			analogWrite(_pins[i], 1);
	}
	
	TIMSK1 |= (1 << TOIE1);

	return true;

}

/*************************************************************************************
 * Name: 	buildLoop(backlight::CycleType_t *node,
 * 					  backlight::CycleType_t *finalNode,
 * 					  uint8_t setup[][4], uint8_t index, uint8_t max)
 *
 * Params:	CycleType_t *node		- the working node.
 * 			CycleType_t *finalNode  - used to return the 'final' node
 * 			uint8_t setup[][4]		- the setup array
 * 			uint8_t index			- how far through the array we currently are
 * 			uint8_t max				- what the array maximum is
 *
 * Returns: Bool - success or failure (failure usually caused by not enough memory).
 *
 * Desc:	Starts a background colour fade.
 ************************************************************************************/
backlight::CycleType_t *backlight::buildLoop(backlight::CycleType_t *node, uint16_t setup[][4], uint8_t index, uint8_t max)
{
	/* Populate node cycletype with the colour */
	for(uint8_t i = 0; i < 3; i++)
		node->colour[i] = setup[index][i];

	/*Populate node cycletype with the duration */
	node->duration = setup[index][3];

	/* Base case */
	if(index == max)
	{
		node->next = NULL;
		return node;
	}

	/* Create child node */
	backlight::CycleType_t *next = (backlight::CycleType_t *)malloc(sizeof(backlight::CycleType_t));
	node->next = next;

	/* No memory!! */
	if(next == NULL)
		return NULL;

	backlight::CycleType_t *toReturn = buildLoop(next, setup, ++index, max);

	/* Recursive case */
	if (NULL != toReturn)
			return toReturn;

	/* if something failed further down the loop */
	free((void *)node);
	return NULL;
}

/*************************************************************************************
 * Name: 	freeLoop(backlight::CycleType_t *node,
 * 					 backlight::CycleType_t *endNode)
 *
 * Params:	CycleType_t *node		- the working node.
 * 			CycleType_t *endNode  	- used to return the 'final' node
 *
 * Returns: None.
 *
 * Desc:	Traverses the loop till the given end node, then returns freeing the
 * 			memory as it goes.
 ************************************************************************************/
void backlight::freeLoop(backlight::CycleType_t *node, backlight::CycleType_t *endNode)
{
	/* find last part of loop */
	if(node->next != endNode->next)
		freeLoop(node->next, endNode);

	/* free up the chain */
	free((void *) node);

	return;
}

/*************************************************************************************
 * Name: 	swapNode(backlight::CycleType_t *node)
 *
 * Params:	CycleType_t	*node - the current node
 *
 * Returns: None.
 *
 * Desc:	Swaps one node to the next.
 ************************************************************************************/
void backlight::swapNode()
{
	/* detach and stop timer */
	TIMSK1 &= ~(1 <<TOIE1);

	/* swap the node */
	currentNode = currentNode->next;

	/* reset everything */
	timerSetup();
	timerCount = 0;

	TIMSK1 |= (1 << TOIE1);

	return;
}

/*************************************************************************************
 * Name: 	timerSetup(void)
 *
 * Params:	None.
 *
 * Returns: None.
 *
 * Desc:	Configures the timer for the current node.
 ************************************************************************************/
void backlight::timerSetup(void)
{
	/* Reset timer 1 control registers */
	TCCR1A = 0;
	TCCR1B = 0;

	uint32_t usCycleTime = currentNode->duration; //must be done in 2 steps to prevent 16 bit int overflow
	usCycleTime *= 1000;
	uint32_t usTickTime = usCycleTime / FADE_RESOLUTION; // div 256
	uint16_t preload;

	/* Work out prescaler and preload val */
	if 	(usTickTime < 0x00007FFF) { //2MHz
		/* Period is 0.5us */
		preload = usTickTime << 1;
		TCCR1B |= (1 << CS11);
	} else if (usTickTime < 0x0003FFFF) { //250KHz
		/* Period is 4us */
		preload = usTickTime >> 2;
		TCCR1B |= (1 << CS10 | 1 << CS11);
	} else if (usTickTime < 0x000FFFFF) { //62.5KHz
		/* Period is 16us */
		preload = usTickTime >> 4;
		TCCR1B |= (1 << CS12);
	} else { //15.625KHz
		/* Period is 64us */
		preload = usTickTime >> 6;
		TCCR1B |= (1 << CS10 | 1 << CS12);
	}

	TIM1Preload = (uint16_t)(0xFFFF - preload);
}

/*************************************************************************************
 * Name: 	isr()
 *
 * Params:	None.
 *
 * Returns: None.
 *
 * Desc:	This is the ISR routine. Updates the colours on the backlight and checks
 * 			the arrayIndex.
 ************************************************************************************/
void backlight::isr()
{
	/* Preload the timer again - done here to increase timing accuracy */
	load_timer1_count(TIM1Preload);

	/* Reckon this will be quicker but not sure */
	uint8_t current[3], aim[3], disp[3];
	memcpy((void *)current, (void *)currentNode->colour, 		3*sizeof(uint8_t));
	memcpy((void *)aim, 	(void *)currentNode->next->colour, 	3*sizeof(uint8_t));

	/* Read the current cos fade value */
	uint16_t wordFromProgMem = pgm_read_word_near(cosFade + timerCount);
	float factor = (float)(wordFromProgMem) / 65535;

	/* Work out the new colours */
	for(uint8_t j = 0; j < 3; j++)
		disp[j]= current[j] + uint8_t(factor*(float)(aim[j] - 	current[j]));

	/* Apply them */
	updateAnalogPin(_pins[0], 255-disp[0]);
	updateAnalogPin(_pins[1], 255-disp[1]);
	updateAnalogPin(_pins[2], 255-disp[2]);

	/* Again reckon this will be quicker but a straightforward assignment may have to do */
	memcpy((void *)currentFadeColour, (void *)disp, 3*sizeof(uint8_t));

	/* if we need to swap nodes */
	if(++timerCount >= FADE_RESOLUTION)
		swapNode();

}

/*************************************************************************************
 * Name: 	stopFade(int stopColour[3], int duration)
 *
 * Params:	int stopColour[3] - the colour to fade to once stopped.
 * 			int duration - how long the fade should last.
 *
 * Returns: None.
 *
 * Desc:	This is the ISR routine. Updates the colours on the backlight and checks
 * 			the arrayIndex.
 ************************************************************************************/
void backlight::stopFade(int stopColour[3], int duration)
{
	/* detach and stop the ISR */
	TIMSK1 &= ~(1 << TOIE1);
	TCCR1B &= 0x00;

	/* free the memory in the loop */
	freeLoop(entry->next, entry);

	/* free the entry point */
	free((void *)entry);

	/* Can't use memcpy as int is 16-bit */
	int tempColour[3] = { currentFadeColour[0], currentFadeColour[1], currentFadeColour[2] };
	int tempStopColour[3] = { stopColour[0], stopColour[1], stopColour[2] };

	/* fade to stop colour */
	crossFade(tempColour, tempStopColour, duration);
}

/*************************************************************************************
 * Name: 	updateAnalogPin(volatile uint8_t pin, uint8_t val)
 *
 * Params:	uint8_t pin - the pin to update.
 * 			uint8_t value - the new value.
 *
 * Returns: None.
 *
 * Desc:	This updates the Capture/Compare register for the PWM pins, but does not
 * 			affect the state of the timers (unlike AnalogWrite). The PWM timers must
 * 			be running before this function is used, otherwise it will do nothing.
 ************************************************************************************/
void backlight::updateAnalogPin(volatile uint8_t pin, uint8_t val)
{
	
	switch(digitalPinToTimer(pin))
	{
		#if defined(TCCR0) && defined(COM00) && !defined(__AVR_ATmega8__)
		case TIMER0A:
			OCR0 = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR0A) && defined(COM0A1)
		case TIMER0A:
			OCR0A = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR0A) && defined(COM0B1)
		case TIMER0B:
			OCR0B = val; // set pwm duty
			break;
		#endif

		/*#if defined(TCCR1A) && defined(COM1A1)
		case TIMER1A:
			break;
		#endif

		#if defined(TCCR1A) && defined(COM1B1)
		case TIMER1B:
			break;
		#endif

		#if defined(TCCR1A) && defined(COM1C1)
		case TIMER1C:
			break;
		#endif*/

		#if defined(TCCR2) && defined(COM21)
		case TIMER2:
			OCR2 = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR2A) && defined(COM2A1)
		case TIMER2A:
			OCR2A = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR2A) && defined(COM2B1)
		case TIMER2B:
			OCR2B = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR3A) && defined(COM3A1)
		case TIMER3A:
			OCR3A = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR3A) && defined(COM3B1)
		case TIMER3B:
			OCR3B = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR3A) && defined(COM3C1)
		case TIMER3C:
			OCR3C = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR4A)
		case TIMER4A:
			OCR4A = val;	// set pwm duty
			break;
		#endif
		
		#if defined(TCCR4A) && defined(COM4B1)
		case TIMER4B:
			OCR4B = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR4A) && defined(COM4C1)
		case TIMER4C:
			OCR4C = val; // set pwm duty
			break;
		#endif
			
		#if defined(TCCR4C) && defined(COM4D1)
		case TIMER4D:				
			OCR4D = val;	// set pwm duty
			break;
		#endif

						
		#if defined(TCCR5A) && defined(COM5A1)
		case TIMER5A:
			OCR5A = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR5A) && defined(COM5B1)
		case TIMER5B:
			OCR5B = val; // set pwm duty
			break;
		#endif

		#if defined(TCCR5A) && defined(COM5C1)
		case TIMER5C:
			OCR5C = val; // set pwm duty
			break;
		#endif

		case NOT_ON_TIMER:
			break;
		default:
			break;
	}
}

/* ISR Vector lands here - calls backlight function to access private functions */
ISR(TIMER1_OVF_vect)
{
	bl->isr();
}
