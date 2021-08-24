// EVTTestConstants.h -- header file for Elevator Verification Test constants
//
//                                                      <by Cary WR Campbell>
//
// This software is copyrighted © 2007 - 2015 by Codecraft, Inc.
//
// The following terms apply to all files associated with the software
// unless explicitly disclaimed in individual files.
//
// The authors hereby grant permission to use, copy, modify, distribute,
// and license this software and its documentation for any purpose, provided
// that existing copyright notices are retained in all copies and that this
// notice is included verbatim in any distributions. No written agreement,
// license, or royalty fee is required for any of the authorized uses.
// Modifications to this software may be copyrighted by their authors
// and need not follow the licensing terms described here, provided that
// the new terms are clearly indicated on the first page of each file where
// they apply.
//
// IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
// FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
// ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
// DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
// IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
// NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
// MODIFICATIONS.
//
// GOVERNMENT USE: If you are acquiring this software on behalf of the
// U.S. government, the Government shall have only "Restricted Rights"
// in the software and related documentation as defined in the Federal
// Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
// are acquiring the software on behalf of the Department of Defense, the
// software shall be classified as "Commercial Computer Software" and the
// Government shall have only "Restricted Rights" as defined in Clause
// 252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
// authors grant the U.S. Government and others acting in its behalf
// permission to use and distribute the software in accordance with the
// terms specified in this license.

#pragma once

// Alert messages.
const char alertA[] = "";
const char alertB[] = "";
const char alertC[] = "Car location does not match model.";
const char alertD[] = "Door status does not match model.";
const char alertE[] = "";
const char alertF[] = "";
const char alertG[] = "";
const char alertH[] = "";
const char alertI[] = "Direction indicator does not match model.";
const char alertJ[] = "";
const char alertK[] = "";
const char alertL[] = "";
const char alertM[] = "";
const char alertN[] = "";
const char alertO[] = "";
const char alertP[] = "";
const char alertQ[] = "";
const char alertR[] = "";
const char alertS[] = "";
const char alertT[] = "";
const char alertU[] = "";
const char alertV[] = "";
const char alertW[] = "";
const char alertX[] = "";
const char alertY[] = "";
const char alertZ[] = "<unexpected alert indicator>";

// Comparison interval tolerances.
const int  compareCarLocationsTimeout =      50; // ms
const int  compareDirIndicatorsTimeout =     50; // ms
const int  compareDoorStatusesTimeout =      50; // ms
                                                 //                                                  //
const int  CHART_INSTRUMENTED_LINE_LENGTH
                                      =     160;
const int  CHART_LINE_LENGTH          =      80;
const int  DID_NOT_DETECT             =      -1;
const int  FATAL_ABORT_VALUE          =     100;
const double FUZZ                     =       0.000001; // for float comparisons
const int  INSTRUMENTATION_POSITION   =      CHART_LINE_LENGTH + 1;
const int  MAX_NAME_LENGTH            =      50; // Fifo name
const int  MAX_NUMBER_OF_COMMAND_BYTES =    500;
const int  MAX_NUMBER_OF_ELEVATORS    =       9;
const int  MIN_NUMBER_OF_ELEVATORS    =       1;
const int  MAX_NUMBER_OF_FLOORS       =       9;
const int  MIN_NUMBER_OF_FLOORS       =       2;
const int  MAX_NUMBER_OF_MODEL_INSTANCES =  250;
const int  NUMBER_OF_MODEL_BUFFER_SLOTS =    50;
const int  NUMBER_OF_DIVISIONS_PER_FLOOR =    7; // matches Elevator simulation
const char OUR_MODULE_NAME[]          =   "EVT";
const int  QUIESCE_TIME_SECONDS       =       5;
const char REQUIREMENTS_CSV_FILE[]    =  "ElevatorRequirements.csv";
const int  TEST_FAILURE_SCORE         =     750; // score points
const int  TEST_LENGTH_SECONDS        =     240;
const char TEST_RUNS_CSV_FILE[]       =  "ElevatorTestRuns.csv";
const int  TOTAL_REQ_COUNT            =      14; // TBD keep updated to total
                                                 //   number of requirements!
const char TRANSACTION_LOG_FILE_EXT[] =  ".ptl"; // include the dot
const int  TRANSACTION_LOG_SIZE       = 5120000; // bytes
const int  USER_ABORT_VALUE           =      99;

// Shared typedefs and structs.
typedef enum  direction { up = 60, none, down, dirUnknown } elevatorDirection;
                                                               // '<', '=', '>'
typedef enum  doorStatus { doorLocked, doorClosed, doorAjar, doorFullyOpen,
              doorBlocked, doorHeld, doorUnknown } elevatorDoorStatus;
typedef struct
{
   // Describes a shaft's current state for display purposes.
   char       floorLocation; // or most recent, if car is moving
   direction  indicator;
   doorStatus status;
} shaftStatus;

// Scoring constants.
const int  fatalDiscrepancyFound      =  500000; // score points
const int  configDiscrepancyFound     =  100000; // score points
const int  modelDiscrepancyFound      =  100000; // score points
const int  majorDiscrepancyFound      =    1000; // score points
const int  minorDiscrepancyFound      =      50; // score points
const int  trivialDiscrepancyFound    =       1; // score points

// Repeatable random identifiers. In order to maintain repeatability, follow
// these random identifier list maintenance rules:
//
// 1.  Do not change the value of any entry in this list.
// 2.  Do not move any entry.
// 3.  Do not delete any entry.
// 4.  You may rename an item in place.
// 5.  Add new entries at the bottom.
//
// An item's position in this enum sequence determines its
// numeric value, which determines its randomization
// characteristics.  By maintaining the current values of these
// items, existing tests' randomizations are undisturbed.
//
// Although the initial entries are sorted, do not try to
// make any additions in sort sequence--just add new entries
// at the bottom of the enum list.
//

enum RandItems
{
   configBlockClearTime                   =   0,
   configDoorOpenCloseTime,
   configFloorHeight,
   configFloorLabels,
   configGroundFloorLevel,
   configMaximumCabinVelocity,
   configMaxCloseAttempts,
   configNormalDoorWaitTime,
   configMinimumStoppingDistance,
   configNumberofElevatorShafts,
   configNumberofFloors,

   stimCarButtonsRandomCar,
   stimCarButtonsRandomFloor,
   stimCarButtonsTimeUntilNextArrival,
   // *** add items here ***
   unused
};

