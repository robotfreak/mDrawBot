#include <EEPROM.h>
#include <Makeblock.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <Wire.h>

MePort stpB(PORT_1);
MePort stpA(PORT_2);
MePort servoPort(PORT_7);
int servopin =  servoPort.pin2();
Servo servoPen;

// arduino only handle A,B step mapping
float curSpd,tarSpd; // speed profile
float curX,curY,curZ;
float tarX,tarY,tarZ; // target xyz position
// step value
long curA,curB;
long tarA,tarB;
float curD,tarD; // target direction
/************** motor movements ******************/
void stepperMoveA(int dir)
{
//  Serial.printf("stepper A %d\n",dir);
  if(dir>0){
    stpA.dWrite1(LOW);
  }else{
    stpA.dWrite1(HIGH);
  }
  stpA.dWrite2(HIGH);
  stpA.dWrite2(LOW);
}

void stepperMoveB(int dir)
{
//  Serial.printf("stepper B %d\n",dir);
  if(dir>0){
    stpB.dWrite1(LOW);
  }else{
    stpB.dWrite1(HIGH);
  }
  stpB.dWrite2(HIGH);
  stpB.dWrite2(LOW);
}

/************** calculate movements ******************/
//#define STEPDELAY_MIN 200 // micro second
//#define STEPDELAY_MAX 1000
int stepAuxDelay=0;
int stepdelay_min=200;
int stepdelay_max=1000;
#define ACCELERATION 2 // mm/s^2 don't get inertia exceed motor could handle
#define SEGMENT_DISTANCE 10 // 1 mm for each segment
#define SPEED_STEP 1

void doMove()
{
  int mDelay=stepdelay_max;
  int speedDiff = -SPEED_STEP;
  int dA,dB,maxD;
  float stepA,stepB,cntA=0,cntB=0;
  int d;
  dA = tarA - curA;
  dB = tarB - curB;
  maxD = max(abs(dA),abs(dB));
  stepA = (float)abs(dA)/(float)maxD;
  stepB = (float)abs(dB)/(float)maxD;
  //Serial.printf("move: max:%d da:%d db:%d\n",maxD,dA,dB);
  //Serial.print(stepA);Serial.print(' ');Serial.println(stepB);
  for(int i=0;i<=maxD;i++){
    //Serial.printf("step %d A:%d B;%d\n",i,posA,posB);
    // move A
    if(curA!=tarA){
      cntA+=stepA;
      if(cntA>=1){
        d = dA>0?1:-1;
        stepperMoveA(d);
        cntA-=1;
        curA+=d;
      }
    }
    // move B
    if(curB!=tarB){
      cntB+=stepB;
      if(cntB>=1){
        d = dB>0?1:-1;
        stepperMoveB(d);
        cntB-=1;
        curB+=d;
      }
    }
    mDelay=constrain(mDelay+speedDiff,stepdelay_min,stepdelay_max)+stepAuxDelay;
    delayMicroseconds(mDelay);
    if((maxD-i)<((stepdelay_max-stepdelay_min)/SPEED_STEP)){
      speedDiff=SPEED_STEP;
    }
  }
  //Serial.printf("finally %d A:%d B;%d\n",maxD,posA,posB);
  curA = tarA;
  curB = tarB;
}

#define STEPS_PER_CIRCLE 3200 // 200*16
#define DIAMETER 64.0 // wheel diameter mm
#define WIDTH 123.5 // distance between wheel
#define STEP_PER_MM (STEPS_PER_CIRCLE/PI/DIAMETER)
void prepareMove()
{
  int maxD; 
  unsigned long t0,t1;
  float segInterval;
  float dx = tarX - curX;
  float dy = tarY - curY;
  float distance = sqrt(dx*dx+dy*dy);
  float distanceMoved=0,distanceLast=0;
  //Serial.print("distance=");Serial.println(distance);
  if (distance < 0.001) 
    return;
  // heading to correct direction 
  tarD = atan2(dy,dx);
  if(tarD>PI){
    tarD-=(2*PI);
  }
  float dAng = tarD - curD; 
  if(dAng>PI){
    dAng-=(2*PI);
  }else if(dAng<-PI){
    dAng+=(2*PI);
  }
  float dL = (dAng)/2*WIDTH;
  float dStep = dL*STEP_PER_MM;
  tarA = curA+dStep;
  tarB = curB+dStep;
  Serial.print("dir:"); Serial.print((tarD)/PI*180);
  Serial.print(" "); Serial.print(dx);Serial.print(" "); Serial.print(dy);Serial.print(" "); Serial.println(dL);
  doMove();
  curD = tarD;
  // move to targe xy position
  dStep = distance*STEP_PER_MM;
  tarA = curA+dStep;
  tarB = curB-dStep;
  Serial.print("dis:"); Serial.println(distance);
  doMove();
 
  curX = tarX;
  curY = tarY;
}

void initPosition()
{
  curX=0; curY=0;
  curA = 0;
  curB = 0;
  curD = 0;
}

/************** calculate movements ******************/
void parseCordinate(char * cmd)
{
  char * tmp;
  char * str;
  str = strtok_r(cmd, " ", &tmp);
  while(str!=NULL){
    str = strtok_r(0, " ", &tmp);
    //Serial.printf("%s;",str);
    if(str[0]=='X'){
      tarX = atof(str+1);
    }else if(str[0]=='Y'){
      tarY = atof(str+1);
    }else if(str[0]=='Z'){
      tarZ = atof(str+1);
    }else if(str[0]=='F'){
      float speed = atof(str+1);
      tarSpd = speed/60; // mm/min -> mm/s
    }else if(str[0]=='D'){ // vector direction
      
    }
  }
  prepareMove();
}


void parseGcode(char * cmd)
{
  int code;
  code = atoi(cmd);
  switch(code){
    case 1: // xyz move
      parseCordinate(cmd);
      break;
    case 28: // home
      tarX=0; tarY=0;
      prepareMove();
      break; 
  }
}

void echoRobotSetup()
{
  Serial.print("M10 MCAR ");
  Serial.print(WIDTH);Serial.print(' ');
  Serial.print(DIAMETER);Serial.print(' ');
  Serial.print(curX);Serial.print(' ');
  Serial.print(curY);Serial.print(' ');
  Serial.println(curD/PI*180);
}

void parsePen(char * cmd)
{
  char * tmp;
  char * str;
  str = strtok_r(cmd, " ", &tmp);
  int pos = atoi(tmp);
  servoPen.write(pos);
}

void parseMcode(char * cmd)
{
  int code;
  code = atoi(cmd);
  switch(code){
    case 1:
      parsePen(cmd);
      break;
    case 10:
      echoRobotSetup();
      break;
  }
}


void parseCmd(char * cmd)
{
  if(cmd[0]=='G'){ // gcode
    parseGcode(cmd+1);  
    Serial.println("OK");
  }else if(cmd[0]=='M'){ // mcode
    parseMcode(cmd+1);
    Serial.println("OK");
  }
}

/************** arduino ******************/
void setup() {
  Serial.begin(115200);
  servoPen.attach(servopin);
  initPosition();
}

char buf[64];
char bufindex;
char buf2[64];
char bufindex2;

void loop() {
  if(Serial.available()){
    char c = Serial.read();
    buf[bufindex++]=c;
    if(c=='\n'){
      parseCmd(buf);
      memset(buf,0,64);
      bufindex = 0;
    }
  }
}
