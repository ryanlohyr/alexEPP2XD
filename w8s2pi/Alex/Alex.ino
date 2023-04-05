#include <serialize.h>
#include <stdarg.h>
#include "packet.h"
#include "constants.h"
#include <math.h>

typedef enum
{
  STOP = 0,
  FORWARD = 1,
  BACKWARD = 2,
  LEFT = 3,
  RIGHT = 4
} TDirection;

volatile TDirection dir = STOP;

/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.

#define COUNTS_PER_REV      160

// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC          20

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  6   // Left forward pin
#define LR                  5   // Left reverse pin
#define RF                  10  // Right forward pin
#define RR                  11  // Right reverse pin

#define PI 3.141592654
#define ALEX_LENGTH 16
#define ALEX_BREADTH 6

//change amt for movement configuration
// #define left 20;

float alexDiagonal = 0.0;
float alexCirc = 0.0;
/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks;
volatile unsigned long rightReverseTicks;

// Left and right encoder ticks for turning
volatile unsigned long leftForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns;
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;

// Variables to keep track of whether we have moved a commanded distance
unsigned long deltaDist;
unsigned long newDist;
unsigned long deltaTicks;
unsigned long targetTicks;

//colour sensor variables 
#define S0 7
#define S1 8
#define S2 9
#define S3 12 
#define OUT 13
#define UPPERR 205
#define LOWERR 170
#define UPPERG 212
#define LOWERG 110
#define UPPERB
#define LOWERB

unsigned long R,G,B = 0;


//#define LedRed    A2
//#define LedGreen  A0
//#define LedBlue   A1


/*
 * 
 * Alex Communication Routines.
 * 
 */

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}
 
// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.s
  char buffer[PACKET_SIZE];
  int len;

 len = serialize(buffer, packet, sizeof(TPacket));
 writeSerial(buffer, len);
}

void sendColourStatus(){
  // Read Red freqency
  digitalWrite(S2,LOW);
  digitalWrite(S3,LOW);
  R = pulseIn(OUT, LOW);
  delay(100); 
  R = map(R, LOWERR, UPPERR, 255, 0);
  
  // Read Green freqency
  digitalWrite(S2,HIGH);
  digitalWrite(S3,HIGH);
  G = pulseIn(OUT, LOW);  // Reading the output Green frequency
  delay(100);
  G = map(G, LOWERG, UPPERG, 255, 0);
  
  // Setting Blue frequency, delete later
//  digitalWrite(S2,LOW);
//  digitalWrite(S3,HIGH);
//  B = pulseIn(OUT, LOW);  // Reading the output Blue frequency
//  delay(100);
//  //B = map(B, LOWERB, UPPERB, 255, 0);

  dbprintf("R = %i", R);
  dbprintf("G = %i", G);
}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.

   TPacket statusPacket;
   statusPacket.packetType = PACKET_TYPE_RESPONSE;
   statusPacket.command = RESP_STATUS;
   statusPacket.params[0] = leftForwardTicks;
   statusPacket.params[1] = rightForwardTicks;
   statusPacket.params[2] = leftReverseTicks;
   statusPacket.params[3] = rightReverseTicks;
   statusPacket.params[4] = leftForwardTicksTurns;
   statusPacket.params[5] = rightForwardTicksTurns;
   statusPacket.params[6] = leftReverseTicksTurns;
   statusPacket.params[7] = rightReverseTicksTurns;
   statusPacket.params[8] = forwardDist;
   statusPacket.params[9] = reverseDist;
   sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void dbprintf(char *format, ...){
   va_list args;
   char buffer[128];

   va_start(args,format);
   vsprintf(buffer, format ,args);
   sendMessage(buffer);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);

}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}


/*
 * and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pins
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs.
  DDRB |= 0b1100;
  PORTD |= 0b1100;
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{
  switch(dir){
    case FORWARD:
      leftForwardTicks ++;
      forwardDist = (unsigned long) ((float) leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC); 
      break;
    case BACKWARD:
      leftReverseTicks ++;
      reverseDist = (unsigned long) ((float) leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC); 
      break;
    case LEFT:
      leftReverseTicksTurns ++;
      break;
    case RIGHT:
      leftForwardTicksTurns ++;
      break;
  }
}

void rightISR()
{
  switch(dir){
    case FORWARD:
      rightForwardTicks ++;
      break;
    case BACKWARD:
      rightReverseTicks ++;
      break;
    case LEFT:
     rightForwardTicksTurns ++;
      break;
    case RIGHT:
      rightReverseTicksTurns ++;
      break;
  }
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  EICRA |= 0b1010;
  EIMSK |= 0b11;
}

ISR(INT0_vect)
{
  leftISR();
}

ISR(INT1_vect)
{
  rightISR();
}

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(9600);
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

/*
 * Alex's motor drivers.
 * 
 */

