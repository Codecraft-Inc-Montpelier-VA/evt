/**************************** EVTModelActions.cpp ******************************
 *
 * This software is copyrighted © 2007 - 2020 by Codecraft, Inc.
 *
 * The following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license.
 *
 *******************************************************************************
 *
 *  This file contains model FSM actions for the Elevator Verification Test
 *  build. The Elevator Verification Test uses the Repeatable Random Test
 *  Generator (RRTGen) framework.
 *
 *  The Elevator simulation (server) program receives commands to move cars and
 *  to light up and down buttons and indicators on various floors.  The Elevator
 *  simulation uses its Terminal window to display the state of the simulation
 *  in realtime.
 *
 *  The server sends status notifications about the arrival of cars at various
 *  floors, the state of each car's door, the state of up and down floor call
 *  buttons, the state of floor call buttons in each car, and the state of the
 *  up and down indicator lights above each elevator door on each floor.
 *
 *  This file includes a series of numbered functions of the form:
 *
 *  void EVTModel_1042(void)
 *  {
 *     // [1042]
 *     <some actions>
 *  }
 *
 *  The code generator places a call to the above function when it encounters
 *  a corresponding reference to "[1042]" in generating the EVTModel_Insert.cpp
 *  file.
 */

/******************************* Configuration ********************************/
/* Uncomment the following define line for a histogram of inter-arrival times.*/
#define SHOW_HISTOGRAM
/******************************************************************************/

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <list>
//#include <ext/hash_map>
#include <map>
#include <string>
#include <errno.h>
#include <math.h>                // fabs, floor
#include <typeinfo>              // typeid (for Cygwin)
#include "EVTTestConstants.h"
#include "EVTModel_Insert.h"
#include "RRTGen.h"              // must precede EVT.h
#include "EVTPipesInterface.h"
#include "RRandom.h"
#include "Histospt.h"            // histogram support
#include "sccorlib.h"            // coroutine support

#include "EVTPipeCommands.cpp"

using namespace std;

extern unsigned int alertIssued;
extern int          alertMessageIndex;
extern int          commandPipeFd; // we write this pipe
extern Id           comparatorModelId;
extern int          currentNumberOfModelInstances;
extern Id           evtShowModelId;
extern bool         pipeOpIsSuccessful;
extern RRandom      rrs;         // random server instance
extern int          score;
extern FSM          *sender;
extern unsigned int showAlertIssued;
extern char         showCommandsIssued[];
extern shaftStatus  showElevator[]; // 1-indexed
extern shaftStatus  showModel[]; // 1-indexed
extern unsigned int showPoints;
extern unsigned int showTime;
extern int          statusPipeFd; // we read this pipe
extern bool         testing;
extern TransactionLog pt;        // named pipe transactions

const double mu = 3000.00; // ms between arrivals **??** TBD Get from a command line parm **??**

const  int          MAX_ECP_ENUM_VALUE_SIZE = 32 + 1; // 32 chars plus null

union ecpValue
{
   char             cValue[MAX_ECP_ENUM_VALUE_SIZE];
   int              iValue;
   double           dValue;
};

typedef struct
{
   string           valType;  // "Float", "Enum", "String", or "Int"
   string           units;
   ecpValue         before;
   ecpValue         after;
   string           command;
} ecpValuesType;

typedef map < string, ecpValuesType, less < string > > STR_EVT;

// A map of ("ECP name" : "value type", "units", before value, after value,
//                        "command string").
STR_EVT             ecpMap;

// An iterator for our map.
STR_EVT :: iterator seIter;

// A routine to enforce dynamic constraints.
typedef void (*CONSTRAINT)(char *value, bool isRange, char *range);

typedef struct
{
   string           ecpName;
   RandItems        randItem;       // repeatable EVTTestConstants.h randomizer
   void             *programmedVariable;
   string           valType;
   string           value;
   int              requirement1;
   int              requirement2;
   const char       *range;        // char value list or start,stop,resolution
   string           units;
   bool             isRange;
   CONSTRAINT       constraint;    // address of a constraint routine (or 0)
   string           command;       // followed immediately by the data
} ecps;

typedef FSM *pFSM;
//typedef ::__gnu_cxx::hash_map < string, pFSM > STR_pFSM;
typedef map < string, pFSM, less < string > > STR_pFSM;

// SLEV maps of ("floorLabel" + "shaftID" : <pointer to FSM for ShaftLevel>).
STR_pFSM            nonRequestedSlevMap;
STR_pFSM            requestedSlevMap;

// An iterator for our SLEV maps.
STR_pFSM :: iterator sfIter;

// An index for Transfer instances.
int  transferInstance = 0;

// Reduce a redundant form.
#define ElevatorMessage(a,b) if (pipeOpIsSuccessful) AttemptPipeOp(a, b)

// Routines defined elsewhere.
void  DoShowEvt(shaftStatus  *modeledElevatorStatus,
                 shaftStatus  *elevatorStatus,
                 unsigned int theTime,
                 char         *commandsIssued,
                 unsigned int alerts,
                 unsigned int points,
                 bool         testIsQuiescent);
bool InitializeShowEvt(int numberOfShafts);
bool ModelInfo(char *pMsg);

// Local prototypes.
int  AttemptPipeOp(char *operationName, int status);
void ConstrainFloorLabels(char *value, bool isRange, char *range);
void ConstrainGroundFloorLevel(char *value, bool isRange, char *range);
void ConstrainNumberOfFloors(char *value, bool isRange, char *range);
void ConstrainNumberOfShafts(char *value, bool isRange, char *range);
void PopulateEVT(void);
bool RandomizeEcps(ecps theEcps[]);
void StartEVT_Configuration(void);

const int           SAFE_SIZE = 32; // ample allocation for short strings

char                commandsIssued[MAX_NUMBER_OF_COMMAND_BYTES];
shaftStatus         elevatorStatus[MAX_NUMBER_OF_ELEVATORS + 1]; // 1-indexed
//direction           floorButtons[MAX_NUMBER_OF_FLOORS];
char                floorLabels[MAX_NUMBER_OF_FLOORS + 1]; // incl. end nul
int                 groundFloor;    //   (temp) 0-based level (from bottom)
#ifdef SHOW_HISTOGRAM
//TimeIntervalHistogram iatHist("EVT inter-arrival times (us)",
//                               0L, 250000, 40); // 0 - 10000 ms
#endif // def SHOW_HISTOGRAM
shaftStatus         modeledElevatorStatus[MAX_NUMBER_OF_ELEVATORS + 1];//1-idx
//direction           modeledFloorButtons[MAX_NUMBER_OF_FLOORS];
int                 numberOfElevators;// (temp)
int                 numberOfFloors; //   (temp)
unsigned int        points;
int                 programmedBlockClearTime;
int                 programmedDoorOpenCloseTime;
double              programmedFloorHeight;
char                programmedFloorLabels[SAFE_SIZE];
int                 programmedGroundFloorLevel;
double              programmedMaximumCabinVelocity;
int                 programmedMaxCloseAttempts;
int                 programmedNormalDoorWaitTime;
double              programmedMinimumStoppingDistance;
int                 programmedNumberOfElevatorShafts;
int                 programmedNumberOfFloors;
bool                simulatorProvidedBuildingDimensions;

// Elevator Configurable Parameters (ECPs).
// N.B.: Don't change the order without proper investigation of the consequences.
ecps elevatorEcps[] =
{
  // Block Clear Time is the amount of time (in ms) to wait after a door
  // is blocked before attempting to close it again.
  {"Block Clear Time",                  configBlockClearTime,
                                        &programmedBlockClearTime,
                                        "Int",  "", 1001, 1009,
                                        "3000,10000,500",
                                        "ms",
                                        true,
                                        0, // no constraints
                                        setBlockClearTime}, // command string

  // Door Open Close Time is the time (in ms) required to open or close
  // (without obstruction) the elevator door.
  {"Door Open Close Time",              configDoorOpenCloseTime,
                                        &programmedDoorOpenCloseTime,
                                        "Int", "", 1001, 1006,
                                        "2000,5000,100",
                                        "ms",
                                        true,
                                        0, // no constraints
                                        setDoorOpenCloseTime},

  // Floor Height is the distance between floors (in meters).
  {"Floor Height",                      configFloorHeight,
                                        &programmedFloorHeight,
                                        "Float", "3.048", 1001, 1013, // 10 feet
                                        "3.0,5.0,0.001",
                                        "meters",
                                        true,
                                        0, // no constraints
                                        setFloorHeight},

  // Maximum Cabin Velocity is the maximum velocity of the cabin
  // (in meters per second).
  {"Maximum Cabin Velocity",            configMaximumCabinVelocity,
                                        &programmedMaximumCabinVelocity,
                                        "Float", "", 1001, 1004,
                                        "0.5,2.0,0.1",
                                        "m/s",
                                        true,
                                        0, // no constraints
                                        setMaxCabinVelocity},

  // Maximum Close Attempts is the maximum number of consecutive attempts
  // allowed when trying to close an obstructed door.  When this value is
  // exceeded, the elevator shaft is brought out of service.
  {"Maximum Close Attempts",            configMaxCloseAttempts,
                                        &programmedMaxCloseAttempts,
                                        "Int",  "",  1001, 1010,
                                        "1,20,1",
                                        "attempts",
                                        true,
                                        0, // no constraints
                                        setMaxCloseAttempts},

  // Normal Door Wait Time is the time to keep the doors open while waiting
  // for passengers to transfer into or out of a cabin.
  {"Normal Door Wait Time",             configNormalDoorWaitTime,
                                        &programmedNormalDoorWaitTime,
                                        "Int",  "",  1001, 1008,
                                        "5000,15000,1000",
                                        "ms",
                                        true,
                                        0, // no constraints
                                        setNormalDoorWaitTime},

  // Minimum Stopping Distance is the distance (in meters) required to
  // safely decelerate the cabin to come to a stop.
  {"Minimum Stopping Distance",         configMinimumStoppingDistance,
                                        &programmedMinimumStoppingDistance,
                                        "Float","1.0", 1001, 1007,
                                        "0.1,10.0,0.1",
                                        "meters",
                                        true,
                                        0, // no constraints
                                        setMinStoppingDistance},

  // Number of Elevator Shafts is the count of elevator shafts in the building.
  {"Number of Elevator Shafts",         configNumberofElevatorShafts,
                                        &programmedNumberOfElevatorShafts,
                                        "Int", "", 1001, 1002,
                                        "1,9,1",
                                        "shafts",
                                        true,
                                        ConstrainNumberOfShafts,
                                        setNumberOfElevators},

  // Number of Floors is the count of floors in the building.
  {"Number of Floors",                  configNumberofFloors,
                                        &programmedNumberOfFloors,
                                        "Int", "", 1001, 1003,
                                        "2,9,1",
                                        "floors",
                                        true,
                                        ConstrainNumberOfFloors,
                                        setNumberOfFloors},

  // Ground Floor Level is the location of the ground floor in the building
  // from the bottom (0-indexed).
  {"Ground Floor Level",                configGroundFloorLevel,
                                        &programmedGroundFloorLevel,
                                        "Int", "", 1001, 1011,
                                        "0,8,1",
                                        "floor (0-indexed)",
                                        true,
                                        ConstrainGroundFloorLevel,
                                        setGroundFloorLevel},

  // Floor Labels are the (single-character) labels corresponding to each
  // floor of the building, from bottom to top.
  {"Floor Labels",                      configFloorLabels, // randomizer
                                        programmedFloorLabels,
                                        "Enum", "", 1001, 1012,
                                        "",
                                        "",
                                        false,             // not a range
                                        ConstrainFloorLabels,
                                        setFloorLabels},

  // Make this empty name the last item to end the table.
  {"",                                  unused, 0, "", "", 0, 0, "", "", false,
                                        0, ""}
};

////////////////////////////////////////////////////////////////////////////////
//
// AttemptPipeOp
//
// This utility accepts an elevator communucation request descriptor and the
// return value from the associated pipe request.  The utility returns the
// pipe operation's return value.
//
// An error message is constructed and sent to the console and to RRTLog if the
// pipe operation was not successful.  Note that a "successful" operation
// includes receiving an indication that the pipe is empty (EAGAIN).
//
// The global booleans pipeOpIsSuccessful and testing may also be updated
// by this routine.
//
// N.B.: the operation descriptor string length must be less than 60.
//

int  AttemptPipeOp(const char *operationName, int status)
{
   if (status == -1 && errno != EAGAIN) {
      const int eStrLen = 80;
      char      str[eStrLen + 80]; // ample
      char      eStr[eStrLen]; // should be ample
      strerror_r(errno, eStr, eStrLen);
      sprintf(str, "%s command failed: %s", operationName, eStr);
      alert(str);
      pipeOpIsSuccessful = false;
      testing = false;
   }
   return status;
}

int  Cabin_Model::EstimateTravelDelay(char floor, char callingDir)
{
   return 0;
}

void Cabin_Model::EVTModel_1400(void)
{
   // [1400]
   // START
   // **??** This assumes we start on the ground floor. It should be dynamic! **??**
   // The myDoor, myShaft, and myTransport member variables were established
   // when the instance was created.
   currentFloor    = programmedFloorLabels[programmedGroundFloorLevel];
   currentXfer     = 0; // none
   moving          = false;
   travelDirection = up;
}

void Cabin_Model::EVTModel_1401(void)
{
   // [1401]
   // NEW_TRANSFER
   //#define SHOW_CABIN_PROCESSING
   #ifdef SHOW_CABIN_PROCESSING
   {
      char str[80]; // ample
      sprintf(str, "   $$$ currentXfer->GetDestination(): %c, currentFloor: %c",
               currentXfer->GetDestination(), currentFloor);
      RRTLog(str);
   }
   #endif // def SHOW_CABIN_PROCESSING
   if (currentXfer->GetDestination() == currentFloor) {
      // We're already there.
      SendPriorityEvent(ALREADY_THERE);
   } else {
      // We'll be there soon, but first we'll move.
      #ifdef SHOW_CABIN_PROCESSING
      {
         char str[80]; // ample
         sprintf( str, "   $$$ sending PREPARE_TO_MOVE in Cabin_Model::EVTModel_1401" );
         RRTLog(str);
      }
      #endif // def SHOW_CABIN_PROCESSING
      SendPriorityEvent(PREPARE_TO_MOVE);
   }
}

void Cabin_Model::EVTModel_1402(char newDest, char unused)
{
   // [1402]
   // CHANGE_DESTINATION
   if (myTransport->GoToFloor(newDest)) {
      SendEvent(CABIN_REDIRECTED, newDest, ' ', currentXfer, this);
   }
   SendPriorityEvent(TRANSPORT_IN_PROGRESS);
}

void Cabin_Model::EVTModel_1403(void)
{
   // [1403]
   // TRANSPORT_IN_PROGRESS
   // No action is associated with this event.
}

void Cabin_Model::EVTModel_1404(void)
{
   // [1404]
   // ARRIVED_AT_FLOOR
   moving = false;
   SendEvent(CABIN_AT_DESTINATION, ' ', ' ', currentXfer, this);
   SendEvent(UNLOCK, ' ', ' ', myDoor, this);
   if (!myShaft->GetInService()) {
      SendPriorityEvent(TAKE_OUT_OF_SERVICE);
   }
}

void Cabin_Model::EVTModel_1405(void)
{
   // [1405]
   // NEW_TRANSFER
   EVTModel_1401();
}

void Cabin_Model::EVTModel_1406(void)
{
   // [1406]
   // ALREADY_THERE
   // Send a floor position msg to the pipe trace: <"@"><floor><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, carLocation);
   msg[1] =  currentFloor;
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   } else {
      EVTModel_1404();
   }
}

void Cabin_Model::EVTModel_1407(void)
{
   // [1407]
   // TAKE_OUT_OF_SERVICE
}

