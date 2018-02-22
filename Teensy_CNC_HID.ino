#include "CNCAxis.h"

#include "MoveCommand.h"

#include <AccelStepper.h>

#include "TeensyCNCCore.h"

//#define useEncoders

#define DEBUG

#define xEnbl 0
#define xDirr 1
#define xStep 2

#define yEnbl 3
#define yDirr 4
#define yStep 5

#define zEnbl 6
#define zDirr 7
#define zStep 8

// Not used
/*#define aDirr 28
  #define aStep 29

  #define bDirr 21
  #define bStep 20

  #define cDirr 16
  #define cStep 15*/

#define s1_pin 33
#define s2_pin 36
#define s3_pin 39

#define engine_running 0
#define engine_paused 1
#define engine_emergency_stopped 2

#define axisMaxSteps 2000000000

TeensyCNCCore cncore;

#ifdef useEncoders
AccelStepper stepperX(&ForwardX, &BackwardX);
AccelStepper stepperY(&ForwardY, &BackwardY);
AccelStepper stepperZ(&ForwardZ, &BackwardZ);
/*AccelStepper stepperA(&ForwardA, &BackwardA);
  AccelStepper stepperB(&ForwardB, &BackwardB);
  AccelStepper stepperC(&ForwardC, &BackwardC);*/
#else
AccelStepper stepperX(AccelStepper::DRIVER, xStep, xDirr);
AccelStepper stepperY(AccelStepper::DRIVER, yStep, yDirr);
AccelStepper stepperZ(AccelStepper::DRIVER, zStep, zDirr);
/*AccelStepper stepperA(AccelStepper::DRIVER, aStep, aDirr);
  AccelStepper stepperB(AccelStepper::DRIVER, bStep, bDirr);
  AccelStepper stepperC(AccelStepper::DRIVER, cStep, cDirr);*/
#endif

unsigned int msBetweenReports = 1000;
unsigned int msBetweenReportsMoving = 50;

// RawHID packets are always 64 bytes
byte buffer[64];
elapsedMillis msUntilPositionReport;
elapsedMillis msUntilStatusReport;

void setup() {
  Serial.begin(9600);

  pinMode(s1_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(s1_pin), emergency_stop, FALLING );
  pinMode(s2_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(s2_pin), pause, FALLING );
  pinMode(s3_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(s3_pin), start_continue, FALLING );

  stepperX.setMaxSpeed(2000);
  stepperY.setMaxSpeed(2000);
  stepperZ.setMaxSpeed(2000);
  /*stepperA.setMaxSpeed(2000);
    stepperB.setMaxSpeed(2000);
    stepperC.setMaxSpeed(2000); */
}

void emergency_stop() {
  cncore.global_state.cnc_status.engine_state = engine_emergency_stopped;
}

void pause() {
  if ( cncore.global_state.cnc_status.engine_state == engine_running )
    cncore.global_state.cnc_status.engine_state = engine_paused;
}

void start_continue() {
  cncore.global_state.cnc_status.engine_state = engine_running;
}

void receiveCommand()
{
  int n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    for (int i = 0; i < 64; i++) {
#ifdef DEBUG
      Serial.print(buffer[i]);
#endif
    }
#ifdef DEBUG
    Serial.println("");
#endif

    cncore.global_state.USBCMDqueue.enqueue(buffer);

#ifdef DEBUG
    Serial.println(cncore.global_state.USBCMDqueue.count());
#endif
  }
}