// Set up Alex's motors. Right now this is empty, but
// later you will replace it with code to set up the PWMs
// to drive the motors.
void setupMotors()
{
  /* Our motor set up is:  
   *    A1IN - Pin 5, PD5, OC0B
   *    A2IN - Pin 6, PD6, OC0A
   *    B1IN - Pin 10, PB2, OC1B
   *    B2In - pIN 11, PB3, OC2A
   */
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if(speed < 0.0)
    speed = 0;

  if(speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.

void forward(int dist, int speed)
{
//  //Activity 4 w8s2
  dbprintf("Dist: %i \n", dist);
  dbprintf("Speed: %i \n", speed);
  if(dist > 0) {
    deltaDist = dist;
  } else {
    deltaDist = 9999999;
  }
  newDist = forwardDist + deltaDist;
  dir = FORWARD;
  dbprintf("f ok\n");
  int val = pwmVal((float)speed);

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code. 
  analogWrite(LF, (val));
  analogWrite(RF, (val));
  analogWrite(LR,0);
  analogWrite(RR, 0);
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(int dist, int speed)
{
  dbprintf("Dist: %i \n", dist);
  dbprintf("Speed: %i \n", speed);
  //  //Activity 4 w8s2
  if(dist > 0) {
    deltaDist = dist;
  } else {
    deltaDist = 9999999;
  }
  newDist = forwardDist + deltaDist;
  dir = BACKWARD;
  dbprintf("fowardDist: %i \n", forwardDist);
  dbprintf("deltaDist: %i \n", deltaDist);
  dbprintf("Newdist: %i \n", newDist);

  int val = pwmVal((float)speed);

  // For now we will ignore dist and 
  // reverse indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  analogWrite(LR, val);
  analogWrite(RR, val);
  analogWrite(LF, 0);
  analogWrite(RF, 0);
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
unsigned long computeDeltaTicks(float ang)
{
  unsigned long ticks = (unsigned long) ((ang * alexCirc * COUNTS_PER_REV) / (360.0 * WHEEL_CIRC));
  return ticks;
}

void left(float ang, float speed)
{
  dir = LEFT;
  
  int val = pwmVal(speed);
  dbprintf("ang: %i \n", (int)ang);
  dbprintf("speed: %i \n", (int)speed);
  dbprintf("delta: %i", (int)deltaTicks);
  if(ang == 0){
    deltaTicks = 9999999;
  } else {
    deltaTicks = computeDeltaTicks((float)ang);
  }

  targetTicks = leftReverseTicksTurns + deltaTicks;
  dbprintf("Tticks = %ld\n", targetTicks);

  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn left we reverse the left wheel and move
  // the right wheel forward.
  analogWrite(LR, (val));
  analogWrite(RF, (val));
  analogWrite(LF, 0);
  analogWrite(RR, 0);
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(int ang, int speed)
{
  dir = RIGHT;
  dbprintf("Ang: %i \n", ang);
  dbprintf("Speed: %i \n", speed);
  
  
  int val = pwmVal(speed);

  if(ang == 0){
    deltaTicks = 9999999;
  } else {
    deltaTicks = computeDeltaTicks((float)ang);
  }

  targetTicks = leftForwardTicksTurns + deltaTicks;

  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn right we reverse the right wheel and move
  // the left wheel forward.
  analogWrite(RR, val);
  analogWrite(LF, val);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
}

// Stop Alex. To replace with bare-metal code later.
void stop()
{
  dir = STOP;
  
  analogWrite(LF, 0);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
  analogWrite(RR, 0);
  clearCounters();
}

// Clears all our counters
void clearCounters()
{
  leftForwardTicks = 0;
  rightForwardTicks = 0;
  leftReverseTicks = 0;
  rightReverseTicks = 0;
  leftForwardTicksTurns = 0;
  leftReverseTicksTurns = 0;
  rightForwardTicksTurns = 0;
  rightReverseTicksTurns = 0;
  leftRevs=0;
  rightRevs=0;
  forwardDist=0;
  reverseDist=0; 
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
}

// Intialize Alex's internal states

void initializeState()
{
  clearCounters();
}

void setupColour(){
  pinMode(S0, OUTPUT);
  pinMode(S1,OUTPUT);
  pinMode(S2,OUTPUT);
  pinMode(S3,OUTPUT);
  pinMode(OUT,INPUT);
  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW);
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        sendOK();
        forward((int) command->params[0], (int) command->params[1]);
        clearCounters();
      break;
      
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_REVERSE:
        sendOK();
        reverse((int) command->params[0], (int) command->params[1]);
        clearCounters();
      break;

    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_TURN_LEFT:
        sendOK();
        left((int) command->params[0], (int) command->params[1]);
        clearCounters();
      break;

    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_TURN_RIGHT:
        sendOK();
        right((int) command->params[0], (int) command->params[1]);
        clearCounters();
      break;

    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_STOP:
        sendOK();
        stop();
      break;

    // Activity 3 w8s2
    case COMMAND_GET_STATS:
        sendStatus();
      break;
    case COMMAND_IDENTIFY_COLOUR:
        sendOK();
        sendColourStatus();
        break;
    case COMMAND_CLEAR_STATS:
        clearOneCounter(command->params[0]);
        sendOK();
      break;
                  
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:
  //Compute diagonal
  alexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH));

  alexCirc = PI * alexDiagonal;
  
  cli();
  setupEINT();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  initializeState();
  setupColour();
  sei();
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      dbprintf("handlePacket");
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;  

    case PACKET_TYPE_MESSAGE:
      break;
    case PACKET_TYPE_HELLO:
      break;
  }
}

void loop() { 
  TPacket recvPacket; // This holds commands from the Pi
  TResult result = readPacket(&recvPacket); 
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket;
    }
  else
    if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 
  if (deltaDist > 0){
    if (dir == FORWARD){
      if (forwardDist > newDist){
        deltaDist = 0;
        newDist = 0;
        stop();
      
      }
    } else if (dir == BACKWARD){
       if (reverseDist > newDist){
        deltaDist = 0;
        newDist = 0;
        stop();
       }
    }
     else if (dir == STOP){
      deltaDist = 0;
      newDist = 0;
      stop();
    }
  }

  if (deltaTicks > 0) {
    if (dir == LEFT) {
    if (leftReverseTicksTurns >= targetTicks) {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
    }
  } else if (dir == RIGHT) {
      if (rightReverseTicksTurns >= targetTicks) {
        deltaTicks = 0;
        targetTicks = 0;
        stop(); 
      }
    } else if (dir == STOP) {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
    }
  }
  


  
}