void Cabin_Model::EVTModel_1408(void)
{
   // [1408]
   // ARRIVED_AT_FLOOR
}

void Cabin_Model::EVTModel_1409(void)
{
   // [1409]
   // TAKE_OUT_OF_SERVICE
}

void Cabin_Model::EVTModel_1410(void)
{
   // [1410]
   // TRANSPORT_UNAVAILABLE
}

void Cabin_Model::EVTModel_1411(void)
{
   // [1411]
   // GO
   if (myTransport->GoToFloor(currentXfer->GetDestination())) {
      #ifdef SHOW_CABIN_PROCESSING
      {
         char str[80]; // ample
         sprintf( str, "   $$$ sending TRANSPORT_IN_PROGRESS in Cabin_Model::EVTModel_1411" );
         RRTLog(str);
      }
      #endif // def SHOW_CABIN_PROCESSING
      SendPriorityEvent(TRANSPORT_IN_PROGRESS);
   } else {
      SendPriorityEvent(TRANSPORT_UNAVAILABLE);
   }
}

void Cabin_Model::EVTModel_1412(void)
{
   // [1412]
   // TRANSPORT_IN_PROGRESS
   moving = true;
}

void Cabin_Model::EVTModel_1413(void)
{
   // [1413]
   // TAKE_OUT_OF_SERVICE
}

void Cabin_Model::EVTModel_1414(void)
{
   // [1414]
   // TAKE_OUT_OF_SERVICE
}

void Cabin_Model::EVTModel_1415(void)
{
   // [1415]
   // PREPARE_TO_MOVE
   myDoor->SetLockRequested(true);
   #ifdef SHOW_CABIN_PROCESSING
   RRTLog("   $$$ SetLockRequested(true) in EVTModel_1415.");
   #endif // def SHOW_CABIN_PROCESSING
   SendEvent(LOCK, ' ', ' ', myDoor, this);
}

void Cabin_Model::EVTModel_1416(void)
{
   // [1416]
   // DOORS_SECURE
   #ifdef SHOW_CABIN_PROCESSING
   RRTLog("   $$$ Sending DISPATCH_CABIN in EVTModel_1416.");
   #endif // def SHOW_CABIN_PROCESSING
   SendEvent(DISPATCH_CABIN, ' ', ' ', currentXfer, this);
}

char Cabin_Model::GetCurrentFloor(void)
{
   return currentFloor;
}

Transfer_Model *Cabin_Model::GetCurrentXfer(void)
{
   return currentXfer;
}

Door_Model* Cabin_Model::GetMyDoor(void)
{
   return myDoor;
}

Shaft_Model* Cabin_Model::GetMyShaft(void)
{
   return myShaft;
}

direction Cabin_Model::GetTravelDirection(void)
{
   return travelDirection;
}

char* Cabin_Model::NearestBLEV(char dir)
{
   char *pStr = 0;
   return pStr;
}

bool Cabin_Model::Ping(char *floor, direction *dir, bool searchingBehind)
{
   // Outward ping.
   direction searchDirection = travelDirection;
   if (searchingBehind) {
      searchDirection = (searchDirection == up) ? down : up;
   }
   *dir = searchDirection;
   //#define SHOW_PING_PROCESSING
   #ifdef SHOW_PING_PROCESSING
   {
      char *psd;
      switch (searchDirection) {
         case up:   psd = "up";   break;
         case down: psd = "down"; break;
         case none: psd = "none"; break;
         default:   psd = "????";
      }
      char *ptd;
      switch (travelDirection) {
         case up:   ptd = "up";   break;
         case down: ptd = "down"; break;
         case none: ptd = "none"; break;
         default:   ptd = "????";
      }
      char str[80]; // ample
      sprintf(str, "   ### travelDirection (in Ping): %s", ptd);
      RRTLog(str);
      sprintf(str, "   ### searchDirection (in Ping): %s", psd);
      RRTLog(str);
      sprintf(str, "   ### searchingBehind (in Ping): %s",
                                           searchingBehind ? "true" : "false");
      RRTLog(str);
      sprintf(str, "   ### currentFloor (in Ping): %c", currentFloor);
      RRTLog(str);
   }
   #endif // def SHOW_PING_PROCESSING

   bool requestFound = false;
   bool endOfShaft   = false;
   if (currentFloor == ' ') {
      alert("currentFloor has a bad value in Ping: ' '.");
      score += modelDiscrepancyFound;
      testing = false;
      return false;
   } else {
      char *pSearchFloor = strchr(programmedFloorLabels, currentFloor);
      if (pSearchFloor != NULL) {
         // Our initial search floor is OK.
         char shaftLevelInstance[SAFE_SIZE]; // ample
         do {
            #ifdef SHOW_PING_PROCESSING
            {
               char str[80]; // ample
               sprintf(str, "   >do top> currentFloor (in Ping): %c",
                        currentFloor);
               RRTLog(str);
               sprintf(str, "   >do top> *pSearchFloor (in Ping): %c",
                        *pSearchFloor);
               RRTLog(str);
            }
            #endif // def SHOW_PING_PROCESSING
            if (!moving || *pSearchFloor != currentFloor) {
               // Is there a requested SLEV here?
               sprintf(shaftLevelInstance, "S%i-%c", GetInstance(),
                        *pSearchFloor);
               sfIter = requestedSlevMap.find(shaftLevelInstance);
               if (sfIter != requestedSlevMap.end()) {
                  // The shaft level instance is there, so we'll determine if
                  // a stop was requested or if a floor call was made matching
                  // our current direction.
                  if (((ShaftLevel_Model *)(sfIter->second))->StopRequested()
                    || (((ShaftLevel_Model *)(sfIter->second))
                                                        ->Floors_requested(up)
                       && searchDirection == up)
                    || (((ShaftLevel_Model *)(sfIter->second))
                                                      ->Floors_requested(down)
                       && searchDirection == down)) {
                     requestFound = true;
                     *floor = *pSearchFloor;
                     #ifdef SHOW_PING_PROCESSING
                     {
                        char str[80]; // ample
                        sprintf(str, "   >do> stop requestFound (in Ping): %c",
                                 *floor);
                        RRTLog(str);
                     }
                     #endif // def SHOW_PING_PROCESSING
                     break;
                  }
               }
            }

            // Proceed to the next floor in the away direction.
            if (searchDirection == up) {
               pSearchFloor++;
               if (pSearchFloor - programmedFloorLabels
                                                 >= programmedNumberOfFloors) {
                  endOfShaft = true;
               }
            } else if (searchDirection == down) {
               pSearchFloor--;
               if (pSearchFloor < programmedFloorLabels)
               {
                  endOfShaft = true;
               }
            } else {
               alert("searchDirection has a bad value in Ping: none.");
               score += modelDiscrepancyFound;
               testing = false;
               return false;
            }
         } while (!endOfShaft);
      } else {
         alert("NULL pSearchFloor in Ping.");
         score += modelDiscrepancyFound;
         testing = false;
         return false;
      }
   }

   if (requestFound) {
      return true;
   }

   // Inward ping.
   char *pSearchFloor;
   if (searchDirection == up) {
      // Search from top floor to cabin.
      pSearchFloor = programmedFloorLabels + programmedNumberOfFloors - 1;
   } else { // searchDirection == down
      // Search from bottom floor to cabin.
      pSearchFloor = programmedFloorLabels;
   }

   // Get the floor closest to the cabin right now.
   char *pCabinFloor = strchr(programmedFloorLabels, currentFloor);

   bool atMyFloor;
   char shaftLevelInstance[SAFE_SIZE]; // ample
   do {
      if (!moving || *pSearchFloor != currentFloor) {
         // Is there a requested SLEV here?
         sprintf(shaftLevelInstance, "S%i-%c", GetInstance(),
                  *pSearchFloor);
         sfIter = requestedSlevMap.find(shaftLevelInstance);
         if (sfIter != requestedSlevMap.end()) {
            // The shaft level instance is there, so we'll determine if
            // a floor call was made opposite our travel direction.
            if ((((ShaftLevel_Model *)(sfIter->second))
                                                        ->Floors_requested(up)
                 && searchDirection == down)
              || (((ShaftLevel_Model *)(sfIter->second))
                                                      ->Floors_requested(down)
                 && searchDirection == up)) {
               requestFound = true;
               *floor = *pSearchFloor;
               break;
            }
         }
      }

      atMyFloor = pCabinFloor == pSearchFloor;
      if (!atMyFloor) {
         if (searchDirection == up) {
            pSearchFloor--;
         } else { // searchDirection == down
            pSearchFloor++;
         }
      }
   } while (!atMyFloor);

   return requestFound;
}

void Cabin_Model::Pong(void)
{
   // Pong is called whenever a call or stop is requested.  The purpose of
   // this method is to ensure that the travelDirection is correct for a
   // singleton request, before others may be added to their respective
   // ShaftLevels.  If we find just one stop request or floor call, then
   // we'll set the travelDirection to point toward the requested floor,
   // providing the cabin door is closed.  The original direction continues
   // until the door is closed.

   int       stopsOrCallsFound       = 0;    //   values
   char      shaftLevelInstance[SAFE_SIZE]; // ample
   char      *pSearchFloor;
   char      *pTargetFloor;

   if (currentFloor == ' ') {
      alert("currentFloor has a bad value in Pong: ' '.");
      score += modelDiscrepancyFound;
      testing = false;
      return;
   }

   if (!(myDoor->IsDoorClosed())) {
      return;
   }

   // Searching upward.
   for (pSearchFloor = programmedFloorLabels;
         pSearchFloor - programmedFloorLabels < programmedNumberOfFloors;
         pSearchFloor++) {
      //#define SHOW_PONG_PROCESSING
      #ifdef SHOW_PONG_PROCESSING
      {
         char str[80]; // ample
         sprintf(str, "   >up> *pSearchFloor (in Pong): %c",
                  *pSearchFloor);
         RRTLog(str);
      }
      #endif // def SHOW_PONG_PROCESSING
      if (!moving /*|| *pSearchFloor != currentFloor*/) { // **??** TBD Do we need something more? **??**
         // Is there a requested SLEV here?
         sprintf(shaftLevelInstance, "S%i-%c", GetInstance(),
                  *pSearchFloor);
         sfIter = requestedSlevMap.find(shaftLevelInstance);
         if (sfIter != requestedSlevMap.end()) {
            // The shaft level instance is there, so we'll determine if
            // a stop was requested or if a floor call was made.
            if (((ShaftLevel_Model *)(sfIter->second))->StopRequested()) {
               stopsOrCallsFound++;
               pTargetFloor = pSearchFloor;
               #ifdef SHOW_PONG_PROCESSING
               {
                  char str[80]; // ample
                  sprintf(str, "   stop request found (in Pong): %c",
                           *pSearchFloor);
                  RRTLog(str);
               }
               #endif // def SHOW_PONG_PROCESSING
            }
            if (((ShaftLevel_Model *)(sfIter->second))
                                                      ->Floors_requested(up)) {
               stopsOrCallsFound++;
               pTargetFloor = pSearchFloor;
               #ifdef SHOW_PONG_PROCESSING
               {
                  char str[80]; // ample
                  sprintf(str, "   up floor request found (in Pong): %c",
                           *pSearchFloor);
                  RRTLog(str);
               }
               #endif // def SHOW_PONG_PROCESSING
            }
            if (((ShaftLevel_Model *)(sfIter->second))
                                                    ->Floors_requested(down)) {
               stopsOrCallsFound++;
               pTargetFloor = pSearchFloor;
               #ifdef SHOW_PONG_PROCESSING
               {
                  char str[80]; // ample
                  sprintf(str, "   down floor request found (in Pong): %c",
                           *pSearchFloor);
                  RRTLog(str);
               }
               #endif // def SHOW_PONG_PROCESSING
            }
         }
      }
   }

   if (stopsOrCallsFound == 1) {
      // Get the floor closest to the cabin right now.
      char *pCabinFloor = strchr(programmedFloorLabels, currentFloor);

      if (pTargetFloor > pCabinFloor) {
         travelDirection = up;
      } else if (pTargetFloor < pCabinFloor) {
         travelDirection = down;
      } else { // pTargetFloor == pCabinFloor
         // We'll leave the travelDirection alone.
      }

      #ifdef SHOW_PONG_PROCESSING
      char *ptd;
      switch (travelDirection) {
         case up:   ptd = "up";   break;
         case down: ptd = "down"; break;
         case none: ptd = "none"; break;
         default:   ptd = "????";
      }
      char str[80]; // ample
      sprintf(str, "   $$$ Pong found one stop and set travelDirection: %s",
               ptd);
      RRTLog(str);
      #endif // def SHOW_PONG_PROCESSING
   }
}

void Cabin_Model::SetCurrentXfer(Transfer_Model *curXfer)
{
   currentXfer = curXfer;
}

void Cabin_Model::SetMyDoor(Door_Model* myDr)
{
   myDoor = myDr;
}

void Cabin_Model::SetMyShaft(Shaft_Model* myShft)
{
   myShaft = myShft;
}

void Cabin_Model::SetMyTransport(Transport_Model* myTrnsprt)
{
   myTransport = myTrnsprt;
}

void Cabin_Model::ToggleDir(void)
{
   travelDirection = (travelDirection == up) ? down : up;
   // **??** TBD Add this! **??**
   // UI::Set_travel_direction(Shaft_ID:self.Shaft,
   //                          Dir: ::dir_to_string(Dir:self.Travel_direction))
   //#define SHOW_TOGGLEDIR_PROCESSING
   #ifdef SHOW_TOGGLEDIR_PROCESSING
   {
      char *ptd;
      switch (travelDirection) {
         case up:   ptd = "up"; break;
         case down: ptd = "down"; break;
         case none: ptd = "none"; break;
         default:   ptd = "????";
      }
      char *pxtd;
      switch ((travelDirection == up) ? down : up) {
         case up:   pxtd = "up"; break;
         case down: pxtd = "down"; break;
         case none: pxtd = "none"; break;
         default:   pxtd = "????";
      }
      char str[80]; // ample
      sprintf(str, "   *** travelDirection (in ToggleDir): to %s from %s",
                                         ptd, pxtd);
      RRTLog(str);
   }
   #endif // def SHOW_TOGGLEDIR_PROCESSING
}

void Cabin_Model::UpdateLocation(char floor)
{
   // We'll relink this Cabin to its current SLEV position.
   currentFloor = floor;
   // **??** TBD Add this! **??**
   // UI::Cabin_at_floor(Shaft_ID:self.Shaft,Floor_name:new_SLEV.Floor)
}

void Comparator_Model::EVTModel_1300(void)
{
   // [1300]
   // START
}

void Comparator_Model::EVTModel_1301(void)
{
   // [1301]
   // DO_INITIALIZE
   // Initialize prior to obtaining initial state.
   for (int i = 1; i <= programmedNumberOfElevatorShafts; i++) {
      modeledElevatorStatus[i].floorLocation = programmedFloorLabels[
                                                   programmedGroundFloorLevel];
      modeledElevatorStatus[i].indicator     = none;
      modeledElevatorStatus[i].status        = doorClosed;

      elevatorStatus[i].floorLocation        = programmedFloorLabels[
                                                   programmedGroundFloorLevel];
      elevatorStatus[i].indicator            = none;
      elevatorStatus[i].status               = doorClosed;
   }
   alertIssued                                 = 1; // none
   commandsIssued[0]                         = '\0';
   points                                      = 0;
}

void Comparator_Model::EVTModel_1302(void)
{
   // [1302]
   // INITIALIZED
   // We'll broadcast a BEGIN_TEST to all components to begin the test.
   SendEvent(BEGIN_TEST, ' ', ' ', broadcast, this);
   nextTickTime = RRTGetTime();

   // This in not sent priority so it will be preceded by the BEGIN_TEST event.
   SendEvent(ONE_SECOND_TICK, ' ', ' ', this, this);
}