void processBuffer(byte* buf)
{
  long type =  BytesToLong(buf[0], buf[1], buf[2], buf[3]);

#ifdef DEBUG
  Serial.println("Executing command " + String(type));
#endif

  switch (type) {
    case 256: //0x0100 immediate linear interpolation
      {
        MoveCommand mcc =  InterpolateLinear(cncore.global_state.cnc_position.x_steps, BytesToLong(buf[4], buf[5], buf[6], buf[7]),
                                             cncore.global_state.cnc_position.y_steps, BytesToLong(buf[8], buf[9], buf[10], buf[11]),
                                             cncore.global_state.cnc_position.z_steps, BytesToLong(buf[12], buf[13], buf[14], buf[15]),
                                             cncore.global_state.cnc_position.a_steps, BytesToLong(buf[16], buf[17], buf[18], buf[19]),
                                             cncore.global_state.cnc_position.b_steps, BytesToLong(buf[20], buf[21], buf[22], buf[23]),
                                             cncore.global_state.cnc_position.c_steps, BytesToLong(buf[24], buf[25], buf[26], buf[27]),
                                             BytesToFloat(buf[28], buf[29], buf[30], buf[31]),
                                             BytesToFloat(buf[32], buf[33], buf[34], buf[35]));

        cncore.global_state.cnc_speeds.x_speed = mcc.SpeedX;
        cncore.global_state.cnc_speeds.y_speed = mcc.SpeedY;
        cncore.global_state.cnc_speeds.z_speed = mcc.SpeedZ;
        cncore.global_state.cnc_speeds.a_speed = mcc.SpeedA;
        cncore.global_state.cnc_speeds.b_speed = mcc.SpeedB;
        cncore.global_state.cnc_speeds.c_speed = mcc.SpeedC;

        cncore.global_state.cnc_position.x_destination_steps = mcc.X >= axisMaxSteps ? cncore.global_state.cnc_position.x_destination_steps : mcc.X;
        cncore.global_state.cnc_position.y_destination_steps = mcc.Y >= axisMaxSteps ? cncore.global_state.cnc_position.y_destination_steps : mcc.Y;
        cncore.global_state.cnc_position.z_destination_steps = mcc.Z >= axisMaxSteps ? cncore.global_state.cnc_position.z_destination_steps : mcc.Z;
        cncore.global_state.cnc_position.a_destination_steps = mcc.A >= axisMaxSteps ? cncore.global_state.cnc_position.a_destination_steps : mcc.A;
        cncore.global_state.cnc_position.b_destination_steps = mcc.B >= axisMaxSteps ? cncore.global_state.cnc_position.b_destination_steps : mcc.B;
        cncore.global_state.cnc_position.c_destination_steps = mcc.C >= axisMaxSteps ? cncore.global_state.cnc_position.c_destination_steps : mcc.C;

        stepperX.moveTo(cncore.global_state.cnc_position.x_destination_steps);
        stepperX.setSpeed(mcc.SpeedX);

        stepperY.moveTo(cncore.global_state.cnc_position.y_destination_steps);
        stepperY.setSpeed(mcc.SpeedY);

        stepperZ.moveTo(cncore.global_state.cnc_position.z_destination_steps);
        stepperZ.setSpeed(mcc.SpeedZ);

        /*stepperA.moveTo(cncore.global_state.cnc_position.a_destination_steps);
          stepperA.setSpeed(mcc.SpeedA);

          stepperB.moveTo(cncore.global_state.cnc_position.b_destination_steps);
          stepperB.setSpeed(mcc.SpeedB);

          stepperC.moveTo(cncore.global_state.cnc_position.c_destination_steps);
          stepperC.setSpeed(mcc.SpeedC);*/

#ifdef DEBUG
        Serial.println("ParameterX : " + String(BytesToLong(buf[4], buf[5], buf[6], buf[7])));
        Serial.println("ParameterY : " + String(BytesToLong(buf[8], buf[9], buf[10], buf[11])));
        Serial.println("ParameterZ : " + String(BytesToLong(buf[12], buf[13], buf[14], buf[15])));
        Serial.println("ParameterA : " + String(BytesToLong(buf[16], buf[17], buf[18], buf[19])));
        Serial.println("ParameterB : " + String(BytesToLong(buf[20], buf[21], buf[22], buf[23])));
        Serial.println("ParameterC : " + String(BytesToLong(buf[24], buf[25], buf[26], buf[27])));
        Serial.println("ParameterXYZ : " + String(BytesToFloat(buf[28], buf[29], buf[30], buf[31])));
        Serial.println("ParameterABC : " + String(BytesToFloat(buf[32], buf[33], buf[34], buf[35])));

        Serial.println("SpeedX : " + String(mcc.SpeedX));
        Serial.println("SpeedY : " + String(mcc.SpeedY));
        Serial.println("SpeedZ : " + String(mcc.SpeedZ));
        Serial.println("SpeedA : " + String(mcc.SpeedA));
        Serial.println("SpeedB : " + String(mcc.SpeedB));
        Serial.println("SpeedC : " + String(mcc.SpeedC));

        Serial.println("DestinationX : " + String(mcc.X) + " | " + String(cncore.global_state.cnc_position.x_destination_steps));
        Serial.println("DestinationY : " + String(mcc.Y) + " | " + String(cncore.global_state.cnc_position.y_destination_steps));
        Serial.println("DestinationZ : " + String(mcc.Z) + " | " + String(cncore.global_state.cnc_position.z_destination_steps));
        Serial.println("DestinationA : " + String(mcc.A) + " | " + String(cncore.global_state.cnc_position.a_destination_steps));
        Serial.println("DestinationB : " + String(mcc.B) + " | " + String(cncore.global_state.cnc_position.b_destination_steps));
        Serial.println("DestinationC : " + String(mcc.C) + " | " + String(cncore.global_state.cnc_position.c_destination_steps));
#endif

        cncore.report_state();
        cncore.report_speeds();
      }
      break;
    case 257: // 0x0101 linear interpolation
      {
        cncore.global_state.cnc_status.line_number = BytesToLong(buf[4], buf[5], buf[6], buf[7]);

        MoveCommand mcc =  InterpolateLinear(cncore.global_state.cnc_position.x_steps, BytesToLong(buf[8], buf[9], buf[10], buf[11]),
                                             cncore.global_state.cnc_position.y_steps, BytesToLong(buf[12], buf[13], buf[14], buf[15]),
                                             cncore.global_state.cnc_position.z_steps, BytesToLong(buf[15], buf[17], buf[18], buf[19]),
                                             cncore.global_state.cnc_position.a_steps, BytesToLong(buf[20], buf[21], buf[22], buf[23]),
                                             cncore.global_state.cnc_position.b_steps, BytesToLong(buf[24], buf[25], buf[26], buf[27]),
                                             cncore.global_state.cnc_position.c_steps, BytesToLong(buf[28], buf[29], buf[30], buf[31]),
                                             BytesToFloat(buf[32], buf[33], buf[34], buf[35]),
                                             BytesToFloat(buf[36], buf[37], buf[38], buf[39]));



        cncore.global_state.cnc_speeds.x_speed = mcc.SpeedX;
        cncore.global_state.cnc_speeds.y_speed = mcc.SpeedY;
        cncore.global_state.cnc_speeds.z_speed = mcc.SpeedZ;
        cncore.global_state.cnc_speeds.a_speed = mcc.SpeedA;
        cncore.global_state.cnc_speeds.b_speed = mcc.SpeedB;
        cncore.global_state.cnc_speeds.c_speed = mcc.SpeedC;

        cncore.global_state.cnc_position.x_destination_steps = mcc.X >= axisMaxSteps ? cncore.global_state.cnc_position.x_destination_steps : mcc.X;
        cncore.global_state.cnc_position.y_destination_steps = mcc.Y >= axisMaxSteps ? cncore.global_state.cnc_position.y_destination_steps : mcc.Y;
        cncore.global_state.cnc_position.z_destination_steps = mcc.Z >= axisMaxSteps ? cncore.global_state.cnc_position.z_destination_steps : mcc.Z;
        cncore.global_state.cnc_position.a_destination_steps = mcc.A >= axisMaxSteps ? cncore.global_state.cnc_position.a_destination_steps : mcc.A;
        cncore.global_state.cnc_position.b_destination_steps = mcc.B >= axisMaxSteps ? cncore.global_state.cnc_position.b_destination_steps : mcc.B;
        cncore.global_state.cnc_position.c_destination_steps = mcc.C >= axisMaxSteps ? cncore.global_state.cnc_position.c_destination_steps : mcc.C;


        stepperX.moveTo(cncore.global_state.cnc_position.x_destination_steps);
        stepperX.setSpeed(mcc.SpeedX);

        stepperY.moveTo(cncore.global_state.cnc_position.y_destination_steps);
        stepperY.setSpeed(mcc.SpeedY);

        stepperZ.moveTo(cncore.global_state.cnc_position.z_destination_steps);
        stepperZ.setSpeed(mcc.SpeedZ);

        /*stepperA.moveTo(cncore.global_state.cnc_position.a_destination_steps);
          stepperA.setSpeed(mcc.SpeedA);

          stepperB.moveTo(cncore.global_state.cnc_position.b_destination_steps);
          stepperB.setSpeed(mcc.SpeedB);

          stepperC.moveTo(cncore.global_state.cnc_position.c_destination_steps);
          stepperC.setSpeed(mcc.SpeedC);*/

#ifdef DEBUG
        Serial.println("ParameterLine : " + String(BytesToLong(buf[4], buf[5], buf[6], buf[7])));
        Serial.println("ParameterX : " + String(BytesToLong(buf[8], buf[9], buf[10], buf[11])));
        Serial.println("ParameterY : " + String(BytesToLong(buf[12], buf[13], buf[14], buf[15])));
        Serial.println("ParameterZ : " + String(BytesToLong(buf[16], buf[17], buf[18], buf[19])));
        Serial.println("ParameterA : " + String(BytesToLong(buf[20], buf[21], buf[22], buf[23])));
        Serial.println("ParameterB : " + String(BytesToLong(buf[24], buf[25], buf[26], buf[27])));
        Serial.println("ParameterC : " + String(BytesToFloat(buf[28], buf[29], buf[30], buf[31])));
        Serial.println("ParameterXYZ : " + String(BytesToFloat(buf[32], buf[33], buf[34], buf[35])));
        Serial.println("ParameterABC : " + String(BytesToFloat(buf[36], buf[37], buf[38], buf[39])));

        Serial.println("SpeedX : " + String(mcc.SpeedX));
        Serial.println("SpeedY : " + String(mcc.SpeedY));
        Serial.println("SpeedZ : " + String(mcc.SpeedZ));
        Serial.println("SpeedA : " + String(mcc.SpeedA));
        Serial.println("SpeedB : " + String(mcc.SpeedB));
        Serial.println("SpeedC : " + String(mcc.SpeedC));

        Serial.println("DestinationX : " + String(mcc.X) + " | " + String(cncore.global_state.cnc_position.x_destination_steps));
        Serial.println("DestinationY : " + String(mcc.Y) + " | " + String(cncore.global_state.cnc_position.y_destination_steps));
        Serial.println("DestinationZ : " + String(mcc.Z) + " | " + String(cncore.global_state.cnc_position.z_destination_steps));
        Serial.println("DestinationA : " + String(mcc.A) + " | " + String(cncore.global_state.cnc_position.a_destination_steps));
        Serial.println("DestinationB : " + String(mcc.B) + " | " + String(cncore.global_state.cnc_position.b_destination_steps));
        Serial.println("DestinationC : " + String(mcc.C) + " | " + String(cncore.global_state.cnc_position.c_destination_steps));
#endif

        cncore.report_state();
        cncore.report_speeds();
      }
      break; //linear interpolation
    case 272: // 0x0110 set zero
      {
        long dimension1 =  BytesToLong(buf[8], buf[9], buf[10], buf[11]);

        switch (dimension1)
        {
          case 1: stepperX.setCurrentPosition(0); cncore.global_state.cnc_position.x_steps = 0; break;
          case 2: stepperY.setCurrentPosition(0); cncore.global_state.cnc_position.y_steps = 0; break;
          case 3: stepperZ.setCurrentPosition(0); cncore.global_state.cnc_position.z_steps = 0; break;
          /*case 4: stepperA.setCurrentPosition(0); cncore.global_state.cnc_position.a_steps = 0; break;
            case 5: stepperB.setCurrentPosition(0); cncore.global_state.cnc_position.b_steps = 0; break;
            case 6: stepperC.setCurrentPosition(0); cncore.global_state.cnc_position.c_steps = 0; break;*/
          case 0:
            stepperX.setCurrentPosition(0);
            stepperY.setCurrentPosition(0);
            stepperZ.setCurrentPosition(0);
            /*stepperA.setCurrentPosition(0);
              stepperB.setCurrentPosition(0);
              stepperC.setCurrentPosition(0);*/

            cncore.global_state.cnc_position.x_steps = 0;
            cncore.global_state.cnc_position.y_steps = 0;
            cncore.global_state.cnc_position.z_steps = 0;
            cncore.global_state.cnc_position.a_steps = 0;
            cncore.global_state.cnc_position.b_steps = 0;
            cncore.global_state.cnc_position.c_steps = 0;

            break;
        }

        cncore.report_positions();
      }
      break;

    case 273: // 0x0111 set position
      {
        long dimension = BytesToLong(buf[8], buf[9], buf[10], buf[11]);
        //(long)(buf[8]  | buf[9] << 8 | buf[10] << 16 | buf[11] << 24);
        long position = BytesToLong(buf[12], buf[13], buf[14], buf[15]);
        //(long)(buf[12]  | buf[13] << 8 | buf[14] << 16 | buf[15] << 24);
        switch (dimension)
        {
          case 1: stepperX.setCurrentPosition(position); break;
          case 2: stepperY.setCurrentPosition(position); break;
          case 3: stepperZ.setCurrentPosition(position); break;
          /*case 4: stepperA.setCurrentPosition(position); break;
            case 5: stepperB.setCurrentPosition(position); break;
            case 6: stepperC.setCurrentPosition(position); break;*/
          case 0:
            {
              stepperX.setCurrentPosition(position);
              stepperY.setCurrentPosition(position);
              stepperZ.setCurrentPosition(position);
              /*stepperA.setCurrentPosition(position);
                stepperB.setCurrentPosition(position);
                stepperC.setCurrentPosition(position);*/
            }
            break;
        }

        //msUntilPositionReport = 0;
        cncore.report_positions();
      }
      break;
  }
}

