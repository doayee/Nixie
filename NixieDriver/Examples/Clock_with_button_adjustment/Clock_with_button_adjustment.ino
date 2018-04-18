/*
  selectbuttonclock.ino
  Written by Thomas McQueen for https://doayee.co.uk
  
  Example sketch for the Nixie Tube Driver from Doayee.
  
  This sketch is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This sketch is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*********************************************************************************
 * File Name: Clock_with_button_adjustment.ino
 *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
 * Description:
 *    This file is an example of how to use the clock mode, and the background
 *    rgb colour fading. 
 *    
 *    The main loop uses the millis() function to time a second, and updates the 
 *    time on the Nixie Tube Driver, whilst fading a rainbow colour pattern in the 
 *    background.
 *    
 *    The example also includes a function to set the time, using a three push-
 *    button interface. 
 *    
 *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
 * Hardware setup:
 *    The hardware for the Nixie Tube Driver should be setup as follows:
 *    
 *    Nixie Tube Driver | Arduino Pin
 *    ------------------|------------------
 *    Data              | 8
 *    Clock             | 9
 *    Output Enable     | 10
 *    Red cathode       | 3
 *    Green cathode     | 5
 *    Blue cathode      | 6
 *    
 *    Buttons should connect to +5V and have a 10k pull down on the input.
 *    
 *            Button
 *              _|_ 
 *    +5v -----o   o---------- Arduino Pin
 *                      |
 *                      -
 *                     | | 10k
 *                      -
 *                      |
 *                      |
 *                    -----
 *                     Gnd
 *    
 *    Button            | Arduino Pin
 *    ------------------|------------------
 *    Up                | 11
 *    Select            | 12
 *    Down              | 13
 *    
 ********************************************************************************/

#include <NixieDriver.h>

//Macro to wait for a debounce
#define Debounce() delay(100)

//Pin assignments for backlight pins
const int redPin = 3;
const int greenPin = 5;
const int bluePin = 6;

//Pin assignments for nixies
const int data = 8;
const int clock = 9;
const int oe = 10;

//Button assignments for incrementing
const int up_button = 11;
const int sel_button = 12;
const int down_button = 13;

nixie nixie(data, clock, oe);
backlight rgb(redPin, greenPin, bluePin);

//Create a background colour cycle
int colourCycle[8][4] = {
  {RED,    1000}, //Fade from red to yellow for 1s
  {YELLOW, 1000}, //Fade from yellow to green for 1s
  {GREEN,  1000}, //Fade from green to aqua for 1s
  {AQUA,   1000}, //Etc...
  {BLUE,   1000}, 
  {MAGENTA,1000},
  {PURPLE, 1000},
  {ENDCYCLE}      //Declare the end of the loop
};

//define the initial time
  int h;
  int m;
  int s;

/*************************************************************************************
 * Name:    setup(void)
 * Inputs:  None
 * Outputs: None
 * Desc:    Main program setup - runs once only  
 ************************************************************************************/
void setup() {
  //Begin serial comms
  Serial.begin(9600);

  //Assign buttons
  pinMode(up_button,INPUT);
  pinMode(sel_button,INPUT);
  pinMode(down_button,INPUT);

  //Attempt to begin fade
  if(!rgb.setFade(colourCycle, 1000)) 
  {
    //If we could not begin the fade
    Serial.println("Could not start fade!"); //print error
    while(1); //spin forever
  }
  
  nixie.setClockMode(1);    //set clock mode on
  
  nixie.setTime(__TIME__);//set the time to the compile time
  h = nixie.hours;          //update variables
  m = nixie.minutes;
  s = nixie.seconds;
}

/*************************************************************************************
 * Name:    loop(void)
 * Inputs:  None
 * Outputs: None
 * Desc:    Main program loop  
 ************************************************************************************/
void loop() {

  //record the current time
  uint32_t currentTime = millis(); 

  //do we want to change the time
  if(digitalRead(sel_button))
  SetNewTime();

  //wait until a second has passed
  while(millis()-currentTime<1000);

  //update the time
  nixie.updateTime();
  if (m == 59 && s == 59) h = (h == 23) ? 0 : h + 1;  //If we need to update hours, wrap around 23
  if (s == 59) m = (m == 59) ? 0 : m + 1;             //If we need to update minutes, wrap around 59
  s = (s == 59) ? 0 : s + 1;                          //Always update seconds and wrap round 59
  nixie.setTime(h, m, s);

}
  

/*************************************************************************************
 * Name:    SetNewTime(void)
 * Inputs:  None
 * Outputs: None
 * Desc:    Performs the setting of the time. Using an up/down/select buutton set. 
 ************************************************************************************/
void SetNewTime()
{
      Debounce();
      rgb.setColour(rgb.white);       //change the backlight so it's obvious we're here
      while(digitalRead(sel_button)); //wait for user to let go of set button to stop code running away
      
            
      nixie.setTime(h, BLANK, BLANK);         //display only the hours to the user
      nixie.updateTime(); 
      while(!digitalRead(sel_button)) //enter setting mode until hours are set
      {
        if(digitalRead(up_button))    //increment when up button pressed
        {
          Debounce();
          if(h==23) h=0; else h++;
          nixie.setTime(h, BLANK, BLANK);
          nixie.updateTime();
          delay(250);
        }
        if(digitalRead(down_button))  //decrement when down button pressed
        {
          Debounce();
          if(h==0) h=23; else h--;
          nixie.setTime(h, BLANK, BLANK);
          nixie.updateTime();
          delay(250);
        }
      }
      
      while(digitalRead(sel_button)); //wait for user to let go of set button to stop code running away
                                  
      nixie.setTime(BLANK,m,BLANK);           //display only the minutes to the user
      nixie.updateTime();
      while(!digitalRead(sel_button)) //enter setting mode until minutes are set  
      {
        if(digitalRead(up_button))    //increment when up button pressed
        {
          Debounce();
          if(m==59) m=0; else m++;
          nixie.setTime(BLANK, m, BLANK); 
          nixie.updateTime();
          delay(250);
        }
        if(digitalRead(down_button))  //decrement when down button pressed
        {
          Debounce();
          if(m==0) m=59; else m--;
          nixie.setTime(BLANK, m, BLANK);
          nixie.updateTime();
          delay(250);
        }
      }     
      
      while(digitalRead(sel_button)); //wait for user to let go of set button to stop code running away
                                        
      nixie.setTime(BLANK,BLANK,s);           //display only the seconds to the user
      nixie.updateTime();
      while(!digitalRead(sel_button)) //enter setting mode until seconds are set 
      {
        if(digitalRead(up_button))    //increment when up button pressed
        {
          Debounce();
          if(s==59) s=0; else s++;
          nixie.setTime(BLANK, BLANK, s);
          nixie.updateTime();
          delay(250);
        }
        if(digitalRead(down_button))  //decrement when down button pressed
        {
          Debounce();
          if(s==0) s=59; else s--;
          nixie.setTime(BLANK, BLANK, s);
          nixie.updateTime();
          delay(250);
        }
      }
      rgb.setFade(colourCycle, 1000);       //return the backlight to the running mode
      while(digitalRead(sel_button)); //wait for the button to be let up before returning to the main program
    }

  
   

  