void Comparator_Model::EVTModel_1303(char floor, char car)
{
   // [1303]
   // CAR_LOCATION
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "CAR_LOCATION event has a bad car value: '%c' (0x%02x).",
               car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (strchr(programmedFloorLabels, floor) == NULL) {
      char str[80]; // ample
      sprintf(str, "CAR_LOCATION floor has a bad value: '%c' (0x%02x).",
               floor, floor);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the car's initial location.
      elevatorStatus[iCar].floorLocation = floor;
      if (++initialCarLocationEventCounter >= programmedNumberOfElevatorShafts) {
         // Set the model's initial car locations to match the target.
         // **??** TBD FIX ME **??**

         // We're now ready to proceed with the test.
         SendPriorityEvent(INITIALIZED);
      }
   }
}

void Comparator_Model::EVTModel_1304(char floor, char car)
{
   // [1304]
   // CAR_LOCATION
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "CAR_LOCATION event has a bad car value: '%c' (0x%02x).",
               car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (strchr(programmedFloorLabels, floor) == NULL) {
      char str[80]; // ample
      sprintf(str, "CAR_LOCATION floor has a bad value: '%c' (0x%02x).",
               floor, floor);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the car's updated location.
      elevatorStatus[iCar].floorLocation = floor;

      // Pass this car location report to the appropriate car location
      // comparison state machine to watch for a corresponding model car
      // location report.
      Id carLocModelId(typeid(CompareCarLocations_Model).name(), iCar);
      SendEventToId(CAR_LOCATION, floor, ' ', &carLocModelId, this);
   }
}

void Comparator_Model::EVTModel_1305(char floor, char car)
{
   // [1305]
   // STOP_REQUESTED

   // Add this stop request to the command list.
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%c%c%c", pushCarButton[0], floor, car);
   int commandsIssuedLength = strlen(commandsIssued);
   if ((commandsIssuedLength + strlen(pushCarButton) + 2)
        < MAX_NUMBER_OF_COMMAND_BYTES) {
      if (commandsIssuedLength) {
         strcat(commandsIssued, ", ");
      }
      strcat(commandsIssued, str);
   } else {
      char strx[80]; // ample
      sprintf(strx, "Not enough room in commandsIssued buffer for '%s'.",
               str);
      alert(strx);
      score += fatalDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   }
}

void Comparator_Model::EVTModel_1306(char floor, char car)
{
   // [1306]
   // FLOOR_CALL
}

void Comparator_Model::EVTModel_1307(void)
{
   // [1307]
   // ONE_SECOND_TICK
   nextTickTime += 1000; // ms
   unsigned int timeNow = RRTGetTime();
   unsigned int toWait = 0; // worst case if we've been long delayed
   if (nextTickTime > timeNow) {
      toWait = nextTickTime - timeNow;
   }
   SendDelayedEventToId(toWait, ONE_SECOND_TICK, ' ', ' ',
                         &comparatorModelId, this, &testing);

   // Give EVT_Show a consistent view of modeled status to display.
   for (int i = 1; i <= programmedNumberOfElevatorShafts; i++) {
      showElevator[i].floorLocation = elevatorStatus[i].floorLocation;
      showElevator[i].indicator     = elevatorStatus[i].indicator;
      showElevator[i].status        = elevatorStatus[i].status;
      showModel[i].floorLocation = modeledElevatorStatus[i].floorLocation;
      showModel[i].indicator     = modeledElevatorStatus[i].indicator;
      showModel[i].status        = modeledElevatorStatus[i].status;
   }
   showTime = RRTGetTime();
   showPoints = points;
   strcpy(showCommandsIssued, commandsIssued);
   showAlertIssued = alertIssued;
   SendEventToId(SHOW_IT, ' ', ' ', &evtShowModelId, this);

   // Include the accumulated points for this line in the score.
   score += points;

   // Reset for next time.
   alertIssued         = 1; // none
   commandsIssued[0] = '\0';
   points              = 0;
}

void Comparator_Model::EVTModel_1308(char floor, char car)
{
   // [1308]
   // MODELED_CAR_LOCATION
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "MODELED_CAR_LOCATION event has a bad car value: '%c' "
               "(0x%02x).", car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (strchr(programmedFloorLabels, floor) == NULL) {
      char str[80]; // ample
      sprintf(str, "MODELED_CAR_LOCATION floor has a bad value: '%c' "
               "(0x%02x).", floor, floor);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the modeled car's updated location.
      modeledElevatorStatus[iCar].floorLocation = floor;

      // Pass this car location report to the appropriate car location
      // comparison state machine to watch for a corresponding car
      // location report.
      Id carLocModelId(typeid(CompareCarLocations_Model).name(), iCar);
      SendEventToId(MODELED_CAR_LOCATION, floor, ' ', &carLocModelId, this);
   }
}

void Comparator_Model::EVTModel_1309(char doorState, char car)
{
   // [1309]
   // DOOR_STATUS
   doorStatus status;
   switch (doorState) {
      case ' ':
         status = doorLocked;
         break;

      case '|':
         status = doorClosed;
         break;

      case '-':
         status = doorAjar;
         break;

      case 'O':
         status = doorFullyOpen;
         break;

      default:
         status = doorUnknown;
   }
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "DOOR_STATUS event has a bad car value: '%c' (0x%02x).",
               car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (status == doorLocked) {
      // Capture the door's updated status.
      elevatorStatus[iCar].status = status;

      // Pass this door locked report to the appropriate door locked
      // comparison state machine to watch for a corresponding door locked
      // report. N.B.: there are programmedNumberOfElevatorShafts instances
      // of CompareDoorPositions_Model dedicated for doorLocked status because
      // two successive door status reports (doorClosed then doorLocked) can
      // occur almost immediately, before the first has had a chance for
      // a comparison.
      Id doorLockedModelId(typeid(CompareDoorPositions_Model).name(),
                            iCar + programmedNumberOfElevatorShafts);
      SendEventToId(DOOR_POSITION, status, ' ', &doorLockedModelId, this);
   } else if (status == doorUnknown) {
      char str[80]; // ample
      sprintf(str, "DOOR_STATUS event has a bad status: '%c' (0x%02x).",
               doorState, doorState);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the door's updated status.
      elevatorStatus[iCar].status = status;

      // Pass this door position report to the appropriate door position
      // comparison state machine to watch for a corresponding model door
      // position report.
      Id doorPosModelId(typeid(CompareDoorPositions_Model).name(), iCar);
      SendEventToId(DOOR_POSITION, status, ' ', &doorPosModelId, this);
   }
}

void Comparator_Model::EVTModel_1310(char doorState, char car)
{
   // [1310]
   // MODELED_DOOR_STATUS
   doorStatus status;
   switch (doorState) {
      case ' ':
         status = doorLocked;
         break;

      case '|':
         status = doorClosed;
         break;

      case '-':
         status = doorAjar;
         break;

      case 'O':
         status = doorFullyOpen;
         break;

      case '*':
         status = doorBlocked;
         break;

      case 'H':
         status = doorHeld;
         break;

      default:
         status = doorUnknown;
   }
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "MODELED_DOOR_STATUS event has a bad car value: "
                    "'%c' (0x%02x).", car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (status == doorLocked) {
      // Capture the door's updated status.
      modeledElevatorStatus[iCar].status = status;

      // Pass this door locked report to the appropriate door locked
      // comparison state machine to watch for a corresponding door locked
      // report. N.B.: there are programmedNumberOfElevatorShafts instances
      // of CompareDoorPositions_Model dedicated for doorLocked status because
      // two successive door status reports (doorClosed then doorLocked) can
      // occur almost immediately, before the first has had a chance for
      // a comparison.
      Id doorLockedModelId(typeid(CompareDoorPositions_Model).name(),
                            iCar + programmedNumberOfElevatorShafts);
      SendEventToId(MODELED_DOOR_POSITION, status, ' ',
                     &doorLockedModelId, this);
   } else if (status == doorUnknown) {
      char str[80]; // ample
      sprintf(str, "MODELED_DOOR_STATUS event has a bad status: '%c' (0x%02x).",
               doorState, doorState);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the door's updated status.
      modeledElevatorStatus[iCar].status = status;

      // Pass this door position report to the appropriate door position
      // comparison state machine to watch for a corresponding door
      // position report.
      Id doorPosModelId(typeid(CompareDoorPositions_Model).name(), iCar);
      SendEventToId(MODELED_DOOR_POSITION, status, ' ', &doorPosModelId, this);
   }
}

void Comparator_Model::EVTModel_1311(char dirChar, char car)
{
   // [1311]
   // DIR_INDICATOR
   direction dir;
   switch (dirChar) {
      case ' ':
         dir = none;
         break;

      case '^':
         dir = up;
         break;

      case 'v':
         dir = down;
         break;

      default:
         dir = dirUnknown;
   }
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "DIR_INDICATOR event has a bad car value: '%c' (0x%02x).",
               car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (dir == dirUnknown) {
      char str[80]; // ample
      sprintf(str, "DIR_INDICATOR event has a bad direction: '%c' (0x%02x).",
               dirChar, dirChar);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the shaft's updated direction indicator.
      elevatorStatus[iCar].indicator = dir;

      // Pass this shaft direction indicator to the appropriate shaft direction
      // comparison state machine to watch for a corresponding model shaft
      // direction indicator report.
      Id dirIndModelId(typeid(CompareDirIndicators_Model).name(), iCar);
      SendEventToId(DIRECTION_INDICATOR, dir, ' ', &dirIndModelId, this);
   }
}