void loop()
{
  if (((cncore.global_state.USBCMDqueue.isEmpty() || cncore.global_state.cnc_status.engine_state != engine_running) &&  msUntilStatusReport > msBetweenReports) || msUntilStatusReport > msBetweenReports) {
    msUntilStatusReport = 0;
    cncore.report_state();
  }

  if (cncore.global_state.cnc_status.engine_state == engine_emergency_stopped) {
    cncore.global_state.USBCMDqueue = QueueArray <byte*>();
    cncore.global_state.ImmediateUSBCMDqueue = QueueArray <byte*>();

    cncore.global_state.cnc_position.x_destination_steps = XPos();
    cncore.global_state.cnc_position.y_destination_steps = YPos();
    cncore.global_state.cnc_position.z_destination_steps = ZPos();
    /*cncore.global_state.cnc_position.a_destination_steps = APos();
    cncore.global_state.cnc_position.b_destination_steps = BPos();
    cncore.global_state.cnc_position.c_destination_steps = CPos();*/

    stepperX.moveTo(XPos());
    stepperY.moveTo(YPos());
    stepperZ.moveTo(ZPos());
    /*stepperA.moveTo(APos());
      stepperB.moveTo(BPos());
      stepperC.moveTo(CPos());*/

    return;
  }

  cncore.global_state.cnc_position.x_steps = XPos();
  cncore.global_state.cnc_position.y_steps = YPos();
  cncore.global_state.cnc_position.z_steps = ZPos();
  /*cncore.global_state.cnc_position.a_steps = APos();
  cncore.global_state.cnc_position.b_steps = BPos();
  cncore.global_state.cnc_position.c_steps = CPos();*/

  receiveCommand();

  // every msBetweenReports, send a packet to the computer
  if ((DestinationReached() && msUntilPositionReport > msBetweenReportsMoving) || msUntilPositionReport > msBetweenReports) {
    msUntilPositionReport = 0;
    cncore.report_positions();
  }

  if (!cncore.global_state.USBCMDqueue.isEmpty() && !DestinationReached())
  {
    if (cncore.global_state.cnc_status.engine_state == engine_running)
      processBuffer(cncore.global_state.USBCMDqueue.dequeue());
    else
      processBuffer(cncore.global_state.ImmediateUSBCMDqueue.dequeue());
  }

  stepperX.runSpeedToPosition();
  stepperY.runSpeedToPosition();
  stepperZ.runSpeedToPosition();
  /*stepperA.runSpeedToPosition();
    stepperB.runSpeedToPosition();
    stepperC.runSpeedToPosition(); */

  //Serial.println(XPos());
  //Serial.println(stepperX.distanceToGo());
  //Serial.println("------------------------");
}

bool DestinationReached()
{
  //return (stepperX.distanceToGo() != 0  || stepperY.distanceToGo() != 0 || stepperZ.distanceToGo() != 0 || stepperA.distanceToGo() != 0 || stepperB.distanceToGo() != 0 || stepperC.distanceToGo() != 0);
  return (stepperX.distanceToGo() != 0  || stepperY.distanceToGo() != 0 || stepperZ.distanceToGo() != 0);
}