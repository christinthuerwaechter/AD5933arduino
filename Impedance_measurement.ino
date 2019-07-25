/*
    Reads impedance values from the AD5933 over I2C and prints them serially, changes 
    sensor settings, single frequency measurement or sweep option
*/

#include <Wire.h>
#include "AD5933.h"
#include <DueTimer.h>

#define START_FREQ  (80000)
#define FREQ_INCR   (1000)
#define MAX_NUM_INCR (500)

bool clockOn = false;
double gain[MAX_NUM_INCR];
int phase[MAX_NUM_INCR];
boolean Acknowledge = false;
boolean Acknowledge_disc = false;
String ack = "INIT_OK";
String disc = "DISCONNECT_OK";
String command;
String current_stage;
String input;
int startmarker;
char action;
String value_str;
int value;
boolean ex = false;
const float pi = 3.1415;
//Default settings for frequencies
int num_incr = 60;
int START_freq = 10000;
int FREQ_incr = 1000;
int num_cycles = 0;
int num_set;
int mult;
int32_t mask_PWM_pin = digitalPinToBitMask(2);
int clockPin = 2;
bool internal = 1;
int exClockPeriod;
int gain_ampl;
int excitation;
double temp;
bool flg_init = false;

void setup(void)
{
  // Begin I2C
  Wire.begin();

  // Begin serial at 9600 baud for output
  Serial.begin(9600); 
   
  while (!Acknowledge){
    if (Serial.available() > 0){
       command = Serial.readStringUntil('?');
       Acknowledge = command.equals(ack);
    }  
  }
  Serial.println(ack);

  // Perform initial configuration. Fail if any one of these fail.
  if (!(AD5933::reset() &&
        AD5933::setInternalClock(true) &&
        AD5933::setPGAGain(PGA_GAIN_X1))){
            flg_init = true;
        }
  current_stage = "SETUP";
  
  pinMode(2, OUTPUT);
  Timer3.attachInterrupt(myHandler);
  
  
}

void myHandler(){
  clockOn = !clockOn;

  digitalWrite(2, clockOn); 
}

  
bool executeWrite(char action, int value, int* num_incr, bool internal){

    switch (action){

      case 'A':
      //A means start frequency is changed
      AD5933::setStartFrequency(value, internal);
      START_freq=value;
      break;

      case 'B':
      //B means end frequency and therefore increment is changed 
      AD5933::setIncrementFrequency(value, internal);
      FREQ_incr=value;
      break;

      case 'C':
      //C means number of frequency increments and therefore increment is changed
      AD5933::setNumberIncrements(value);
      *num_incr = value;
      Serial.println("FREQ_UPDATE_OK");
      break;
      
      default:
      break;
     }

    
}

void frequencySweepEasy() {
      // Create arrays to hold the data
        int real[num_incr+1], imag[num_incr+1];
        Serial.println("MEAS_START");
        
      // Perform the frequency sweep
        if (AD5933::frequencySweep(real, imag, num_incr+1)) {
    
        int cfreq = START_freq;
        
        Serial.println("RECEIVE_VALUES");        
          for (int i = 0; i < num_incr+1; i++, cfreq += FREQ_incr) {          
            // Print frequency data
            Serial.println(real[i]);
            Serial.println(imag[i])
      
        }
      Serial.println("MEAS_DONE");
    }
    else {
      current_stage = "ERRORMODE";
      }
}