void Comparator_Model::EVTModel_1312(char dirChar, char car)
{
   // [1312]
   // MODELED_DIR_INDICATOR
   direction dir;
   switch (dirChar) {
      case ' ':
         dir = none;
         break;

      case '^':
         dir = up;
         break;

      case 'v':
         dir = down;
         break;

      default:
         dir = dirUnknown;
   }
   int iCar = car - '0';
   if (iCar < 1 || iCar > programmedNumberOfElevatorShafts) {
      char str[80]; // ample
      sprintf(str, "MODELED_DIR_INDICATOR event has a bad car value: "
                    "'%c' (0x%02x).", car, car);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else if (dir == dirUnknown) {
      char str[80]; // ample
      sprintf(str, "MODELED_DIR_INDICATOR event has a bad direction: "
                    "'%c' (0x%02x).", dirChar, dirChar);
      alert(str);
      score += modelDiscrepancyFound;
      SendEvent(ABORT_TEST, ' ', ' ', broadcast, this);
   } else {
      // Capture the shaft's updated direction indicator.
      modeledElevatorStatus[iCar].indicator = dir;

      // Pass this shaft direction indicator to the appropriate shaft direction
      // comparison state machine to watch for a corresponding shaft
      // direction indicator report.
      Id dirIndModelId(typeid(CompareDirIndicators_Model).name(), iCar);
      SendEventToId(MODELED_DIRECTION_INDICATOR, dir, ' ', &dirIndModelId, this);
   }
}

void CompareCarLocations_Model::EVTModel_2100(void)
{
   // [2100]
   // START
   lastCarLocation      = blank; // invalid
   lastCarTime          = 0;     // invalid
   lastTimeDifferential = 0;
   // **??** TBD Use this for reporting alerts. **??**
   shaft                = GetInstance() + '0';
}

void CompareCarLocations_Model::EVTModel_2101(char floor, char unused)
{
   // [2101]
   // MODELED_CAR_LOCATION
   if (floor == lastCarLocation) {
      // The car location report from the elevator was early.  We've already
      // sent an alert due to the missing model car location, so we can just
      // ignore this reported model car location.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding car location isn't received from
      // the elevator simulation within the prescribed time interval.
      lastCarLocation = floor;
      lastCarTime     = RRTGetTime();
      SendDelayedEvent(compareCarLocationsTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareCarLocations_Model::EVTModel_2102(char floor, char unused)
{
   // [2102]
   // CAR_LOCATION
   if (floor == lastCarLocation) {
      // OK, the model and elevator agree on location. Account for any time
      // difference between the notifications.
      unsigned int timeDelta = RRTGetTime() - lastCarTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding location to the model's location.
      SendEventToId(ALERT, 'C', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareCarLocations_Model::EVTModel_2103(char floor, char unused)
{
   // [2103]
   // CAR_LOCATION
   if (floor == lastCarLocation) {
      // This car location report from the elevator is late.  We've already
      // sent an alert due to its late arrival, so we can just ignore this
      // reported location.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding car location isn't received from
      // the model within the prescribed time interval.
      lastCarLocation = floor;
      lastCarTime     = RRTGetTime();
      SendDelayedEvent(compareCarLocationsTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareCarLocations_Model::EVTModel_2104(char floor, char unused)
{
   // [2104]
   // MODELED_CAR_LOCATION
   if (floor == lastCarLocation) {
      // OK, the model and elevator agree on location.
      unsigned int timeDelta = RRTGetTime() - lastCarTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding location to the model's location.
      SendEventToId(ALERT, 'C', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareCarLocations_Model::EVTModel_2105(void)
{
   // [2105]
   // TO_READY
}

void CompareCarLocations_Model::EVTModel_2106(char floor, char unused)
{
   // [2106]
   // CAR_LOCATION
   // Elevator sent an extra location report (before the timeout occurred).
   SendEventToId(ALERT, 'C', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareCarLocations_Model::EVTModel_2107(void)
{
   // [2107]
   // TO_READY
}

void CompareCarLocations_Model::EVTModel_2108(void)
{
   // [2108]
   // TIMEOUT
   // Elevator didn't send a car location in time.
   SendEventToId(ALERT, 'C', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareCarLocations_Model::EVTModel_2109(void)
{
   // [2109]
   // TIMEOUT
   // We didn't receive a model car location in time.
   SendEventToId(ALERT, 'C', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDirIndicators_Model::EVTModel_2300(void)
{
   // [2300]
   // START
   lastDirIndicator     = '?';   // invalid
   lastDirTime          = 0;     // invalid
   lastTimeDifferential = 0;
   // **??** TBD Use this for reporting alerts. **??**
   shaft                = GetInstance() + '0';
}

void CompareDirIndicators_Model::EVTModel_2301(char dir, char unused)
{
   // [2301]
   // MODELED_DIRECTION_INDICATOR
   if (dir == lastDirIndicator) {
      // The dir indicator report from the elevator was early.  We've already
      // sent an alert due to the missing model dir indicator, so we can just
      // ignore this reported model dir indicator.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding dir indicator isn't received from
      // the elevator simulation within the prescribed time interval.
      lastDirIndicator = dir;
      lastDirTime      = RRTGetTime();
      SendDelayedEvent(compareDirIndicatorsTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareDirIndicators_Model::EVTModel_2302(char dir, char unused)
{
   // [2302]
   // DIRECTION_INDICATOR
   if (dir == lastDirIndicator) {
      // OK, the model and elevator agree on direction. Account for any time
      // difference between the notifications.
      unsigned int timeDelta = RRTGetTime() - lastDirTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding dir indicator to the model's
      // dir indicator.
      SendEventToId(ALERT, 'I', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareDirIndicators_Model::EVTModel_2303(char dir, char unused)
{
   // [2303]
   // DIRECTION_INDICATOR
   if (dir == lastDirIndicator) {
      // This dir indicator report from the elevator is late.  We've already
      // sent an alert due to its late arrival, so we can just ignore this
      // reported dir indicator.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding dir indicator isn't received from
      // the model within the prescribed time interval.
      lastDirIndicator = dir;
      lastDirTime      = RRTGetTime();
      SendDelayedEvent(compareDirIndicatorsTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareDirIndicators_Model::EVTModel_2304(char dir, char unused)
{
   // [2304]
   // MODELED_DIRECTION_INDICATOR
   if (dir == lastDirIndicator) {
      // OK, the model and elevator agree on direction.
      unsigned int timeDelta = RRTGetTime() - lastDirTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding dir indicator to the model's
      // dir indicator.
      SendEventToId(ALERT, 'I', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareDirIndicators_Model::EVTModel_2305(void)
{
   // [2305]
   // TO_READY
}

void CompareDirIndicators_Model::EVTModel_2306(char dir, char unused)
{
   // [2306]
   // DIRECTION_INDICATOR
   // Elevator sent an extra dir indicator report (before the timeout occurred).
   SendEventToId(ALERT, 'I', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDirIndicators_Model::EVTModel_2307(void)
{
   // [2307]
   // TO_READY
}

void CompareDirIndicators_Model::EVTModel_2308(void)
{
   // [2308]
   // TIMEOUT
   // Elevator didn't send a dir indicator in time.
   SendEventToId(ALERT, 'I', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDirIndicators_Model::EVTModel_2309(void)
{
   // [2309]
   // TIMEOUT
   // We didn't receive a dir indicator in time.
   SendEventToId(ALERT, 'I', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDoorPositions_Model::EVTModel_2200(void)
{
   // [2200]
   // START
   lastDoorPosition     = blank; // invalid
   lastDoorTime         = 0;     // invalid
   lastTimeDifferential = 0;
   // **??** TBD Use this for reporting alerts. **??**
   shaft                = GetInstance() + '0';
}

void CompareDoorPositions_Model::EVTModel_2201(char position, char unused)
{
   // [2201]
   // MODELED_DOOR_POSITION
   if (position == lastDoorPosition) {
      // The door position report from the elevator was early.  We've already
      // sent an alert due to the missing model door position, so we can just
      // ignore this reported model door position.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding door position isn't received from
      // the elevator simulation within the prescribed time interval.
      lastDoorPosition = position;
      lastDoorTime     = RRTGetTime();
      SendDelayedEvent(compareDoorStatusesTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareDoorPositions_Model::EVTModel_2202(char position, char unused)
{
   // [2202]
   // DOOR_POSITION
   if (position == lastDoorPosition) {
      // OK, the model and elevator agree on door position. Account for any time
      // difference between the notifications.
      unsigned int timeDelta = RRTGetTime() - lastDoorTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding door position to the model's
      // door position.
      SendEventToId(ALERT, 'D', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareDoorPositions_Model::EVTModel_2203(char position, char unused)
{
   // [2203]
   // DOOR_POSITION
   if (position == lastDoorPosition) {
      // This door position report from the elevator is late.  We've already
      // sent an alert due to its late arrival, so we can just ignore this
      // reported door position.
      SendPriorityEvent(TO_READY);
   } else {
      // We'll timeout if the corresponding door position isn't received from
      // the model within the prescribed time interval.
      lastDoorPosition = position;
      lastDoorTime     = RRTGetTime();
      SendDelayedEvent(compareDoorStatusesTimeout, TIMEOUT, ' ', ' ',
                        this, this, &testing);
   }
}

void CompareDoorPositions_Model::EVTModel_2204(char position, char unused)
{
   // [2204]
   // MODELED_DOOR_POSITION
   if (position == lastDoorPosition) {
      // OK, the model and elevator agree on door position.
      unsigned int timeDelta = RRTGetTime() - lastDoorTime;
      if (timeDelta > lastTimeDifferential) {
         points += timeDelta - lastTimeDifferential;
      } else if (timeDelta < lastTimeDifferential) {
         points += lastTimeDifferential - timeDelta;
      }
      lastTimeDifferential = timeDelta;
   } else {
      // Elevator didn't send a corresponding door position to the model's
      // door position.
      SendEventToId(ALERT, 'D', ' ', &evtShowModelId, this);
      points += majorDiscrepancyFound;
   }
}

void CompareDoorPositions_Model::EVTModel_2205(void)
{
   // [2205]
   // TO_READY
}

void CompareDoorPositions_Model::EVTModel_2206(char position, char unused)
{
   // [2206]
   // DOOR_POSITION
   // Elevator sent an extra door position report (before the timeout occurred).
   SendEventToId(ALERT, 'D', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDoorPositions_Model::EVTModel_2207(void)
{
   // [2207]
   // TO_READY
}

void CompareDoorPositions_Model::EVTModel_2208(void)
{
   // [2208]
   // TIMEOUT
   // Elevator didn't send a door position in time.
   SendEventToId(ALERT, 'D', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

void CompareDoorPositions_Model::EVTModel_2209(void)
{
   // [2209]
   // TIMEOUT
   // We didn't receive a door position location in time.
   SendEventToId(ALERT, 'D', ' ', &evtShowModelId, this);
   points += majorDiscrepancyFound;
}

////////////////////////////////////////////////////////////////////////////////
//
// ConstrainFloorLabels
//
// This utility causes the randomizer to use labels provided by the Elevator
// simulator when the simulator indicates that the building dimensions have
// already been set by the Elevator simulator's command line.
//
// The global boolean simulatorProvidedBuildingDimensions is used to determine
// when we adopt the entire simulator-provided floor labels string.
//
// Otherwise, we will adjust the length of the floor labels string to match
// the global programmedNumberOfFloors variable, which must have already been
// set.
//
// Similarly, we adjust the location of the ground floor (0-based) in the
// floor labels to match the global programmedGroundFloorLevel variable.
//
// The number of floor labels in the value string are set to the appropriate
// number based on the global programmedNumberOfFloors variable.
//
// N.B.: ConstrainFloorLabels must follow randomization of numberOfFloors,
//     numberOfElevators, and groundFloor variables.
//

void ConstrainFloorLabels(char *value, bool isRange, char *range)
{
   // The sizes of value and range are ample. The programmedNumberOfFloors must
   // already be set.
   if (simulatorProvidedBuildingDimensions) {
      strcpy(value, floorLabels);
   } else {
      if (strlen(floorLabels) == 0) {
         char str[SAFE_SIZE]; // ample
         for (int i = 0; i < programmedNumberOfFloors; i++) {
            if (i < programmedGroundFloorLevel) {
               str[i] = 'A' + (programmedGroundFloorLevel - i);
            } else if (i > programmedGroundFloorLevel) {
               str[i] = '1' + (i - programmedGroundFloorLevel);
            } else { // i == programmedGroundFloorLevel
               str[i] = 'L';
            }
            str[i + 1] = '\0'; // keep as a string
         }
         strcpy(value, str);
      } else {
         // We'll just make sure the size is OK.
         if (strlen(floorLabels) > programmedNumberOfFloors) {
            // Crop to fit.
            floorLabels[programmedNumberOfFloors] = '\0';
            strcpy(value, floorLabels);
         } else if (strlen(floorLabels) < programmedNumberOfFloors) {
            // Fill out.
            char str[SAFE_SIZE]; // ample
            char last = floorLabels[strlen(floorLabels) - 1];
            strcpy(str, floorLabels);
            for (int i = strlen(str), j = 0; i < programmedNumberOfFloors;
                  i++, j++) {
               str[i] = last + j;
            }
            strcpy(value, str);
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// ConstrainGroundFloorLevel
//
// This utility causes the randomizer to use a ground floor level provided by
// the Elevator simulator when the simulator indicates that the building
// dimensions have already been set by the Elevator simulator's command line.
//
// The global boolean simulatorProvidedBuildingDimensions is used to determine
// when we adopt the simulator-provided ground floor level value.
//
// Otherwise, we will adjust the randomization limits to conform to the
// global programmedNumberOfFloors variable, which must have already been
// set.  We set the ground floor to occur in the lower-half of the building.
//

void ConstrainGroundFloorLevel(char *value, bool isRange, char *range)
{
   // The sizes of value and range are ample. The programmedNumberOfFloors must
   // already be set.
   if (simulatorProvidedBuildingDimensions) {
      sprintf(value, "%i", groundFloor);
   } else  {
      if (atoi(value) > programmedNumberOfFloors - 1) {
         sprintf(value, "%i", programmedNumberOfFloors - 1);
      }

      int low  = 0;
      int high = (programmedNumberOfFloors - 1) / 2; // lower half
      int res  = 1;
      sprintf(range, "%i,%i,%i", low, high, res);
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// ConstrainNumberOfFloors
//
// This utility causes the randomizer to use the number of floors provided by
// the Elevator simulator when the simulator indicates that the building
// dimensions have already been set by the Elevator simulator's command line.
//
// The global boolean simulatorProvidedBuildingDimensions is used to determine
// when we adopt the simulator-provided number of floors value.
//
// Otherwise, we will adjust the high randomization limit to conform to the
// global numberOfFloors variable, which must have already been set based on
// the Elevator-provided maximum number of floors that can be displayed in its
// current Terminal window.
//

void ConstrainNumberOfFloors(char *value, bool isRange, char *range)
{
   // The sizes of value and range are ample.
   if (simulatorProvidedBuildingDimensions) {
      sprintf(value, "%i", numberOfFloors);
   } else {
      // Restricting the number of floors to Elevator's limit.
      if (atoi(value) > numberOfFloors) {
         sprintf(value, "%i", numberOfFloors);
      }

      int low  = 2;
      int high = numberOfFloors;
      int res  = 1;
      sprintf(range, "%i,%i,%i", low, high, res);
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// ConstrainNumberOfShafts
//
// This utility causes the randomizer to use the number of shafts provided by
// the Elevator simulator when the simulator indicates that the building
// dimensions have already been set by the Elevator simulator's command line.
//
// The global boolean simulatorProvidedBuildingDimensions is used to determine
// when we adopt the simulator-provided number of shafts value.
//
// Otherwise, we will adjust the high randomization limit to conform to the
// global numberOfElevators variable, which must have already been set based on
// the Elevator-provided maximum number of shafts that can be displayed in its
// current Terminal window.
//

void ConstrainNumberOfShafts(char *value, bool isRange, char *range)
{
   if (simulatorProvidedBuildingDimensions) {
      sprintf(value, "%i", numberOfElevators);
   } else {
      // Restricting the number of shafts to Elevator's limit.
      if (atoi(value) > numberOfElevators) {
         sprintf(value, "%i", numberOfElevators);
      }

      int low  = 1;
      int high = numberOfElevators;
      int res  = 1;
      sprintf(range, "%i,%i,%i", low, high, res);
   }
}

void Door_Model::EVTModel_1500(void)
{
   // [1500]
   // START
   doorClosed    = true;
   retries       = programmedMaxCloseAttempts;
   openWaitTime  = programmedNormalDoorWaitTime;
   lockRequested = false;
   held          = false;
   emergencyHold = false;
}

void Door_Model::EVTModel_1501(void)
{
   // [1501]
   // UNLOCK
   // The cabin has either just arrived at a floor or the cabin was called at
   // its current location. We'll unlock to become alert to passengers' door
   // open / close requests. The door close retry counter is initialized.
   // The door open wait time is initialized to its normal value.
   retries = programmedMaxCloseAttempts;
   openWaitTime  = programmedNormalDoorWaitTime;
   SendPriorityEvent(OPEN);

   // Send a doorIsClosed (i.e., door is "unlocked") status msg to the
   // pipe trace: <"%"><"|"><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsClosed);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }

   // "Turn on" the corresponding direction indicator ("above the door")
   // by sending a dir indicator msg to the pipe trace: <"~"><dir><shaft>.
   // If the cabin has reached the top or bottom floor, reverse the
   // travel direction.
   direction dir = myCabin->GetTravelDirection();
   char dirChar;
   char currentFloor = myCabin->GetCurrentFloor();
   char *pCurrentFloorLabel = strchr(programmedFloorLabels, currentFloor);
   if (pCurrentFloorLabel != NULL) {
      int floorIndex = pCurrentFloorLabel - programmedFloorLabels;
      if (dir == up && floorIndex == (programmedNumberOfFloors - 1)) {
         dir = down;
      } else if (dir == down && floorIndex == 0) {
         dir = up;
      }
   } else {
      alert("NULL pCurrentFloorLabel in EVTModel_1501.");
      score += modelDiscrepancyFound;
      testing = false;
   }
   switch (dir) {
      case none:
         dirChar = ' ';
         break;

      case up:
         dirChar = '^';
         break;

      case down:
         dirChar = 'v';
         break;

      default:
         dirChar = '?';
   }
   strcpy(msg, indicatorIsUp); // surrogate
   msg[1] = dirChar;
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

void Door_Model::EVTModel_1502(void)
{
   // [1502]
   // LOCK
   // Tell the cabin that we're ready to go.
   lockRequested = false; // must be done here, not in UNLOCK.
   #ifdef SHOW_CABIN_PROCESSING
   {
      char str[80]; // ample
      sprintf( str, "   $$$ sending DOORS_SECURE in Door_Model::EVTModel_1502" );
      RRTLog(str);
   }
   #endif // def SHOW_CABIN_PROCESSING
   SendEvent(DOORS_SECURE, ' ', ' ', myCabin, this);

   // Send a doorIsLocked status msg to the pipe trace: <"%"><" "><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsLocked);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

void Door_Model::EVTModel_1503(void)
{
   // [1503]
   // UNLOCK
   EVTModel_1501();
}

void Door_Model::EVTModel_1504(void)
{
   // [1504]
   // OPEN
   // If there hasn't been an excessive number of door close attempts due to
   // an obstacle, open the doors.
   if (retries-- > 0) {
      doorClosed = false;
      SendEvent(OPEN_DOOR, ' ', ' ', mySignalIo, this);
//      UI::Door_opening(Shaft_ID:self.Shaft); // for animation
   } else {
      // Give up!
// **??** TBD     SendPriorityEvent(CANT_CLOSE_DOOR);
   }
}

void Door_Model::EVTModel_1505(void)
{
   // [1505]
   // DOOR_IS_OPEN
   // Keep the doors open for whatever time period has been set into the
   // doors openWaitTime.
   SendDelayedEvent(openWaitTime, CLOSE, ' ', ' ', this, this, &testing);

   // Send a doorIsOpen status msg to the pipe trace: <"%"><"O"><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsOpen);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

void Door_Model::EVTModel_1506(void)
{
   // [1506]
   // CLOSE
   SendEvent(CLOSE_DOOR, ' ', ' ', mySignalIo, this);
}

void Door_Model::EVTModel_1507(void)
{
   // [1507]
   // DOOR_IS_CLOSED
   doorClosed = true;
   retries = 0;
   if (lockRequested) {
      SendPriorityEvent(LOCK);
   }
   Transfer_Model *tm = myCabin->GetCurrentXfer();
   if (tm != 0) {
      if (tm->GetCallInProgress()) {
         SendEvent(DOOR_CLOSED, ' ', ' ', tm, this);
      }
   }

   // Send a doorIsClosed status msg to the pipe trace: <"%"><"|"><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsClosed);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }

   // "Turn off" the direction indicator ("above the door") by sending
   // an indicatorIsOff status msg to the pipe trace: <"~"><" "><shaft>.
   strcpy(msg, indicatorIsOff);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

void Door_Model::EVTModel_1508(void)
{
   // [1508]
   // DOOR_IS_HALF_OPEN

   // Send a doorIsAjar status msg to the pipe trace: <"%"><"-"><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsAjar);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

void Door_Model::EVTModel_1509(void)
{
   // [1509]
   // DOOR_IS_HALF_CLOSED

   // Send a doorIsAjar status msg to the pipe trace: <"%"><"-"><shaft>.
   char msg[SAFE_SIZE];
   strcpy(msg, doorIsAjar);
   msg[2] = GetInstance() + '0';
   if (!ModelInfo(msg)) {
      testing = false;
   }
}

bool Door_Model::IsDoorClosed(void)
{
   return doorClosed;
}

void Door_Model::SetLockRequested(bool lckRequstd)
{
   lockRequested = lckRequstd;
}

void Door_Model::SetMyCabin(Cabin_Model* myCbn)
{
   myCabin = myCbn;
}

void Door_Model::SetMySignalIo(Signal_IO_Model *mySgnlIo)
{
   mySignalIo = mySgnlIo;
}

void EVT_Configuration_Model::EVTModel_1000(void)
{
   // [1000]
   // START
   ElevatorMessage("Requesting max bldg dimensions",
                    write(commandPipeFd, queryMaxDimensions,
                           strlen(queryMaxDimensions)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent((char *)queryMaxDimensions) != SUCCESS) {
      // Unable to log our queryMaxDimensions message.
      char str[80]; // ample
      sprintf(str, "Unable to log queryMaxDimensions message: '%s'.",
               queryMaxDimensions);
      alert(str);
   }
}

void EVT_Configuration_Model::EVTModel_1001(char parm1, char parm2)
{
   // [1001]
   // MAX_BLDG_DIMS_RECEIVED
   numberOfFloors    = parm1 >> 4;   // in high-order nibble
   numberOfElevators = parm1 & 0x0F; // in low-order nibble
   if (parm2 == '*') {
      simulatorProvidedBuildingDimensions = false;

      // We will randomize these.
      groundFloor                         = -1;
      floorLabels[0]                      = '\0';
      SendPriorityEvent(FLOOR_LABELS_RECEIVED);
   } else {
      simulatorProvidedBuildingDimensions = true;
      groundFloor                         = parm2;

      // We'll ask the Elevator for its floor labels.
      ElevatorMessage("Requesting floor labels",
                       write(commandPipeFd, queryFloorLabels,
                              strlen(queryFloorLabels)));

      // Put the transaction we're sending in our pipe trace.
      if (pt.MessageSent((char *)queryFloorLabels) != SUCCESS) {
         // Unable to log our queryFloorLabels message.
         char str[80]; // ample
         sprintf(str, "Unable to log queryFloorLabels message: '%s'.",
                  queryFloorLabels);
         alert(str);
      }
   }
}

void EVT_Configuration_Model::EVTModel_1002(void)
{
   // [1002]
   // FLOOR_LABELS_RECEIVED
   if (!RandomizeEcps(&elevatorEcps[0])) {
      char str[80]; // ample
      sprintf(str, "Error occurred in RandomizeEcps routine. Aborting...");
      cout << str << endl;
   }
   if (simulatorProvidedBuildingDimensions) {
      SendPriorityEvent(DONE);
   } else {
      SendPriorityEvent(SET_FLOOR_COUNT);
   }
}

void EVT_Configuration_Model::EVTModel_1003(void)
{
   // [1003]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setDoorOpenCloseTime, programmedDoorOpenCloseTime);
   ElevatorMessage("Setting door open / close time",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setDoorOpenCloseTime message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setDoorOpenCloseTime message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1004(void)
{
   // [1004]
   // SET_FLOOR_COUNT
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setNumberOfFloors, programmedNumberOfFloors);
   ElevatorMessage("Setting floor count",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setNumberOfFloors message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setNumberOfFloors message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1005(void)
{
   // [1005]
   // SET_SHAFT_COUNT
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setNumberOfElevators,
            programmedNumberOfElevatorShafts);
   ElevatorMessage("Setting shaft count",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setNumberOfElevators message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setNumberOfElevators message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1006(void)
{
   // [1006]
   // SET_FLOOR_LABELS
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%s", setFloorLabels, programmedFloorLabels);
   ElevatorMessage("Setting floor labels",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setFloorLabels message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setFloorLabels message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1007(void)
{
   // [1007]
   // SET_GROUND_FLOOR
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setGroundFloorLevel, programmedGroundFloorLevel);
   ElevatorMessage("Setting ground floor",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setGroundFloorLevel message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setGroundFloorLevel message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1008(void)
{
   // [1008]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%f", setFloorHeight, programmedFloorHeight);
   ElevatorMessage("Setting floor height",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setFloorHeight message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setFloorHeight message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1009(void)
{
   // [1009]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%f", setMaxCabinVelocity, programmedMaximumCabinVelocity);
   ElevatorMessage("Setting max cabin velocity",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setMaxCabinVelocity message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setMaxCabinVelocity message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1010(void)
{
   // [1010]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setMaxCloseAttempts, programmedMaxCloseAttempts);
   ElevatorMessage("Setting max close attempts",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setMaxCloseAttempts message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setMaxCloseAttempts message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1011(void)
{
   // [1011]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setNormalDoorWaitTime, programmedNormalDoorWaitTime);
   ElevatorMessage("Setting normal door wait time",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setNormalDoorWaitTime message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setNormalDoorWaitTime message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1012(void)
{
   // [1012]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%f", setMinStoppingDistance,
            programmedMinimumStoppingDistance);
   ElevatorMessage("Setting min stopping distance",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setMinStoppingDistance message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setMinStoppingDistance message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1013(void)
{
   // [1013]
   // DONE
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%s%i", setBlockClearTime, programmedBlockClearTime);
   ElevatorMessage("Setting block clear time",
                    write(commandPipeFd, str, strlen(str)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our setBlockClearTime message.
      char strx[80]; // ample
      sprintf(strx, "Unable to log setBlockClearTime message: '%s'.",
               str);
      alert(strx);
   }
}

void EVT_Configuration_Model::EVTModel_1014(void)
{
   // [1014]
   // DONE
   ElevatorMessage("Ending configuration phase",
                    write(commandPipeFd, endConfiguration,
                           strlen(endConfiguration)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent((char *)endConfiguration) != SUCCESS) {
      // Unable to log our endConfiguration message.
      char str[80]; // ample
      sprintf(str, "Unable to log endConfiguration message: '%s'.",
               endConfiguration);
      alert(str);
   }
}

void EVT_Configuration_Model::EVTModel_1015(void)
{
   // [1015]
   // BEGIN_TEST
}

void EVT_Configuration_Model::EVTModel_1016(void)
{
   // [1016]
   // END_TEST
   testing = false;
}

void EVT_Configuration_Model::EVTModel_1017(void)
{
   // [1017]
   // ABORT_TEST
   testing = false;
}

void EVT_Configuration_Model::EVTModel_1018(void)
{
   // [1018]
   // QUIT_TEST
   testing = false;
}

void EVT_Show_Model::EVTModel_1100(void)
{
   // [1100]
   // START
}

void EVT_Show_Model::EVTModel_1101(void)
{
   // [1101]
   // BEGIN_TEST
   InitializeShowEvt(programmedNumberOfElevatorShafts);
}

void EVT_Show_Model::EVTModel_1102(void)
{
   // [1102]
   // SHOW_IT
   DoShowEvt(showModel, showElevator, showTime, showCommandsIssued,
              showAlertIssued, showPoints, quiescent);
}

void EVT_Show_Model::EVTModel_1103(void)
{
   // [1103]
   // END_TEST
   // Output ALERT messages to console and log.
   OutputAlertMessages();

   // Output the score to console and log.
   char str[20]; // ample
   sprintf(str, ">>> Score: %u.", score);
   cout << endl << str << endl;
   RRTLog(str);
}

void EVT_Show_Model::EVTModel_1104(char alert, char notUsed)
{
   // [1104]
   // ALERT
   AddAlertMessage(alert);
}

void EVT_Show_Model::EVTModel_1105(void)
{
   // [1105]
   // QUIESCE_TEST
   quiescent = true;
}

////////////////////////////////////////////////////////////////////////////////
//
// PopulateEVT
//
// This routine provides initial populations of model instances.
//
// An error message is constructed and sent to the console and to RRTLog if the
// population was not successful.
//
// N.B.: the currentNumberOfModelInstances should be 1 after
//     StartEVT_Configuration() ran previously.
//

void  PopulateEVT(void)
{
   FSM *temp = 0;
   FSM *temp2 = 0;
   FSM *temp3 = 0;
   FSM *temp4 = 0;
   FSM *temp5 = 0;
   Cabin_Model *cabins[programmedNumberOfElevatorShafts];

   // Initialize the remaining state machine instances for the test. N.B.: these
   // first three default to instance = 1;
   bool brc;
   brc = AddModelInstance(new Stim_CarButtons_Model);
   brc &= AddModelInstance(new Comparator_Model);
   brc &= AddModelInstance(new EVT_Show_Model);

   // Add the Shaft, Cabin, Door, Transport, and Signal_IO instances.
   for (int i = 1; brc && i <= programmedNumberOfElevatorShafts; i++) {
      brc &= AddModelInstance(new CompareCarLocations_Model(i));
      brc &= AddModelInstance(new CompareDoorPositions_Model(i));
      brc &= AddModelInstance(new CompareDoorPositions_Model(i
              + programmedNumberOfElevatorShafts)); // for door locked reports
      brc &= AddModelInstance(new CompareDirIndicators_Model(i));
      temp = new Cabin_Model(i);
      brc &= AddModelInstance(temp);
      cabins[i - 1] = (Cabin_Model *)temp;
      temp2 = new Shaft_Model(i);
      brc &= AddModelInstance(temp2);
      temp3 = new Transport_Model(i);
      brc &= AddModelInstance(temp3);
      temp4 = new Door_Model(i);
      brc &= AddModelInstance(temp4);
      temp5 = new Signal_IO_Model(i);
      brc &= AddModelInstance(temp5);
      if (brc) {
         // Relate Cabin to Shaft across R2.
         ((Shaft_Model *)temp2)->SetMyCabin((Cabin_Model *)temp);
         ((Cabin_Model *)temp)->SetMyShaft((Shaft_Model *)temp2);

         // Relate Cabin to Door across R4.
         ((Door_Model *)temp4)->SetMyCabin((Cabin_Model *)temp);
         ((Cabin_Model *)temp)->SetMyDoor((Door_Model *)temp4);

         // Relate Cabin to Transport across R900.
         ((Transport_Model *)temp3)->SetMyCabin((Cabin_Model *)temp);
         ((Cabin_Model *)temp)->SetMyTransport((Transport_Model *)temp3);

         // Relate Signal_IO to Door across R901.
         ((Door_Model *)temp4)->SetMySignalIo((Signal_IO_Model *)temp5);
         ((Signal_IO_Model *)temp5)->SetMyDoor((Door_Model *)temp4);
      }
   }

   // Add the Shaft Level instances (with appropriate myCabin attributes).
   for (int i = 1; brc && i <= programmedNumberOfElevatorShafts; i++) {
      for (int j = 0; brc && j < programmedNumberOfFloors; j++) {
         temp = new ShaftLevel_Model((i - 1)
                                   * programmedNumberOfElevatorShafts + j + 1);
         ((ShaftLevel_Model *)temp)->Init(programmedFloorLabels[j], i);
         ((ShaftLevel_Model *)temp)->SetMyCabin(cabins[i - 1]);
         brc = AddModelInstance(temp);
         char str[SAFE_SIZE]; // ample
         sprintf(str, "S%i-%c", i, programmedFloorLabels[j]);
         nonRequestedSlevMap[str] = temp;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// RandomizeEcps
//
// This routine provides repeatable random values for the input elevator
// configuration parameters.
//
// The routine builds a map of ECP names with randomized pre-programmed values.
// The post-programming values are added to the map later.
//
// An error message is constructed and sent to the console and to RRTLog if the
// randomization was not successful.
//

bool RandomizeEcps(ecps theEcps[])
{
   bool brc = true; // assume success
   bool moreEcps = true;
   for (int i = 0; moreEcps; i++) {
      // Give other coroutines a chance.
      coresume();
      #if 0
      {
          char str[80]; // ample
          sprintf(str, "ECP Name: '%s'", theEcps[i].ecpName.c_str());
          RRTLog(str);
          cout << str << endl;
      }
      #endif // 0
      if (theEcps[i].ecpName == "") {
         moreEcps = false;
      } else {
         const int MAX_TEMP_RANGE_SIZE = 200; // chars
         char tempRange[MAX_TEMP_RANGE_SIZE]; // ample
         strcpy(tempRange, theEcps[i].range);
         char tempValue[SAFE_SIZE];
         strcpy(tempValue, theEcps[i].value.c_str());
         char *temp = tempValue;

         // Possibly alter the tempValue or tempRange strings.
         if (theEcps[i].constraint) {
            (theEcps[i].constraint)(tempValue, theEcps[i].isRange, tempRange);
         }

         ecpValuesType valueObject;

         valueObject.units = theEcps[i].units;
         valueObject.command = theEcps[i].command;
         valueObject.valType = theEcps[i].valType;
         if (valueObject.valType == "Float") {
            // If not already specified, pick a random value.
            double tempDouble;
            if (strlen(temp) == 0) {
               if (theEcps[i].isRange) {
                  // There should be two or three values: min,max[,resolution].
                  char *minVal = strtok(tempRange, ","); // minimum value
                  if (minVal == NULL) {
                     char str[80]; // ample
                     sprintf(str, "NULL minVal for %s in RandomizeEcps.",
                              theEcps[i].ecpName.c_str());
                     alert(str);
                     score += configDiscrepancyFound;
                     testing = false;
                     brc = false;
                  }
                  char *maxVal = strtok(NULL, ",");      // maximum value
                  if (maxVal == NULL) {
                     char str[80]; // ample
                     sprintf(str, "NULL maxVal for %s in RandomizeEcps.",
                              theEcps[i].ecpName.c_str());
                     alert(str);
                     score += configDiscrepancyFound;
                     testing = false;
                     brc = false;
                  }
                  char *resolution = strtok(NULL, ",");
                  if (resolution == NULL) {
                     resolution = const_cast <char *>("1.0"); // default if not specified
                  }

                  // Convert the strings to doubles.
                  double start = atof(minVal);
                  double end   = atof(maxVal);
                  double step  = atof(resolution);

                  // Calculate the number of steps possible.
                  int k = int(0.5 + (end - start) / step);

                  // Check (rounded) end value for consistency.
                  char str1[32]; // ample
                  char str2[32]; // ample
                  sprintf(str1, "%.4f", end);
                  sprintf(str2, "%.4f", start + k * step);
                  if (strcmp(str1, str2)) {
                     char str[80]; // ample
                     //sprintf(str,"%s has an inconsistent end value [%i,%i,%i].",
                     sprintf(str,"%s has an inconsistent end value [%.4f,%.4f,%.4f].",
                             theEcps[i].ecpName.c_str(), start, end, step);

                     // We'll pick the start value.
                     tempDouble = start;
                  } else {
                     tempDouble = rrs.GetFloatValue(theEcps[i].randItem,
                                                     float(start),
                                                     float(end),
                                                     float(step));
                  }
               } else {
                  // Determine how many things there are to pick between.
                  char *pThing = tempRange;
                  char *pDest = pThing; // non-NULL
                  int thingCount;
                  for (thingCount = 0; pDest != NULL; thingCount++) {
                     pDest = strchr(pThing, ',');
                     pThing = pDest + 1; // stepping past the one found
                  }
                  // Choose a (repeatable) random instance, uniformly.
                  int chosenIndex = rrs.GetIntValue(theEcps[i].randItem,
                                                     1, thingCount);

                  // Pick off the chosen value.
                  temp = strtok(tempRange, ","); // 1st value
                  for (int k = 1; k < chosenIndex; k++) {
                     temp = strtok(NULL, ",");
                  }
                  tempDouble = atof(temp);
               }
            } else {
               // Use the value provided.
               tempDouble = atof(temp);
            }
            *(double *)(theEcps[i].programmedVariable) = tempDouble;
            valueObject.before.dValue = tempDouble;
         } else if (valueObject.valType == "Int") {
            // If not already specified, pick a random value.
            int tempInt;
            if (strlen(temp) == 0) {
               if (theEcps[i].isRange) {
                  // There should be two or three values: min,max[,resolution].
                  char *minVal = strtok(tempRange, ","); // minimum value
                  if (minVal == NULL) {
                     char str[80]; // ample
                     sprintf(str, "NULL minVal for %s in RandomizeEcps.",
                              theEcps[i].ecpName.c_str());
                     alert(str);
                     score += configDiscrepancyFound;
                     testing = false;
                     brc = false;
                  }
                  char *maxVal = strtok(NULL, ",");      // maximum value
                  if (maxVal == NULL) {
                     char str[80]; // ample
                     sprintf(str, "NULL maxVal for %s in RandomizeEcps.",
                              theEcps[i].ecpName.c_str());
                     alert(str);
                     score += configDiscrepancyFound;
                     testing = false;
                     brc = false;
                  }
                  char *resolution = strtok(NULL, ",");
                  if (resolution == NULL) {
                     resolution = const_cast <char *>("1"); // default if not specified
                  }

                  // Convert the strings to integers.
                  int start = atoi(minVal);
                  int end   = atoi(maxVal);
                  int step  = atoi(resolution);

                  // Calculate the number of steps possible.
                  int k = int(0.5 + (end - start) / double(step));

                  // Check end value for consistency.
                  if (end != start + k * step) {
                     char str[80]; // ample
                     sprintf(str,"%s has an inconsistent end value [%i,%i,%i].",
                             theEcps[i].ecpName.c_str(), start, end, step);

                     // We'll pick the start value.
                     tempInt = start;
                  } else {
                     tempInt = rrs.GetIntValue(theEcps[i].randItem,
                                                start, end, step);
                  }
               } else {
                  // Determine how many things there are to pick between.
                  char *pThing = tempRange;
                  char *pDest = pThing; // non-NULL
                  int thingCount;
                  for (thingCount = 0; pDest != NULL; thingCount++) {
                     pDest = strchr(pThing, ',');
                     pThing = pDest + 1; // stepping past the one found
                  }
                  // Choose a (repeatable) random instance, uniformly.
                  int chosenIndex = rrs.GetIntValue(theEcps[i].randItem,
                                                     1, thingCount);

                  // Pick off the chosen value.
                  temp = strtok(tempRange, ","); // 1st value
                  for (int k = 1; k < chosenIndex; k++) {
                     temp = strtok(NULL, ",");
                  }
                  tempInt = atoi(temp);
               }
            } else {
               // Use the value provided.
               tempInt = atoi(temp);
            }
            *(int *)(theEcps[i].programmedVariable) = tempInt;
            valueObject.before.iValue = tempInt;
         } else if (valueObject.valType == "Enum") {
            // If not already specified, pick a random value.
            if (strlen(temp) == 0) {
               // Determine how many things there are to pick between.
               char *pThing = tempRange;
               char *pDest = pThing; // non-NULL
               int thingCount;
               for (thingCount = 0; pDest != NULL; thingCount++) {
                  pDest = strchr(pThing, ',');
                  pThing = pDest + 1; // stepping past the one found
               }

               // Choose a (repeatable) random instance, uniformly.
               int chosenIndex = rrs.GetIntValue(theEcps[i].randItem,
                                                  1, thingCount);
               // Pick off the chosen value.
               temp = strtok(tempRange, ","); // 1st value
               for (int k = 1; k < chosenIndex; k++) {
                  temp = strtok(NULL, ",");
               }

               // Save the randomized value.
               strcpy((char *)(theEcps[i].programmedVariable), temp);
            } else {
               // Use the value provided.
               strcpy((char *)(theEcps[i].programmedVariable), temp);
            }

            strcpy(valueObject.before.cValue, temp);
         } else {
            char str[80]; // ample
            sprintf(str, "Unexpected valType: %s, in RandomizeEcps.",
                     valueObject.valType.c_str());
            alert(str);
            score += configDiscrepancyFound;
            testing = false;
            brc = false;
         }

         if (testing) {
            // Add this item to our map.
            ecpMap[theEcps[i].ecpName] = valueObject;

            // Log the static requirements.
            if (theEcps[i].requirement1) {
               _Rs(theEcps[i].requirement1);
            }
            if (theEcps[i].requirement2) {
               _Rs(theEcps[i].requirement2);
            }
         }
      }
   }

   // Log the randomized elevator configuration parameters requested for
   // this test run.
   char str[80]; // ample
   for (seIter = ecpMap.begin(); seIter != ecpMap.end(); seIter++) {
      if ((seIter -> second).valType == "Enum") {
         sprintf(str, "  %s: %s %s", (seIter -> first).c_str(),
                  (seIter -> second).before.cValue,
                  (seIter -> second).units.c_str());
      } else if ((seIter -> second).valType == "Int") {
         sprintf(str, "  %s: %i %s", (seIter -> first).c_str(),
                  (seIter -> second).before.iValue,
                  (seIter -> second).units.c_str());
      } else { // (seIter -> second).valType == "Float"
         sprintf(str, "  %s: %.2f %s", (seIter -> first).c_str(),
                  (seIter -> second).before.dValue,
                  (seIter -> second).units.c_str());
      }
      cout << str << endl;
      RRTLog(str);
   }

   return brc;
}

void Shaft_Model::EVTModel_1600(void)
{
   // [1600]
   // START
   inService = true;
}

void Shaft_Model::EVTModel_1601(void)
{
   // [1601]
   // SERVICE_REQUESTED
   bool callInProgress = false;
   if (myCabin->GetCurrentXfer()) {
      callInProgress = myCabin->GetCurrentXfer()->GetCallInProgress();
      //#define SHOW_SHAFT_PROCESSING
      #ifdef SHOW_SHAFT_PROCESSING
      {
         char str[80]; // ample
         sprintf(str, "   $$$ callInProgress (in EVTModel_1601): %s",
                                            callInProgress ? "true" : "false");
         RRTLog(str);
      }
      #endif // def SHOW_SHAFT_PROCESSING
   }
   char destination = ' ';
   direction searchDirection = none;
   bool searchingBehind = false;
   if (myCabin->Ping(&destination, &searchDirection, searchingBehind)) {
      #ifdef SHOW_SHAFT_PROCESSING
      {
         char str[100]; // ample
         sprintf(str, "   $$$ Ping returned true (in EVTModel_1601) with "
                  "destination %c and searchDirection %s.",
                  destination, searchDirection == up ? "up" :
                  (searchDirection  == down ? "down" : "????"));
         RRTLog(str);
      }
      #endif // def SHOW_SHAFT_PROCESSING
      SendPriorityEvent(DEST_SELECTED, destination, searchDirection);
   } else {
      #ifdef SHOW_SHAFT_PROCESSING
      {
         char str[80]; // ample
         sprintf(str, "   $$$ Ping returned false (in EVTModel_1601)");
         RRTLog(str);
      }
      #endif // def SHOW_SHAFT_PROCESSING
      if (callInProgress) {
         SendPriorityEvent(NO_DEST);
      } else {
         searchingBehind = true;
         if (myCabin->Ping(&destination, &searchDirection, searchingBehind)) {
            #ifdef SHOW_SHAFT_PROCESSING
            {
               char str[100]; // ample
               sprintf(str, "   $$$ inner Ping returned true (in EVTModel_1601)"
                             " with destination %c and searchDirection %s.",
                        destination, searchDirection == up ? "up" :
                           (searchDirection  == down ? "down" : "????"));
               RRTLog(str);
            }
            #endif // def SHOW_SHAFT_PROCESSING
            SendPriorityEvent(DEST_SELECTED, destination, searchDirection);
         } else {
            #ifdef SHOW_SHAFT_PROCESSING
            {
               char str[80]; // ample
               sprintf(str, "   $$$ inner Ping returned false "
                             "(in EVTModel_1601)");
               RRTLog(str);
            }
            #endif // def SHOW_SHAFT_PROCESSING
            SendPriorityEvent(NO_DEST);
         }
      }
   }
}

void Shaft_Model::EVTModel_1602(void)
{
   // [1602]
   // NO_DEST
   if (!inService) {
      SendPriorityEvent(TAKE_OUT_OF_SERVICE);
   }
}

void Shaft_Model::EVTModel_1603(void)
{
   // [1603]
   // TRANSFER_COMPLETED
   EVTModel_1601();
}

void Shaft_Model::EVTModel_1604(void)
{
   // [1604]
   // XFER_UPDATE_REQUESTED
   EVTModel_1602();
}

void Shaft_Model::EVTModel_1605(char floor, char dir)
{
   // [1605]
   // SET_NEW_DEST
   if (myCabin->GetCurrentXfer() == 0) {
      // There isn't an existing transfer, so we'll create one.
      Transfer_Model *pTransferInstance = new Transfer_Model(++transferInstance);
      bool brc = AddModelInstance(pTransferInstance);
      if (!brc || pTransferInstance == 0) {
         char str[80]; // ample
         sprintf(str, "Unable to add Transfer instance (%i) to model.",
                  transferInstance);
         alert(str);
         score += modelDiscrepancyFound;
         testing = false;
         return;
      } else {
         myCabin->SetCurrentXfer(pTransferInstance);
         // Set Transfer instance's dest, shaft, cabin.
         myCabin->GetCurrentXfer()->Init(floor, GetInstance(), myCabin);
         SendEvent(START, ' ', ' ', myCabin->GetCurrentXfer(), this);
         char shaftLevelInstance[SAFE_SIZE]; // ample
         sprintf(shaftLevelInstance, "S%i-%c", GetInstance(), floor);
         sfIter = requestedSlevMap.find(shaftLevelInstance);
         if (sfIter == requestedSlevMap.end()) {
            // There wasn't a matching requested shaft level found.
            char str[80]; // ample
            sprintf(str, "No requested shaft level 'S%i-%c' in EVTModel_1605.",
                     GetInstance(), floor);
            alert(str);
            score += modelDiscrepancyFound;
            testing = false;
            return;
         }

         // "Relate myCabin to dest_SLEV across R53 using new_XFER".
         #if 0
         ((ShaftLevel_Model *)(sfIter->second))->SetMyCabin(myCabin);
         #endif // 0
         SendEvent(INITIALIZE, dir, ' ', myCabin->GetCurrentXfer(), this);
      }
   } else if (myCabin->GetCurrentXfer()->GetDestination() != floor) {
      // We'll try to replace the current destination with a better one.
      SendEvent(UPDATE, floor, ' ', myCabin->GetCurrentXfer(), this);
   }

   SendPriorityEvent(XFER_UPDATE_REQUESTED);
}

void Shaft_Model::EVTModel_1606(char floor, char dir)
{
   // [1606]
   // DEST_SELECTED
   if (!inService) {
      SendPriorityEvent(TAKE_OUT_OF_SERVICE);
   } else {
      SendPriorityEvent(SET_NEW_DEST, floor, dir);
   }
}

void Shaft_Model::EVTModel_1607(void)
{
   // [1607]
   // TAKE_OUT_OF_SERVICE
}

void Shaft_Model::EVTModel_1608(void)
{
   // [1608]
   // TAKE_OUT_OF_SERVICE
}

void Shaft_Model::EVTModel_1609(void)
{
   // [1609]
   // TRANSFER_COMPLETED
}

void Shaft_Model::EVTModel_1610(void)
{
   // [1610]
   // NO_TRANSFER_IN_PROGRESS
}

bool Shaft_Model::GetInService(void)
{
   return inService;
}

Cabin_Model *Shaft_Model::GetMyCabin(void)
{
   return myCabin;
}

void Shaft_Model::SetMyCabin(Cabin_Model* myCbn)
{
   myCabin = myCbn;
}

bool ShaftLevel_Model::Clear_call(direction dir)
{
   bool brc = false; // pessimistically

   if (dir == up) {
      // **??** TBD Add this! **??**
   } else if (dir == down) {
      // **??** TBD Add this! **??**
   } else  {
      char str[80]; // ample
      sprintf(str, "The dir parameter was '%s' in the Clear_call method.",
               dir == none ? "none" : "????");
      alert(str);
      score += modelDiscrepancyFound;
      testing = false;
   }

   return brc;
}

void ShaftLevel_Model::Clear_stop(void)
{
   if (stopRequested) {
      // UI::Clear_stop_request(Floor_name:self.Floor, Shaft_ID:self.Shaft);
      stopRequested = false;
   }
}

void ShaftLevel_Model::E_clear(void)
{
}

void ShaftLevel_Model::EVTModel_1700(void)
{
   // [1700]
   // START
   // The floor and shaft variables are initialized by the Init() method.
   inService     = true;
   stopRequested = false;
}

void ShaftLevel_Model::EVTModel_1701(char dir, char unused)
{
   // [1701]
   // FLOOR_CALLED
}

void ShaftLevel_Model::EVTModel_1702(char dir, char unused)
{
   // [1702]
   // MIGRATED_FOR_CALL
}

void ShaftLevel_Model::EVTModel_1703(void)
{
   // [1703]
   // CALL_REGISTERED
   // Ensure that the direction is correctly set now.
   myCabin->Pong();

   Id shaftModelId(typeid(Shaft_Model).name(), shaft);
   SendEventToId(SERVICE_REQUESTED, ' ', ' ', &shaftModelId, this);
   SendPriorityEvent(PENDING);
}

void ShaftLevel_Model::EVTModel_1704(void)
{
   // [1704]
   // PENDING
   // No action is associated with this event.
}

void ShaftLevel_Model::EVTModel_1705(char dir, char unused)
{
   // [1705]
   // FLOOR_CALLED
   EVTModel_1701(dir, unused);
}

void ShaftLevel_Model::EVTModel_1706(char dir, char unused)
{
   // [1706]
   // SERVICED
   // Clear the stop request if it was set.
   Clear_stop();

   // Clear serviced floor call; cases:
   //   (1) None, do nothing.
   //   (2) Up or down, clear it.
   //   (3) Up and down, clear the one in the requested direction.
   direction clearDir;
   switch (dir) {
      case '=': clearDir = none; break;
      case '<': clearDir = up;   break;
      case '>': clearDir = down; break;
      default:  clearDir = dirUnknown;
   }

   /*   direction clearDir = none;
   if () {
   } else {
   }
   */
   SendEvent(SERVICED, clearDir, ' ', myCabin->GetCurrentXfer(), this);
   SendPriorityEvent(PENDING);
}

void ShaftLevel_Model::EVTModel_1707(void)
{
   // [1707]
   // PENDING
   // No action is associated with this event.
}

void ShaftLevel_Model::EVTModel_1708(void)
{
   // [1708]
   // XFER_DELETED
   if (Floors_requested(up) || Floors_requested(down)) {
      SendPriorityEvent(CALL_REMAINING);
   } else {
      Migrate_Nreq();
      SendPriorityEvent(ALL_REQUESTS_CLEARED);
   }
}

void ShaftLevel_Model::EVTModel_1709(void)
{
   // [1709]
   // CALL_REMAINING
   // No action is associated with this event.
}

void ShaftLevel_Model::EVTModel_1710(void)
{
   // [1710]
   // MIGRATED_FOR_STOP
   stopRequested = true;
   SendPriorityEvent(STOP_REGISTERED);
}

void ShaftLevel_Model::EVTModel_1711(void)
{
   // [1711]
   // STOP_REGISTERED
   EVTModel_1703();
}

void ShaftLevel_Model::EVTModel_1712(void)
{
   // [1712]
   // STOP_REQUESTED
   // Determine if this cabin is already on the requested floor with the
   // door open. If so, we won't allow setting a stop request for this floor
   // until the door closes.
   bool okToSetStopRequested = false; // pessimistically
#if 0
{
   if (myCabin) {
      char str[160]; // ample
      sprintf(str, "myCabin: 0x%02x", myCabin);
      cout << str << endl;
      sprintf(str, "myCabin->GetCurrentFloor(): %c", myCabin->GetCurrentFloor());
      cout << str << endl;
      sprintf(str, "myCabin->GetMyDoor(): 0x%02x", myCabin->GetMyDoor());
      cout << str << endl;
      sprintf(str, "myCabin->GetMyDoor()->IsDoorClosed(): %s",
               myCabin->GetMyDoor()->IsDoorClosed() ? "true" : "false");
      cout << str << endl;
   }
}
#endif // 0
   if (!myCabin || myCabin->GetCurrentFloor() != floor
      || myCabin->GetMyDoor()->IsDoorClosed()) {
      okToSetStopRequested = true;
   }
#if 0
{
   if (stopRequested) {
      RRTLog(" * * * stopRequested.");
   }
   if (!okToSetStopRequested) {
      RRTLog(" * * * !okToSetStopRequested.");
   }
}
#endif // 0
   if (stopRequested) {
      SendPriorityEvent(STOP_ACTIVE);
   } else if (!okToSetStopRequested) {
      SendPriorityEvent(STOP_ALREADY_ACTIVE);
   } else {
      Migrate_Req();
      SendPriorityEvent(MIGRATED_FOR_STOP);
   }
}

void ShaftLevel_Model::EVTModel_1713(void)
{
   // [1713]
   // STOP_REQUESTED
   EVTModel_1712();
}

void ShaftLevel_Model::EVTModel_1714(void)
{
   // [1714]
   // ALL_REQUESTS_CLEARED
   // No action is associated with this event.
}

void ShaftLevel_Model::EVTModel_1715(void)
{
   // [1715]
   // STOP_ALREADY_ACTIVE
   // We'll just ignore this request, since we already have a request to
   // stop on this floor, or else the door is open on this floor.
   // No action is associated with this event.
}

void ShaftLevel_Model::EVTModel_1716(void)
{
   // [1716]
   // STOP_ACTIVE
   EVTModel_1715();
}

bool ShaftLevel_Model::Floors_requested(direction dir)
{
   bool brc = false; // pessimistically
   // **??** TBD Add this! **??**

   return brc;
}

void ShaftLevel_Model::Init(char flr, char shft)
{
   // N.B.: do this initialization before first use.
   floor = flr;
   shaft = shft;
}

void ShaftLevel_Model::Migrate_Nreq(void)
{
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", shaft, floor);
   sfIter = nonRequestedSlevMap.find(shaftLevelInstance);

   // This is a no-op if the shaft level instance is already not requested.
   if (sfIter == nonRequestedSlevMap.end()) {
      // The shaft level instance isn't there, so we'll migrate it.
      nonRequestedSlevMap[shaftLevelInstance] = this;
      requestedSlevMap.erase(shaftLevelInstance);
   }
}

void ShaftLevel_Model::Migrate_Req(void)
{
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", shaft, floor);
   sfIter = requestedSlevMap.find(shaftLevelInstance);

   // This is a no-op if the shaft level instance is already requested.
   if (sfIter == requestedSlevMap.end()) {
      // The shaft level instance isn't there, so we'll migrate it.
      requestedSlevMap[shaftLevelInstance] = this;
      nonRequestedSlevMap.erase(shaftLevelInstance);
   }
}

void ShaftLevel_Model::SetMyCabin(Cabin_Model* myCbn)
{
   myCabin = myCbn;
}

bool ShaftLevel_Model::StopRequested(void)
{
   return stopRequested;
}

void Signal_IO_Model::EVTModel_2000(void)
{
   // [2000]
   // START
   simulateJam = false;
   jamInPlace = false;
}

void Signal_IO_Model::EVTModel_2001(void)
{
   // [2001]
   // OPEN_DOOR
   SendDelayedEvent(programmedDoorOpenCloseTime / 2, TRANSITIONED, ' ', ' ',
                     this, this, &testing);
}

void Signal_IO_Model::EVTModel_2002(void)
{
   // [2002]
   // TRANSITIONED
   SendEvent(DOOR_IS_HALF_OPEN, ' ', ' ', myDoor, this);
   SendDelayedEvent(programmedDoorOpenCloseTime / 2, TRANSITIONED, ' ', ' ',
                     this, this, &testing);
}

void Signal_IO_Model::EVTModel_2003(void)
{
   // [2003]
   // CLOSE_DOOR
   if (simulateJam && jamInPlace) {
      SendPriorityEvent(JAMMED);
   } else {
      SendPriorityEvent(NOT_OBSTRUCTED);
   }
}

void Signal_IO_Model::EVTModel_2004(void)
{
   // [2004]
   // NOT_OBSTRUCTED
   SendDelayedEvent(programmedDoorOpenCloseTime / 2, TRANSITIONED, ' ', ' ',
                     this, this, &testing);
}

void Signal_IO_Model::EVTModel_2005(void)
{
   // [2005]
   // TRANSITIONED
   SendEvent(DOOR_IS_HALF_CLOSED, ' ', ' ', myDoor, this);
   if (simulateJam) {
      // Prepare for next jam.
      jamInPlace = true;
   }
   SendPriorityEvent(CONTINUE_CLOSING_DOOR);
}

void Signal_IO_Model::EVTModel_2006(void)
{
   // [2006]
   // JAMMED
   jamInPlace = false;
// **??** TBD Not yet jamming the elevator doors. **??**
//   SendEvent(DOOR_IS_BLOCKED, ' ', ' ', myDoor, this);
}

void Signal_IO_Model::EVTModel_2007(void)
{
   // [2007]
   // OPEN_DOOR
   EVTModel_2001();
}

void Signal_IO_Model::EVTModel_2008(void)
{
   // [2008]
   // TRANSITIONED
   SendEvent(DOOR_IS_OPEN, ' ', ' ', myDoor, this);
}

void Signal_IO_Model::EVTModel_2009(void)
{
   // [2009]
   // NOT_OBSTRUCTED
   SendEvent(DOOR_IS_CLOSED, ' ', ' ', myDoor, this);
}

void Signal_IO_Model::EVTModel_2010(void)
{
   // [2010]
   // CONTINUE_CLOSING_DOOR
   if (simulateJam && jamInPlace)  {
      SendPriorityEvent(JAMMED);
   } else {
      SendDelayedEvent(programmedDoorOpenCloseTime / 2,
                        NOT_OBSTRUCTED, ' ', ' ', this, this, &testing);
   }
}

void Signal_IO_Model::EVTModel_2011(void)
{
   // [2011]
   // JAMMED
   jamInPlace = false;
// **??** TBD Not yet jamming the elevator doors. **??**
//   SendEvent(DOOR_IS_BLOCKED, ' ', ' ', myDoor, this);
}

void Signal_IO_Model::SetMyDoor(Door_Model* myDr)
{
   myDoor = myDr;
}

////////////////////////////////////////////////////////////////////////////////
//
// StartEVT_Configuration
//
// This routine creates the initial model instance.
//
// An error message is constructed and sent to the console and to RRTLog if the
// creation was not successful.
//
// N.B.: the currentNumberOfModelInstances is pre-initialized to 0 in
//     EVTModel_Insert.cpp.
//

void  StartEVT_Configuration(void)
{
   // Initialize the first state machine instance for the test. N.B.: the
   // default is instance = 1;
   AddModelInstance(new EVT_Configuration_Model);
}

void Stim_CarButtons_Model::EVTModel_1200(void)
{
   // [1200]
   // START
}

void Stim_CarButtons_Model::EVTModel_1201(void)
{
   // [1201]
   // BEGIN_TEST
   #ifdef SHOW_HISTOGRAM
//   iatHist.restartTimer(); // first time we won't add a histogram point
   #endif // def SHOW_HISTOGRAM

   nextArrivalTime = RRTGetTime();
   double r0 = rrs.GetFloatValue(stimCarButtonsTimeUntilNextArrival,
                                  0.000001, 1.0, 0.000001);
   unsigned int timeToNextArrival = - mu * log(r0);
   nextArrivalTime += timeToNextArrival;
   unsigned int currentTime = RRTGetTime();
   unsigned int toWait = 0;
   if (nextArrivalTime > currentTime) {
      toWait = nextArrivalTime - currentTime;
   }
   SendDelayedEvent(toWait, TIME_TO_PUSH_THE_CAR_BUTTON, ' ', ' ',
                     this, this, &testing);

// *** For testing, send a car button push now. ***
//SendDelayedEvent( 0, TIME_TO_PUSH_THE_CAR_BUTTON, ' ', ' ',
//                   this, this, &testing ) ;

   // Establish the test length.
   SendDelayedEvent(TEST_LENGTH_SECONDS * 1000, QUIESCE_TEST, ' ', ' ',
                     broadcast, this, &testing);

// *** For testing, make test run for 20 seconds. ***
//SendDelayedEvent( 20 * 1000, QUIESCE_TEST, ' ', ' ',
//                     broadcast, this, &testing ) ;
}

void Stim_CarButtons_Model::EVTModel_1202(void)
{
   // [1202]
   // TIME_TO_PUSH_THE_CAR_BUTTON

   // Calculate the interarrival time and send a delayed event for the
   // next arrival. We are using a "random" (i.e., negative exponential)
   // distribution for the inter-arrival times.
   #ifdef SHOW_HISTOGRAM
//   iatHist.tally(); // add a histogram point
   #endif // def SHOW_HISTOGRAM

   double r0 = rrs.GetFloatValue(stimCarButtonsTimeUntilNextArrival,
                                  0.000001, 1.0, 0.000001);
   unsigned int timeToNextArrival = - mu * log(r0);
   nextArrivalTime += timeToNextArrival;
   unsigned int currentTime = RRTGetTime();
   unsigned int toWait = 0;
   if (nextArrivalTime > currentTime) {
      toWait = nextArrivalTime - currentTime;
   }
   SendDelayedEvent(toWait, TIME_TO_PUSH_THE_CAR_BUTTON, ' ', ' ',
                     this, this, &testing);

// *** For testing, send a second car button push in 10 seconds. ***
//SendDelayedEvent( 10000, TIME_TO_PUSH_THE_CAR_BUTTON, ' ', ' ',
//                   this, this, &testing ) ;

   // Inform our model of the stop request.
   int randomCar    = rrs.GetIntValue(stimCarButtonsRandomCar, 1,
                                       programmedNumberOfElevatorShafts, 1);
   int randomFloor  = rrs.GetIntValue(stimCarButtonsRandomFloor, 0,
                                       programmedNumberOfFloors - 1, 1);
   char chosenFloor = programmedFloorLabels[randomFloor];

// *** For testing, set floor 5 in car 1.
//int randomCar    = 1 ;
//static int randomFloor ;
//randomFloor = randomFloor ? 1 : 5 ;
//char chosenFloor = programmedFloorLabels[randomFloor];

   //#define TEST_ONLY_CAR_TWO
   #ifdef TEST_ONLY_CAR_TWO
   if (randomCar != 2) {
      return;
   }
   #endif // def TEST_ONLY_CAR_TWO
   //#define TEST_ONLY_CARS_ONE_AND_TWO
   #ifdef TEST_ONLY_CARS_ONE_AND_TWO
   if (randomCar > 2) {
      return;
   }
   #endif // def TEST_ONLY_CARS_ONE_AND_TWO

   //#define SHOW_RANDOM_CAR_VALUES
   #ifdef SHOW_RANDOM_CAR_VALUES
   {
      char str[80]; // ample
      sprintf(str, "randomCar: %i, chosenFloor: %c", randomCar, chosenFloor);
      RRTLog(str);
   }
   #endif // def SHOW_RANDOM_CAR_VALUES

   SendEventToId(STOP_REQUESTED, chosenFloor, randomCar + '0',
                  &comparatorModelId, this);
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", randomCar, chosenFloor);
   sfIter = nonRequestedSlevMap.find(shaftLevelInstance);
   if (sfIter != nonRequestedSlevMap.end()) {
      SendEvent(STOP_REQUESTED, chosenFloor, randomCar + '0',
                 sfIter->second, this);
   } else {
      sfIter = requestedSlevMap.find(shaftLevelInstance);
      if (sfIter != requestedSlevMap.end()) {
         SendEvent(STOP_REQUESTED, chosenFloor, randomCar + '0',
                    sfIter->second, this);
      } else {
         // Unable to locate the ShaftLevel instance in either map.
         char str[80]; // ample
         sprintf(str, "Unable to locate shaftLevelInstance: '%s'.",
                  shaftLevelInstance);
         alert(str);
      }
   }

   // Send the stop request command to the Elevator simulation.
   static char str[SAFE_SIZE]; // ample
   sprintf(str, "%c%c%c", pushCarButton[0], chosenFloor,
            char(randomCar  + '0'));
   ElevatorMessage("Issuing a stop request",
                    write(commandPipeFd, str, strlen(str)));

   // Put the command we're sending in our pipe trace.
   if (pt.MessageSent(str) != SUCCESS) {
      // Unable to log our pushCarButton command.
      char strx[80]; // ample
      sprintf(strx, "Unable to log pushCarButton command: '%s'.",
               str);
      alert(strx);
   }
}

void Stim_CarButtons_Model::EVTModel_1203(void)
{
   // [1203]
   // QUIESCE_TEST
   // Wait until the test is in an idle state, i.e., we have received
   // QUIESCE_TIME_SECONDS successive SHOW_UNCHANGED events.  Any SHOW_CHANGED
   // event received resets the idle counter.
   idleCounter = 0;
}

void Stim_CarButtons_Model::EVTModel_1204(void)
{
   // [1204]
   // END_TEST
   // Stop the elevator.
   ElevatorMessage("Requesting quit",
                    write(commandPipeFd, quitTest, strlen(quitTest)));

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent((char *)quitTest) != SUCCESS) {
      // Unable to log our quitTest message.
      char str[80]; // ample
      sprintf(str, "Unable to log quitTest message: '%s'.",
               quitTest);
      alert(str);
   }
}

void Stim_CarButtons_Model::EVTModel_1205(void)
{
   // [1205]
   // SHOW_CHANGED
   idleCounter = 0;
}

void Stim_CarButtons_Model::EVTModel_1206(void)
{
   // [1206]
   // SHOW_UNCHANGED
   if (++idleCounter >= QUIESCE_TIME_SECONDS) {
      SendPriorityEvent(END_TEST);
   }
}

void Transfer_Model::Change_Destination(char newDest)
{
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", shaft, destination);
   sfIter = requestedSlevMap.find(shaftLevelInstance);
   if (sfIter == requestedSlevMap.end()) {
      // There wasn't a matching old requested shaft level found.
      char str[80]; // ample
      sprintf(str, "No old requested shaft level 'S%i-%c' in "
               "Change_Destination.", shaft, destination);
      alert(str);
      score += modelDiscrepancyFound;
      testing = false;
      return;
   } else {
      // "Unrelate myCabin from old SLEV across R53 using self."

      sprintf(shaftLevelInstance, "S%i-%c", shaft, newDest);
      sfIter = requestedSlevMap.find(shaftLevelInstance);
      if (sfIter == requestedSlevMap.end()) {
         // There wasn't a matching new requested shaft level found.
         char str[80]; // ample
         sprintf(str, "No new requested shaft level 'S%i-%c' in "
                  "Change_Destination.", shaft, newDest);
         alert(str);
         score += modelDiscrepancyFound;
         testing = false;
         return;
      } else {
         // "Relate myCabin to new_SLEV across R53 using self".
         #if 0
         ((ShaftLevel_Model *)(sfIter->second))->SetMyCabin(myCabin);
         #endif // 0
         destination = newDest;
         //#define SHOW_TRANSFER_DESTINATION
         #ifdef SHOW_TRANSFER_DESTINATION
         {
            char str[80]; // ample
            sprintf(str, " * * * destination (in Change_Destination): %c",
                     destination);
            RRTLog(str);
         }
         #endif // def SHOW_TRANSFER_DESTINATION
      }
   }
}

void Transfer_Model::EVTModel_1800(void)
{
   // [1800]
   // START
   // No action is associated with this event.
}

void Transfer_Model::EVTModel_1801(char newDest, char parm2)
{
   // [1801]
   // UPDATE
   Change_Destination(newDest);
   SendPriorityEvent(UPDATE_SUCCEEDED);
}

void Transfer_Model::EVTModel_1802(void)
{
   // [1802]
   // UPDATE_SUCCEEDED
   // No action is associated with this event.
}

void Transfer_Model::EVTModel_1803(void)
{
   // [1803]
   // E_CANCEL
}

void Transfer_Model::EVTModel_1804(void)
{
   // [1804]
   // E_CANCEL
}

void Transfer_Model::EVTModel_1805(void)
{
   // [1805]
   // DISPATCH_CABIN
   #ifdef SHOW_CABIN_PROCESSING
   {
      char str[80]; // ample
      sprintf( str, "   $$$ sending GOin Transfer_Model::EVTModel_1805" ) ;
      RRTLog( str );
   }
   #endif // def SHOW_CABIN_PROCESSING
   SendEvent(GO, ' ', ' ', myCabin, this);
}

void Transfer_Model::EVTModel_1806(void)
{
   // [1806]
   // CABIN_AT_DESTINATION
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", shaft, destination);
   sfIter = requestedSlevMap.find(shaftLevelInstance);
   if (sfIter == requestedSlevMap.end()) {
      // There wasn't a matching requested shaft level found.
      char str[80]; // ample
      sprintf(str, "No requested shaft level 'S%i-%c' in EVTModel_1806.",
               shaft, myCabin->GetCurrentFloor());
      alert(str);
      score += modelDiscrepancyFound;
      testing = false;
   } else {
      SendEvent(SERVICED, myCabin->GetTravelDirection(), ' ',
                 (ShaftLevel_Model *)(sfIter->second), this);
   }
}

void Transfer_Model::EVTModel_1807(char dir, char parm2)
{
   // [1807]
   // SERVICED
   if (dir == none) {
      // Only the requested stop was serviced.  If the cabin has reached
      // the top or bottom floor, reverse the travel direction.
      char currentFloor = myCabin->GetCurrentFloor();
      char *pCurrentFloorLabel = strchr(programmedFloorLabels, currentFloor);
      if (pCurrentFloorLabel != NULL) {
         int floorIndex = pCurrentFloorLabel - programmedFloorLabels;
         if (myCabin->GetTravelDirection() == up
           && floorIndex == programmedNumberOfFloors - 1) {
            myCabin->ToggleDir();
         }
         // **??** TBD Should we handle this case as well? **??**
         //else if (myCabin->GetTravelDirection() == down && floorIndex == 0) {
         //  myCabin->ToggleDir();
         //}
         SendPriorityEvent(DELETE);
      } else {
         alert("NULL pCurrentFloorLabel in EVTModel_1807.");
         score += modelDiscrepancyFound;
         testing = false;
      }
   } else {
      // At least one floor call was cleared.
      callInProgress = true; // bias destination selection direction
      if (dir != myCabin->GetTravelDirection()) {
         // Cleared call was opposite the current travel direction.
         myCabin->ToggleDir();
      }
      SendPriorityEvent(HOLD_FOR_CALL);
   }
}

void Transfer_Model::EVTModel_1808(void)
{
   // [1808]
   // DELETE
   // There may be requests pending.
   callInProgress = false;
   SendEvent(TRANSFER_COMPLETED, ' ', ' ', myCabin->GetMyShaft(), this);

   // Release requested SLEV.
   char shaftLevelInstance[SAFE_SIZE]; // ample
   sprintf(shaftLevelInstance, "S%i-%c", shaft, destination);
   sfIter = requestedSlevMap.find(shaftLevelInstance);
   if (sfIter != requestedSlevMap.end()) {
      // The shaft level instance is there.
      SendEvent(XFER_DELETED, ' ', ' ',
                 (ShaftLevel_Model *)(sfIter->second), this);

      // Unlink before deletion.
      myCabin->SetCurrentXfer(0);

      // Suicide.
      if (RemoveModelInstance(this) == 0) {
         alert("RemoveModelInstance returned 0 in EVTModel_1808.");
         score += modelDiscrepancyFound;
         testing = false;
      }
      delete this;
   } else {
      alert("shaftLevelInstance missing in EVTModel_1808.");
      score += modelDiscrepancyFound;
      testing = false;
   }
}

void Transfer_Model::EVTModel_1809(void)
{
   // [1809]
   // CABIN_AT_DESTINATION
   EVTModel_1806();
}

void Transfer_Model::EVTModel_1810(char newDest, char parm2)
{
   // [1810]
   // UPDATE
   SendEvent(CHANGE_DESTINATION, newDest, ' ', myCabin);
   SendPriorityEvent(UPDATE_REQUESTED);
}

void Transfer_Model::EVTModel_1811(void)
{
   // [1811]
   // UPDATE_REQUESTED
   // No action is associated with this event.
}

void Transfer_Model::EVTModel_1812(char updatedDest, char parm2)
{
   // [1812]
   // CABIN_REDIRECTED
   Change_Destination(updatedDest);
   SendPriorityEvent(UPDATE_SUCCEEDED);
}

void Transfer_Model::EVTModel_1813(void)
{
   // [1813]
   // UPDATE_SUCCEEDED
   // No action is associated with this event.
}

void Transfer_Model::EVTModel_1814(void)
{
   // [1814]
   // HOLD_FOR_CALL
   // Once door closes (for the first time after arrival) the cleared call
   // direction can be safely forgotten.  A passenger should have selected
   // a floor in the requested direction by now.  If not, too bad.
}

void Transfer_Model::EVTModel_1815(void)
{
   // [1815]
   // DOOR_CLOSED
   EVTModel_1808();
}

void Transfer_Model::EVTModel_1816(char dir, char parm2)
{
   // [1816]
   // INITIALIZE
   callInProgress = false;
   // **??** TBD UI::Set_destination(Shaft_ID:self.Shaft,Floor_name:self.Destination) **??**
   if (myCabin->GetTravelDirection() != dir) {
      myCabin->ToggleDir();
   }
   SendEvent(NEW_TRANSFER, ' ', ' ', myCabin, this);
}

bool Transfer_Model::GetCallInProgress(void)
{
   return callInProgress;
}

char Transfer_Model::GetDestination(void)
{
   return destination;
}

void Transfer_Model::Init(char dest, char shft, Cabin_Model *cabin)
{
   destination = dest;
   //#define SHOW_TRANSFER_DESTINATION
   #ifdef SHOW_TRANSFER_DESTINATION
   {
      char str[80]; // ample
      sprintf(str, " * * * destination (in Init): %c", destination);
      RRTLog(str);
   }
   #endif // def SHOW_TRANSFER_DESTINATION
   shaft       = shft;
   myCabin     = cabin;
}

void Transport_Model::EVTModel_1900(void)
{
   // [1900]
   // START
   int transitDelay = 1000 * programmedFloorHeight
           / (NUMBER_OF_DIVISIONS_PER_FLOOR * programmedMaximumCabinVelocity);
   //#define SHOW_FLOOR_HEIGHT
   #ifdef SHOW_FLOOR_HEIGHT
   {
      char str[80]; // ample
      sprintf(str, " * * * programmedFloorHeight (in EVTModel_1900): %f", programmedFloorHeight);
      RRTLog(str);
   }
   #endif // def SHOW_FLOOR_HEIGHT
   //#define SHOW_TRANSIT_DELAY
   #ifdef SHOW_TRANSIT_DELAY
   {
      char str[80]; // ample
      sprintf(str, " * * * transitDelay (in EVTModel_1900): %i ms", transitDelay);
      RRTLog(str);
   }
   #endif // def SHOW_TRANSIT_DELAY
   // **??** This assumes we start on the ground floor. It should be dynamic! **??**
   currentFloor    = programmedFloorLabels[programmedGroundFloorLevel];
   currentPosition = programmedGroundFloorLevel * programmedFloorHeight;
   destFloor       = ' ';
   destPosition    = currentPosition;
   dirScale        = 1;
   transitDistance = programmedFloorHeight / NUMBER_OF_DIVISIONS_PER_FLOOR;
   transitTime     = transitDelay;
   SendPriorityEvent(INITIALIZED);
}

void Transport_Model::EVTModel_1901(void)
{
   // [1901]
   // /INITIALIZED
   dirScale = 0; // not moving
}

void Transport_Model::EVTModel_1902(char dest, char unused)
{
   // [1902]
   // GO_TO_FLOOR
   // Initialize new move.
   currentFloor = myCabin->GetCurrentFloor();
   char *pCurrentFloorLabel = strchr(programmedFloorLabels, currentFloor);
   if (pCurrentFloorLabel != NULL) {
      int floorIndex = pCurrentFloorLabel - programmedFloorLabels;
      double startingPosition = floorIndex * programmedFloorHeight;
      currentPosition = startingPosition;
      destFloor = dest;
      char *pDestFloorLabel = strchr(programmedFloorLabels, destFloor);
      if (pDestFloorLabel != NULL) {
         floorIndex = pDestFloorLabel - programmedFloorLabels;
         destPosition = floorIndex * programmedFloorHeight;
         double distance = destPosition - startingPosition;
         dirScale = 0;
         if (distance > FUZZ) {
            dirScale = 1;
         } else if (distance < -FUZZ) {
            dirScale = -1;
         }

         // Simulate the transit time.
         #ifdef SHOW_CABIN_PROCESSING
         {
            char str[80]; // ample
            sprintf( str, "   $$$ startingPosition = %f, destPosition = %f, distance = %f in Transport_Model::EVTModel_1902",
                     startingPosition, destPosition, distance );
            RRTLog(str);
            sprintf( str, "   $$$ sending delayed event TIME_TO_PASS_FLOOR (%i ms) in Transport_Model::EVTModel_1902",
                     transitTime );
            RRTLog(str);
         }
         #endif // def SHOW_CABIN_PROCESSING
         SendDelayedEvent(transitTime, TIME_TO_PASS_FLOOR, ' ', ' ',
                           this, this, &testing);
      } else {
         alert("NULL pDestFloorLabel in EVTModel_1902.");
         score += modelDiscrepancyFound;
         testing = false;
      }
   } else {
      alert("NULL pCurrentFloorLabel in EVTModel_1902.");
      score += modelDiscrepancyFound;
      testing = false;
   }
}

void Transport_Model::EVTModel_1903(void)
{
   // [1903]
   // TIME_TO_PASS_FLOOR
   bool atFloor = false; // assume we're still between floors

   // Update our load's (cabin's) simulated position (in meters above
   // the bottom of the shaft).
   currentPosition += dirScale * transitDistance;

   #if 0
   // **??** TBD For debugging, announce the model's position for this cabin. **??**
   char str[120]; // ample
   sprintf(str, "   $$$ in Transport_Model::EVTModel_1903 shaft: %i, currentPosition: %.6f, destPosition: %.6f",
            GetInstance(), currentPosition, destPosition);
   RRTLog(str);
   #endif // 0

   // See if we're at a floor boundary.
   for (int i = 0; i < programmedNumberOfFloors; i++) {
      if (fabs(currentPosition - i * programmedFloorHeight) < FUZZ) {
         atFloor = true;
         myCabin->UpdateLocation(programmedFloorLabels[i]);

         // Send a floor position msg to the pipe trace: <"@"><floor><shaft>.
         char msg[SAFE_SIZE];
         strcpy(msg, carLocation);
         msg[1] =  programmedFloorLabels[i];
         msg[2] = GetInstance() + '0';
         if (!ModelInfo(msg)) {
            testing = false;
         }
         break;
      }
   }

   // See if we're at the destination.
   if (fabs(currentPosition - destPosition) < FUZZ) {
      // We've reached our destination.
      SendEvent(ARRIVED_AT_FLOOR, ' ', ' ', myCabin, this);
      SendPriorityEvent(DONE);
   } else {
      // Still enroute.
      if (atFloor) {
         char *pCurrentFloorLabel;
         if (dirScale == 1) {
            // Going up.
            pCurrentFloorLabel = strchr(programmedFloorLabels,
                                         currentFloor);
            pCurrentFloorLabel++;
         } else {
            // Going down.
            pCurrentFloorLabel = strchr(programmedFloorLabels,
                                         currentFloor);
            pCurrentFloorLabel--;
         }
         currentFloor = *pCurrentFloorLabel;
      }

      // Simulate the transit time.
      #ifdef SHOW_CABIN_PROCESSING
      {
         char str[80]; // ample
         sprintf( str, "   $$$ sending delayed event TIME_TO_PASS_FLOOR (%i ms) in Transport_Model::EVTModel_1903",
                  transitTime );
         RRTLog(str);
      }
      #endif // def SHOW_CABIN_PROCESSING
      SendDelayedEvent(transitTime, TIME_TO_PASS_FLOOR, ' ', ' ',
                        this, this, &testing);
   }
}

void Transport_Model::EVTModel_1904(void)
{
   // [1904]
   // DONE
   dirScale = 0; // not moving
}

void Transport_Model::EVTModel_1905(char dest, char unused)
{
   // [1905]
   // GO_TO_FLOOR
   SendEvent(CT_GO_TO_FLOOR, dest, ' ', this, this);
}

void Transport_Model::EVTModel_1906(char dest, char unused)
{
   // [1906]
   // CT_GO_TO_FLOOR
   // Updating for new move.
   double startingPosition = currentPosition;
   destFloor = dest;
   char *pDestFloorLabel = strchr(programmedFloorLabels, destFloor);
   if (pDestFloorLabel != NULL) {
      int floorIndex = pDestFloorLabel - programmedFloorLabels;
      destPosition = floorIndex * programmedFloorHeight;
      double distance = destPosition - startingPosition;
      dirScale = 0;
      if (distance > FUZZ) {
         dirScale = 1;
      } else if (distance < -FUZZ) {
         dirScale = -1;
      }
   } else {
      alert("NULL pDestFloorLabel in EVTModel_1906.");
      score += modelDiscrepancyFound;
      testing = false;
   }
}

void Transport_Model::EVTModel_1907(void)
{
   // [1907]
   // TIME_TO_PASS_FLOOR
   EVTModel_1903();
}

void Transport_Model::EVTModel_1908(void)
{
   // [1908]
   // TIME_TO_PASS_FLOOR
   EVTModel_1903();
}

void Transport_Model::EVTModel_1909(void)
{
   // [1909]
   // QUICK_STOP
}

void Transport_Model::EVTModel_1910(void)
{
   // [1910]
   // QUICK_STOP
}

void Transport_Model::EVTModel_1911(char dest, char unused)
{
   // [1911]
   // GO_TO_FLOOR
   EVTModel_1905(dest, unused);
}

void Transport_Model::EVTModel_1912(void)
{
   // [1912]
   // QUICK_STOP
}

Cabin_Model *Transport_Model::GetMyCabin(void)
{
   return myCabin;
}

bool Transport_Model::GoToFloor(char dest)
{
   bool brc = true;
   // **??** TBD Add logic here to determine if it's possible to execute
   //        a safe stop. Return false if not. **??**
   #ifdef SHOW_CABIN_PROCESSING
   {
      char str[80]; // ample
      sprintf( str, "   $$$ sending GO_TO_FLOOR %c in Transport_Model::GoToFloor",
               dest );
      RRTLog(str);
   }
   #endif // def SHOW_CABIN_PROCESSING
   SendEvent(GO_TO_FLOOR, dest, ' ', this, this);

   return brc;
}

void Transport_Model::SetMyCabin(Cabin_Model* myCbn)
{
   myCabin = myCbn;
}