void loop(void)
{
   while  (current_stage.equals("SETUP")){
      
      //Read incoming command
      while (Serial.available()>0){
        input = Serial.readStringUntil('.');
        startmarker = input.indexOf('*');

  
      //Disconnect serial port
      if (input.substring(startmarker+1, input.length()).equals(disc)){
        Serial.end();
        Serial.println(input.substring(startmarker+1, input.length()));
      }

      // Internal clock mode
      if (input.charAt(startmarker+1) == 'I'){ 
        internal = 1;  
        Timer3.stop();
        if (!AD5933::setInternalClock(true)){
        }
        // Receive setting values
        if (input.charAt(startmarker+2) == 'S'){
        value_str = input.substring(startmarker+3, input.length());
        num_set = value_str.toInt();
        }
        if (input.charAt(startmarker+2) == 'M'){
          value_str = input.substring(startmarker+3, input.length());
          mult = value_str.toInt();

          if (!AD5933::setSettlingCycles(mult, num_set)){
            Serial.println("UPDATE_FAIL");
          }
          else{
          Serial.println("UPDATE_OK");
          }
        }
      }
      
      // External clock mode
      if (input.charAt(startmarker+1) == 'E'){
        internal = 0;
        if (!AD5933::setInternalClock(false)){
        }  
        // Receive clock period value
        if (input.charAt(startmarker+2) == 'C'){
          value_str = input.substring(startmarker+3, input.length());
          exClockPeriod = value_str.toInt();
          
          Timer3.start(exClockPeriod);
        }
   
        
        // Receive setting values
        if (input.charAt(startmarker+2) == 'S'){
          value_str = input.substring(startmarker+3, input.length());
          num_set = value_str.toInt();
        }
        if (input.charAt(startmarker+2) == 'M'){
          value_str = input.substring(startmarker+3, input.length());
          mult = value_str.toInt();

          if (!AD5933::setSettlingCycles(mult, num_set)){
            Serial.println("UPDATE_FAIL");
          }
          else{
          Serial.println("UPDATE_OK");
          }
        }
  
      }

      // Gain amplifier settings
      if (input.charAt(startmarker+1) == 'G'){ 
          value_str = input.substring(startmarker+2, input.length());
          gain_ampl = value_str.toInt();
          if (!AD5933::setPGAGain(gain_ampl)){
            current_stage = "ERRORMODE";
          }
          else{
            Serial.println("GAIN_UPDATE_OK");
          }
      }

      // Excitation Level setting
      if (input.charAt(startmarker+1) == 'X'){ 
          value_str = input.substring(startmarker+2, input.length());
          excitation = value_str.toInt();
          if (!AD5933::setExcitationRange(excitation)){
            current_stage = "ERRORMODE";
          }
          else{
            Serial.println("EXCITATION_UPDATE_OK");
          }
      }

      
      //Calibration Mode
      if (input.charAt(startmarker+1) == 'C'){
      current_stage = "CALIB";
      }
      
      //Measurement Mode (Frequency Sweep)
      if (input.charAt(startmarker+1) == 'M'){
      current_stage = "MEAS";
      }

      //Single Frequency Mode
      if (input.charAt(startmarker+1) == 'S'){
      current_stage = "SINGLE";
      }

      //Change Frequency Settings
      if (input.charAt(startmarker+1) == 'W'){
        action = input.charAt(startmarker+2);
        value_str = input.substring(startmarker+3, input.length());
        value = value_str.toInt();           

        if(executeWrite(action, value, &num_incr, internal)){
        }
        
        else{
          current_stage = "ERRORMODE";
        }        
      }
      
               
  }
   
  }

  while (current_stage.equals("CALIB")){
        Serial.println("CALIB_START");
        
        // Create arrays to hold the data
        int real[num_incr+1], imag[num_incr+1]; 
        // Perform the frequency sweep for calibration
        if (AD5933::frequencySweep(real, imag, num_incr+1)) { 
             
          int cfreq = START_freq;      
          Serial.println("RECEIVE_VALUES");        
          for (int i = 0; i < num_incr+1; i++, cfreq += FREQ_incr) {          
            // Print frequency data
            Serial.println(real[i]);
            Serial.println(imag[i]);
        }
     current_stage = "SETUP";      
    Serial.println("CALIB_DONE");
    }
    else{
            current_stage = "ERRORMODE";
        }
  }
  while (current_stage.equals("MEAS")){
        frequencySweepEasy();
        
        // Return to Setup Mode
         current_stage = "SETUP";
  }
  while (current_stage.equals("SINGLE")){
        Serial1.println("in Single Frequency Measurement Mode");
        temp = AD5933::getTemperature();
        // Create arrays to hold the data
        int real[num_incr+1], imag[num_incr+1];   
        // Perform the frequency sweep for one value
        if (AD5933::frequencySweep(real, imag, num_incr+1)) { 
                 
          Serial.println("RECEIVE_VALUES");        
                   
            // Print data
            Serial.println(real[0]);
            Serial.println(imag[0]);  
            Serial.println(temp);
            
        }
        // Return to Setup Mode
        Serial1.println("Return to setup mode");
        current_stage = "SETUP";
  }
        }
