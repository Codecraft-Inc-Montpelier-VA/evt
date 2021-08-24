/************************************ EVT.cpp **********************************
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
 *  This program is the Elevator Verification Test client. It uses the
 *  Repeatable Random Test Generation (RRTGen) framework.
 *
 *  This file is linked with test-specific and support files to create a
 *  test executable ("evt"). See its Makefile for details.
 *
 *  The resulting test program (client) and the system under test (SUT) elevator
 *  program (server) share two pipes, ElevatorCommandsPipe (client --> server)
 *  and ElevatorStatusPipe (client <-- server), to send commands from the client
 *  to the server and to send back status from the server to the client.  The
 *  server creates the two pipes if they don't already exist.
 *
 *  The server program should be started first, in a Terminal window; then the
 *  client program should be started, in another Terminal window.
 *
 *  As the simulation progresses, status notifications about the progress of
 *  the SUT are passed back to this program via the status pipe for comparison
 *  with the test model, etc.
 *
 *  The normal test execution is ended by a "quitTest" command from the client
 *  to the server upon a pre-determined run time and an P_END response from the
 *  server.
 *
 *  The test may be interrupted by the user, in which case a
 *  "userRequestedQuitTest" command is sent from the client to the server
 *  resulting in an P_END response from the server.
 */

/******************************* Configuration ********************************/
/* Uncomment the following define line for a histogram of coroutine           **
** round-trip latencies.                                                      */
#define SHOW_HISTOGRAM
/******************************************************************************/

#include <iostream>
#include <stdio.h>                     // getchar
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <typeinfo>                    // typeid (for Cygwin)
#include "EVTTestConstants.h"
#include "EVTModel_Insert.h"           // must precede RRTGen.h
#include "RRTGen.h"                    // must precede EVT.h
#include "EVTPipesInterface.h"
#include "RRandom.h"                   // repeatable random generator support
#include "Histospt.h"                  // histogram support
#include "sccorlib.h"                  // coroutine support

using namespace std;

//typedef unsigned char byte;

// Variables defined elsewhere.
extern unsigned int      alertIssued;
extern int               alertMessageIndex;
extern int               commandPipeFd;// we write this pipe
extern int               currentNumberOfModelInstances;
extern char              floorLabels[];
#ifdef SHOW_HISTOGRAM
//extern TimeIntervalHistogram iatHist;
extern TimeIntervalHistogram itHistCR;
#endif // def SHOW_HISTOGRAM
extern int               numberOfFloors;
extern int               programmedNumberOfElevatorShafts;
extern char              RoundtripCounterOutputString[];
extern int               score;
extern int               statusPipeFd; // we read this pipe
extern bool              testing;      // primary flag that test is still active
extern Fifo              theEventQ;
extern Fifo              theEventSelfDirectedQ;

// Model component IDs.
Id comparatorModelId(typeid(Comparator_Model).name() , 1);
Id evtConfigurationModelId(typeid(EVT_Configuration_Model).name() , 1);
Id evtShowModelId(typeid(EVT_Show_Model).name() , 1);
Id stimCarButtonsModelId(typeid(Stim_CarButtons_Model).name() , 1);

const int                SAFE_SIZE = 32; // ample allocation for short strings

// Global variables that are shared between components.
bool                     exitRequestedByUser;
bool                     firstShowCall;
char                     line[CHART_INSTRUMENTED_LINE_LENGTH];
bool                     logInstrumentation;
bool                     pipeOpIsSuccessful;
TransactionLog           pt;           // named pipe transactions
RepeatableRandomTest     rrt("Elevator Verification Test"); // the test
RRandom                  rrs;          // rrandom server instance
RequirementsTallyHandler rth;
unsigned int             showAlertIssued;
char                     showCommandsIssued[MAX_NUMBER_OF_COMMAND_BYTES];
shaftStatus              showElevator[MAX_NUMBER_OF_ELEVATORS + 1];//1-indexed
shaftStatus              showModel[MAX_NUMBER_OF_ELEVATORS + 1];  // 1-indexed
unsigned int             showPoints;
unsigned int             showTime;
char                     sut_name_and_version[MAX_NAME_AND_VERSION_LEN];

// Reduce a redundant form.
#define ElevatorMessage(a,b) if (pipeOpIsSuccessful) AttemptPipeOp(a, b)

// Locally-defined routines.
void  DeviceInfoThread(void);        // coroutine
void  DoShowEvt(shaftStatus  *modeledElevatorStatus,
                 shaftStatus  *elevatorStatus,
                 unsigned int theTime,
                 char         *commandsIssued,
                 unsigned int alerts,
                 unsigned int points,
                 bool         testIsQuiescent);
bool  InitializeShowEvt(int numberOfShafts);
bool  ModelInfo(char *pMsg);
void  UserInterface(void);           // coroutine

// Routines defined elsewhere.
int   AttemptPipeOp(const char *operationName, int status);
void  PopulateEVT(void);
void  StartEVT_Configuration(void);

////////////////////////////////////////////////////////////////////////////////
//
// alert
//
// This routine sends an alert message to the test log file and to the console.
//
// Inputs: message (const char string)
//
// Returns: None
//

void  alert(const char *message)
{
   cout << "    *** ALERT: " << message << endl;
   if (rrt.IsTestInProgress()) {
      char str[MAX_LOG_RECORD_SIZE + 1]; // ample (with nul char)
      sprintf(str, "*** ALERT: %s", message);
      rrt.Log(str);
   }
}

////////////////////////////////// Coroutine ///////////////////////////////////
//
// DeviceInfoThread
//
// This coroutine watches the incoming pipe from the Elevator simulator for
// relevant elevator status notifications and sends appropriate events to
// various model components.
//
// This implementation forgoes the notification handler and its queue
// of incoming message notifications since this routine does very little
// processing other than to queue events in the model queue.  It is felt
// that the extra level of queuing is superfluous in this instance.
//
// Messages have a trailing newline in order to keep them separated.
//

void  DeviceInfoThread(void)
{
   rrt.Log("DeviceInfoThread started.");

   int numRead = 0; // invalid default
   static char buf[MAX_BUF_SIZE];

   while (testing) {
      ElevatorMessage("Reading status pipe",
                       numRead = read(statusPipeFd, buf, MAX_BUF_SIZE));
      if (numRead == -1 && errno == EAGAIN) {
         // No data was available.
      } else if (numRead > 0) {
         char *pMsg = buf;
         bool moreMessages = true;
         buf[numRead] = '\0'; // mark the end of all messages in buf

         // Put the received transaction in our pipe trace.
         if (pt.MessageReceived(pMsg) != SUCCESS) {
            moreMessages = false;
            testing = false;
         }

         char *pc = strchr(pMsg, '\n');
         if (pc != NULL) {
            // Put a nul char in place of the newline char.
            *pc = '\0';
         }
         while (moreMessages && testing) {
            /////////////////////////////// P_END //////////////////////////////

            if (!strcmp(pMsg, P_END)) {
               if (exitRequestedByUser) {
                  cout << "\nExit requested by user." << endl;
                  rrt.Log("Exit requested by user.");

                  // Indicate that the test is ending, so it's time to wrap
                  // things up.
                  SendEvent(QUIT_TEST, ' ', ' ', broadcast, DEVICE_INTERFACE);
               } else {
                  SendEvent(END_TEST, ' ', ' ', broadcast, DEVICE_INTERFACE );
               }
            }

            //////////////////////////////// @yx ///////////////////////////////

            else if (pMsg[0] == carLocation[0]
                   && strlen(pMsg) == strlen(carLocation)) {
               // pMsg is <"@"><floor><shaft>.
               SendEventToId(CAR_LOCATION, pMsg[1], // floor
                              pMsg[2],               // car
                              &comparatorModelId, DEVICE_INTERFACE);
            }

            //////////////////////////////// %sx ///////////////////////////////

            else if (pMsg[0] == doorIsOpen[0] // surrogate
                   && strlen(pMsg) == strlen(doorIsOpen)) {
               // pMsg is <"%"><doorStatus><shaft>.
               SendEventToId(DOOR_STATUS, pMsg[1],  // door status
                              pMsg[2],               // car
                              &comparatorModelId, DEVICE_INTERFACE);
            }

            //////////////////////////////// ~dx ///////////////////////////////

            else if (pMsg[0] == indicatorIsDown[0] // surrogate
                   && strlen(pMsg) == strlen(indicatorIsDown)) {
               // pMsg is <"~"><direction indicator><shaft>.
               SendEventToId(DIR_INDICATOR, pMsg[1],  // direction indicator
                              pMsg[2],                 // car
                              &comparatorModelId, DEVICE_INTERFACE);
            }

            /////////////////////////////// ..#BAD /////////////////////////////

            else if (pMsg[2] == '#' && !strcmp(pMsg + 3, P_BAD)) {
               if (!strncmp(pMsg, endConfiguration + 1,
                              strlen(endConfiguration) - 1)) {
                  // pMsg is "EC#BAD".
                  SendEventToId(ABORT_TEST, ' ', ' ',
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               } else {
                  char str[80]; // ample
                  sprintf(str, "Bad message from Elevator (length is %zi)"
                                " in DeviceInfoThread: '%s'",
                           strlen(pMsg), pMsg);
                  alert(str);
                  score += fatalDiscrepancyFound;
                  testing = false;
               }
            }

            /////////////////////////////// ..#OK //////////////////////////////

            else if (pMsg[2] == '#' && !strcmp(pMsg + 3, P_OK)) {
               if (!strncmp(pMsg, setBlockClearTime + 1,
                              strlen(setBlockClearTime) - 1)) {
                  // pMsg is "BC#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setDoorOpenCloseTime + 1,
                                   strlen(setDoorOpenCloseTime) - 1)) {
                  // pMsg is "DO#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, endConfiguration + 1,
                                   strlen(endConfiguration) - 1)) {
                  // pMsg is "EC#OK".
                  // Start up the rest of the static state machine instances.
                  PopulateEVT();
                  SendEvent(START, ' ', ' ', broadcast, DEVICE_INTERFACE);
                  SendEventToId(DO_INITIALIZE, ' ', ' ',
                                 &comparatorModelId, DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setFloorHeight + 1,
                                   strlen(setFloorHeight) - 1)) {
                  // pMsg is "FH#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setFloorLabels + 1,
                                   strlen(setFloorLabels) - 1)) {
                  // pMsg is "FL#OK".
                  SendEventToId(SET_GROUND_FLOOR, ' ', ' ',
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               }
               else if (!strncmp(pMsg, setGroundFloorLevel + 1,
                                   strlen(setGroundFloorLevel) - 1)) {
                  // pMsg is "GF#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setMaxCabinVelocity + 1,
                                   strlen(setMaxCabinVelocity) - 1)) {
                  // pMsg is "CV#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setMaxCloseAttempts + 1,
                                   strlen(setMaxCloseAttempts) - 1)) {
                  // pMsg is "CA#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setMinStoppingDistance + 1,
                                   strlen(setMinStoppingDistance) - 1)) {
                  // pMsg is "MS#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setNormalDoorWaitTime + 1,
                                   strlen(setNormalDoorWaitTime) - 1)) {
                  // pMsg is "DW#OK".
                  SendEventToId(DONE, ' ', ' ', &evtConfigurationModelId,
                                 DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setNumberOfElevators + 1,
                                   strlen(setNumberOfElevators) - 1)) {
                  // pMsg is "NE#OK".
                  SendEventToId(SET_FLOOR_LABELS, ' ', ' ',
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               } else if (!strncmp(pMsg, setNumberOfFloors + 1,
                                   strlen(setNumberOfFloors) - 1)) {
                  // pMsg is "NF#OK".
                  SendEventToId(SET_SHAFT_COUNT, ' ', ' ',
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               } else {
                  char str[80]; // ample
                  sprintf(str, "Bad message from Elevator (length is %zi)"
                                " in DeviceInfoThread: '%s'",
                           strlen(pMsg), pMsg);
                  alert(str);
                  score += fatalDiscrepancyFound;
                  testing = false;
               }
            }

            /////////////////////////////// FLabc //////////////////////////////

            else if (!strncmp(pMsg, queryFloorLabels + 1,
                                strlen(queryFloorLabels) - 1)) {
               if (strlen(pMsg) == strlen(queryFloorLabels) - 1
                                                            + numberOfFloors) {
                  // pMsg is <"F"><"L"><floorLabels>.
                  strcpy(floorLabels, pMsg + strlen(queryFloorLabels) - 1);
                  SendEventToId(FLOOR_LABELS_RECEIVED, ' ', ' ',
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               } else {
                  char str[80]; // ample
                  sprintf(str, "Bad floor labels from Elevator (length is %zi)"
                                " in DeviceInfoThread: '%s'",
                           strlen(pMsg) - strlen(queryFloorLabels) - 1,
                           pMsg + strlen(queryFloorLabels) - 1);
                  alert(str);
                  score += fatalDiscrepancyFound;
                  testing = false;
               }
            }

            /////////////////////////////// MDyx* //////////////////////////////

            else if (!strncmp(pMsg, queryMaxDimensions + 1,
                                strlen(queryMaxDimensions) - 1)) {
               if (strlen(pMsg) == strlen(queryMaxDimensions) - 1 + 3) {
                  // pMsg is <"M"><"D"><#floors><#shafts><*|groundFloor>.
                  char parm1 = ((pMsg[2] - '0') << 4) + (pMsg[3] -'0');
                  if (pMsg[4] != '*') {
                     pMsg[4] -= '0';
                  }
                  char parm2 = pMsg[4];
                  SendEventToId(MAX_BLDG_DIMS_RECEIVED, parm1, parm2,
                                 &evtConfigurationModelId, DEVICE_INTERFACE);
               } else {
                  char str[80]; // ample
                  sprintf(str, "Bad maximum dimensions response (length is %zi)"
                                " in DeviceInfoThread.", strlen(pMsg));
                  alert(str);
                  score += fatalDiscrepancyFound;
                  testing = false;
               }
            }

            //////////////////////////////// CXx ///////////////////////////////
            // This is for debugging purposes--indicates button press wasn't  //
            // available since the cabin was already on the requested floor.  //
            ////////////////////////////////////////////////////////////////////

            else if (pMsg[0] == pushCarButton[0] && pMsg[1] == 'X'
                   && strlen(pMsg) == strlen(pushCarButton)) {
               // pMsg is <"C"><"X"><shaft>.
            }

            //////////////////////////////// @yx ///////////////////////////////
            // This is for debugging purposes--indicates position of cabin    //
            // at positions other than a floor boundary.                      //
            ////////////////////////////////////////////////////////////////////

            else if (pMsg[0] == 'a'
                   && strlen(pMsg) == strlen(carLocation)) {
               // pMsg is <"a"><floor><position above floor>.
            }

            /////////////////////////////// ????? //////////////////////////////

            else {
               char str[80]; // ample
               sprintf(str, "Unknown notification (length is %zi)"
                             " in DeviceInfoThread: '%s'",
                        strlen(pMsg), pMsg);
               alert(str);
               score += fatalDiscrepancyFound;
               testing = false;
            }

            // See if there are any more messages in the buf.
            if (!*(pMsg += strlen(pMsg) + 1)) {
               moreMessages = false;
            } else {
               pc = strchr(pMsg, '\n');
               if (pc != NULL) {
                  // Put a nul char in place of the newline char.
                  *pc = '\0';
               }
            }
         }
      }
      wait(1 /*10*/); // ms
   }

   rrt.Log("DeviceInfoThread ended.");
}

////////////////////////////////////////////////////////////////////////////////
//
// DoShowEvt
//
// This utility produces a strip chart on the console of the location of
// each of the modeled elevators, their door statuses, elevator commands sent,
// and alerts when elevator behavior doesn't match that of the EVT model.
//
// One line is produced each time this routine is called.
//
// A sample might look like:
//
// ------ --- --- --- --- --- --- --- --- --- ---------------//- -------- ------
// <Secs> <1> <2> <3> <4> <5> <6> <7> <8> <9> <--- Commmands //> <Alerts> <Pnts>
// ------ --- --- --- --- --- --- --- --- --- ---------------//- -------- ------
//     1  LO^ L   L   L   L   L   L   L   L   F3U CB4 C46    //
//     2  LO^ L   L   L   L   L   L   L   L                  //
//     3  LO^ L   L   B ^ L   2   L   L   L                  //  A          1000
//     4  LO^ L   L   B|^ L   2   L   L   L                  //
//     5  LO^ L   L   B-^ L   3   L   L   L                  //
//     6  LO^ L   L   B-^ L   3   L   L   L                  //
//     7  LO^ L   L   BO^ L   4   L   L   L                  //
//              :          :          :
// Each of the modeled elevators is coded in a column as 'ydi', where:
//    'y' : current floor (or most recent floor, if moving)
//    'd' : door status (' ' = locked (and closed),
//                       '|' = unlocked and closed,
//                       '-' = opening or closing,
//                       'O' = fully open,
//                       '*' = obstructed while closing, and
//                       'H' = open door button held by passenger)
//    'i' : shaft indicator direction
//
// Commands show when a car button is pressed (e.g., 'CB4' indicates a stop
// request for the Basement in car 4) or when a floor stop request has been
// made (e.g., 'F3U' indicates that the Up button on floor 3 has been pressed).
//
// If the log instrumentation ("li") command line parameter is specified,
// the log file has an extended width in order to display differences between
// the modeled and observed elevator behavior.  When differences occur,
// the differing observed elevator status is shown on the right in the extended
// line.
//
// Details for each alert appear in the .TST log file.
//
// In this example, each output line represents 1.0 seconds.
//

void  DoShowEvt(shaftStatus  *modeledElevatorStatus,
                 shaftStatus  *elevatorStatus,
                 unsigned int theTime,
                 char         *commandsIssued,
                 unsigned int alerts,
                 unsigned int points,
                 bool         testIsQuiescent)
{
   const int TIME_POSITION            =  0;
   const int SHAFT_1_POSITION         =  7;
   const int SHAFT_DISPLAY_WIDTH      =  4; // includes trailing space
   const int COMMAND_POSITION         = 43;
   const int MAX_COMMAND_LENGTH       = 20; // bytes
   const int ALERT_POSITION           = 64;
   const int POINTS                   = 73;
   const int LI_SHAFT_1_POSITION      = INSTRUMENTATION_POSITION;
   const int PREVIOUS_STATUS_LENGTH   = MAX_NUMBER_OF_ELEVATORS
                                                          * SHAFT_DISPLAY_WIDTH;

   static unsigned int startingSecond;
   static char         previousStatus[PREVIOUS_STATUS_LENGTH]; // init 0's

   if (firstShowCall) {
      startingSecond = theTime / 1000;
   }

   // Clear the output line, including the extended area for optional
   // instrumentation.
   memset(line, ' ', CHART_INSTRUMENTED_LINE_LENGTH - 1);
   line[CHART_INSTRUMENTED_LINE_LENGTH - 1] = '\0';

   // Indicate the time (in seconds).
   char str[10]; // with ample space for nnnnnn
   sprintf(str, "%5i", theTime / 1000 - startingSecond);
   memcpy(line + TIME_POSITION, str, 5);

   // Indicate elevator status for our modeled shafts.
   int  linePosition;
   char doorChar;
   char indicatorChar;
   for (int i = 0; i < programmedNumberOfElevatorShafts; i++) {
      linePosition = SHAFT_DISPLAY_WIDTH * i + SHAFT_1_POSITION;
      line[linePosition] = modeledElevatorStatus[i + 1].floorLocation;
      switch (modeledElevatorStatus[i + 1].status) {
         case doorLocked:
            doorChar = ' ';
            break;

         case doorClosed:
            doorChar = '|';
            break;

         case doorAjar:
            doorChar = '-';
            break;

         case doorFullyOpen:
            doorChar = 'O';
            break;

         case doorBlocked:
            doorChar = '*';
            break;

         case doorHeld:
            doorChar = 'H';
            break;

         default:
            doorChar = '?';
      }
      line[linePosition + 1] = doorChar;
      switch (modeledElevatorStatus[i + 1].indicator) {
         case up:
            indicatorChar = '^';
            break;

         case down:
            indicatorChar = 'v';
            break;

         case none:
            indicatorChar = ' ';
            break;

         default:
            indicatorChar = '?';
      }
      line[linePosition + 2] = indicatorChar;
   }

   // Indicate commands issued.
   if (strlen(commandsIssued) > MAX_COMMAND_LENGTH) {
      strncpy(line + COMMAND_POSITION, commandsIssued, MAX_COMMAND_LENGTH);
      line[COMMAND_POSITION + MAX_COMMAND_LENGTH] = '>';// indicate truncation
   } else {
      strncpy(line + COMMAND_POSITION, commandsIssued,
               strlen(commandsIssued));
   }

   // Show points (i.e., delta score for this line) if non-zero.
   if (points) {
      sprintf(str, "%6i", points);
      memcpy(line + POINTS, str, 6);
   }

   // Indicate if an ALERT was indicated. The alert character
   // is tied to the alert message.
   if (alerts > 1) {
      char aStr[MAX_ALERT_MESSAGES + 1];
      char *pStr = aStr;
      if (!(alerts % 2)) {       // alertA
         *pStr++ = 'A';
      }
      if (!(alerts % 3)) {       // alertB
         *pStr++ = 'B';
      }
      if (!(alerts % 5)) {       // alertC
         *pStr++ = 'C';
      }
      if (!(alerts % 7)) {       // alertD
         *pStr++ = 'D';
      }
      if (!(alerts % 11)) {      // alertE
         *pStr++ = 'E';
      }
      if (!(alerts % 13)) {      // alertF
         *pStr++ = 'F';
      }
      if (!(alerts % 17)) {      // alertG
         *pStr++ = 'G';
      }
      if (!(alerts % 19)) {      // alertH
         *pStr++ = 'H';
      }
      if (!(alerts % 23)) {      // alertI
         *pStr++ = 'I';
      }
      if (!(alerts % 29)) {      // alertJ
         *pStr++ = 'J';
      }
      if (!(alerts % 31)) {      // alertK
         *pStr++ = 'K';
      }
      if (!(alerts % 37)) {      // alertL
         *pStr++ = 'L';
      }
      if (!(alerts % 41)) {      // alertM
         *pStr++ = 'M';
      }
      if (!(alerts % 43)) {      // alertN
         *pStr++ = 'N';
      }
      if (!(alerts % 47)) {      // alertO
         *pStr++ = 'O';
      }
      if (!(alerts % 53)) {      // alertP
         *pStr++ = 'P';
      }
      if (!(alerts % 59)) {      // alertQ
         *pStr++ = 'Q';
      }
      if (!(alerts % 61)) {      // alertR
         *pStr++ = 'R';
      }
      if (!(alerts % 67)) {      // alertS
         *pStr++ = 'S';
      }
      if (!(alerts % 101)) {     // alertZ
         *pStr++ = 'Z';
      }

      // End with a blank space.
      *pStr++ = ' ';

      strncpy(line + ALERT_POSITION, aStr, pStr - aStr);
   }

   if (logInstrumentation) {
      for (int i = 0; i < programmedNumberOfElevatorShafts; i++) {
         linePosition = SHAFT_DISPLAY_WIDTH * i + LI_SHAFT_1_POSITION;
         if (modeledElevatorStatus[i + 1].floorLocation
                                     != elevatorStatus[i + 1].floorLocation) {
            line[linePosition] = elevatorStatus[i + 1].floorLocation;
         }
         if (modeledElevatorStatus[i + 1].status
                                            != elevatorStatus[i + 1].status) {
            switch (elevatorStatus[i + 1].status) {
               case doorClosed:
                  doorChar = '|';
                  break;

               case doorLocked:
                  doorChar = ' ';
                  break;

               case doorAjar:
                  doorChar = '-';
                  break;

               case doorFullyOpen:
                  doorChar = 'O';
                  break;

               case doorBlocked:
                  doorChar = '*';
                  break;

               case doorHeld:
                  doorChar = 'H';
                  break;

               default:
                  doorChar = '?';
            }
            line[linePosition + 1] = doorChar;
         }
         if (modeledElevatorStatus[i + 1].indicator
                                         != elevatorStatus[i + 1].indicator) {
            switch (elevatorStatus[i + 1].indicator) {
               case up:
                  indicatorChar = '^';
                  break;

               case down:
                  indicatorChar = 'v';
                  break;

               case none:
                  indicatorChar = ' ';
                  break;

               default:
                  indicatorChar = '?';
            }
            line[linePosition + 2] = indicatorChar;
         }
      }
      line[linePosition + 3] = '\0';
   } else {
      line[CHART_LINE_LENGTH - 1] = '\0';
   }

   // Now log the line.
   rrt.Log(line);

   // Display the line without any extended instrumentation.
   line[CHART_LINE_LENGTH - 1] = '\0';
   cout << line << endl;

   // Indicate status changes to Stim_CarButtons if the test is quiescent.
   if (testIsQuiescent) {
      if (memcmp(line + SHAFT_1_POSITION, previousStatus,
                   PREVIOUS_STATUS_LENGTH)) {
         // The status is changed from the previous call.
         SendEventToId(SHOW_CHANGED, ' ', ' ', &stimCarButtonsModelId,
                        MODEL_INTERFACE);
      } else {
         // If any doors aren't closed, then the status is "changed."
         bool anyDoorsNotClosed = false; // assume all closed
         char *pc = line + SHAFT_1_POSITION + 1; // door status position
         char closedIndicator = doorIsClosed[1];
         for (int i = 0; i < programmedNumberOfElevatorShafts; i++) {
            if (*pc != closedIndicator) {
               anyDoorsNotClosed = true;
               break;
            }
            pc += SHAFT_DISPLAY_WIDTH;
         }
         if (anyDoorsNotClosed) {
            SendEventToId(SHOW_CHANGED, ' ', ' ', &stimCarButtonsModelId,
                           MODEL_INTERFACE);
         } else {
            // All doors are closed and the status is unchanged from
            // the previous call.
            SendEventToId(SHOW_UNCHANGED, ' ', ' ', &stimCarButtonsModelId,
                           MODEL_INTERFACE);
         }
      }

      // Save the status for next time.
      memcpy(previousStatus, line + SHAFT_1_POSITION, PREVIOUS_STATUS_LENGTH);
   }
   // Indicate we've been here once.
   firstShowCall = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// InitializeShowEvt
//
// This utility initializes the test's presentation component. It should be
// called prior to any calls to DoShowEvt.
//

bool  InitializeShowEvt(int numberOfShafts)
{
// **??** TBD use numberOfShafts to limit the log. **??**

   bool brc = true; // assume success
   const char line1[] =
           "------ --- --- -- M o d e l e d -- --- --- -------------------- "
           "-------- ------  --- --- - O b s e r v e d - --- ---";
   strcpy(line, line1);
   if (!logInstrumentation) line[INSTRUMENTATION_POSITION] = '\0';
   rrt.Log(line);
   line[CHART_LINE_LENGTH - 1] = '\0';
   cout << endl << line << endl;
   const char line2[] =
           "<Secs> <1> <2> <3> <4> <5> <6> <7> <8> <9> <--- Commmands ----> "
           "<Alerts> <Pnts>  <1> <2> <3> <4> <5> <6> <7> <8> <9>";
   strcpy(line, line2);
   if (!logInstrumentation) line[INSTRUMENTATION_POSITION] = '\0';
   rrt.Log(line);
   line[CHART_LINE_LENGTH - 1] = '\0';
   cout << line << endl;
   const char line3[] =
           "------ --- --- --- --- --- --- --- --- --- -------------------- "
           "-------- ------  --- --- --- --- --- --- --- --- ---";
   strcpy(line, line3);
   if (!logInstrumentation) line[INSTRUMENTATION_POSITION] = '\0';
   rrt.Log(line);
   line[CHART_LINE_LENGTH - 1] = '\0';
   cout << line << endl;

   // Initialize some variables for the strip chart.
   firstShowCall       = true;

   return brc;
}

////////////////////////////////////////////////////////////////////////////////
//
// main
//

int   main (int argc, char *argv[])
{
   char str[128]; // ample

   // Show help text if requested.
   for (int i = 1; i < argc; i++) {
      if (!strcmp(strupr(argv[i]), "?")
                || !strcmp(argv[i], "-?")
                || !strcmp(argv[i], "-H")) {
         cout << "\r\nEVT is a verification test of the Elevator simulator."
                 "\r\n\r\n";
         cout << "Syntax:" << endl << endl;
         cout << "  EVT [test#] [li]"
              << endl << endl;
         cout << "  with optional parameters:" << endl;
         cout << "     test#     is the Test Number for seeding randomization"
              << endl;
         cout << "     li        is 'li' to log instrumentation"        << endl;

         exit(411);
      }
   }

   // Get the command line parameters, if any.
                logInstrumentation  = false; // default
   unsigned int testNumber          = -1;    // invalid
   for (int i = 1; i < argc; i++) {
      if (!strncmp(strupr(argv[i]), "LI", 2)) {
         logInstrumentation = true;
      } else if (atoi(argv[i])) {
         testNumber = atoi(argv[i]);
      } else {
         sprintf(str, "Unrecognized parameter specified: '%s'.  Aborting...",
                  argv[i]);
         alert(str);
         exit(fatalDiscrepancyFound + 999);
      }
   }

   // If the test number was not on the command line, we'll make one up.
   if (testNumber == -1) {
      // Make a somewhat random test number < 10000.
      testNumber = (int)time(NULL) % 10000;
   }

   // Initialize random number cycle.
   RRandom::SetTestNumber(testNumber);

   // Start the test.
   if (rrt.StartTest() != SUCCESS) {
      // A message was already issued.
      exit(1);
   }

   // Establish command and status pipes to server Elevator simulation process.
   commandPipeFd = open(ElevatorCommandsPipe, O_WRONLY | O_NONBLOCK);
   if (commandPipeFd == -1) {
      const int eStrLen = 80;
      char      str[eStrLen + 80]; // ample
      char      eStr[eStrLen]; // should be ample
      strerror_r(errno, eStr, eStrLen);
      sprintf(str, "Unable to open the ElevatorCommandPipe: %s", eStr);
      alert(str);
      rrt.AbortTest();
      exit(1);
   }
   statusPipeFd  = open(ElevatorStatusPipe,   O_RDONLY | O_NONBLOCK);
   if (statusPipeFd == -1) {
      const int eStrLen = 80;
      char      str[eStrLen + 80]; // ample
      char      eStr[eStrLen]; // should be ample
      strerror_r(errno, eStr, eStrLen);
      sprintf(str, "Unable to open the ElevatorStatusPipe: %s", eStr);
      alert(str);
      rrt.AbortTest();
      exit(1);
   }

   pipeOpIsSuccessful = true;
   ElevatorMessage("Sending hello to Elevator",
                    write(commandPipeFd, helloMessage,
                           strlen(helloMessage)));
   if (!pipeOpIsSuccessful) {
      const int eStrLen = 80;
      char      str[eStrLen + 80]; // ample
      char      eStr[eStrLen]; // should be ample
      strerror_r(errno, eStr, eStrLen);
      sprintf(str, "Unable to send hello to Elevator: %s", eStr);
      alert(str);
      rrt.AbortTest();
      exit(1);
   }

   // Put the transaction we're sending in our pipe trace.
   if (pt.MessageSent((char *)helloMessage) != SUCCESS) {
      // Unable to log our hello message.
      sprintf(str, "Unable to log 'hello' message: '%s'.", helloMessage);
      alert(str);
      rrt.AbortTest();
      exit(1);
   }

   sleepMs(pipeWakeupDelay);
   int numRead = 0; // invalid default
   static char buf[MAX_BUF_SIZE];
   sut_name_and_version[0] = '\0'; // initialized to null string
   ElevatorMessage("Reading response to hello message",
                    numRead = read(statusPipeFd, buf, MAX_BUF_SIZE));
   if (numRead == -1 && errno == EAGAIN) {
      // No data was available.
      sprintf(str, "No response to 'hello' message.");
      alert(str);
      rrt.AbortTest();
      exit(1);
   } else if (numRead > 0) {
      buf[numRead] = '\0';
      char *pc = strchr(buf, '\n');
      if (pc != NULL) {
         // Put a nul char in place of the newline char.
         *pc = '\0';
      }

      // Put the received transaction in our pipe trace.
      if (pt.MessageReceived(buf) != SUCCESS) {
         // Unable to log response to our hello message.
         sprintf(str, "Unable to log response to 'hello' message. "
                  "Response: '%s'.", buf);
         alert(str);
         rrt.AbortTest();
         exit(1);
      }

      if (buf[0] == P_OK[0] && buf[1] == P_OK[1]) {
         // The SUT name and version follows 'OK' in the buffer.
         strncpy(sut_name_and_version, buf + 2, MAX_NAME_AND_VERSION_LEN - 1);
         sut_name_and_version[MAX_NAME_AND_VERSION_LEN - 1] = '\0';
      } else {
         // This is not the expected response to our hello message.
         sprintf(str, "Unexpected response (length is %zi)"
                      " to 'hello' message: '%s'.", strlen(buf), buf);
         alert(str);
         rrt.AbortTest();
         exit(1);
      }
   } else if (!pipeOpIsSuccessful) {
      const int eStrLen = 80;
      char      str[eStrLen + 80]; // ample
      char      eStr[eStrLen]; // should be ample
      strerror_r(errno, eStr, eStrLen);
      sprintf(str, "Unable to establish communications with Elevator: %s",
               eStr);
      alert(str);
      rrt.AbortTest();
      exit(1);
   }

   // Announce our test.
   sprintf(str, "Elevator Verification Test (EVT) with Test Number %i",
            testNumber);
   cout << str << endl;
   rrt.Log(str);

   if (argc > 1) {
      // Show chosen command line parameters.
      strcpy(str, "Test Run Options: ");
      bool firstTime = true;
      for (int i = 1; i < argc; i++) {
         if (firstTime) {
            firstTime = false;
         } else {
            strcat(str, ", ");
         }
         strcat(str, strlwr(argv[i]));
      }
      strcat(str, ".");
      rrt.Log(str);
      cout << str << endl;
   }

   // Show some of the test settings.
   sprintf(str, "  SUT : %s", sut_name_and_version);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Test Run Time (seconds) : %i", TEST_LENGTH_SECONDS);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Test Quiesce Time (seconds): %i", QUIESCE_TIME_SECONDS);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Car Location Timeout (ms) : %i",compareCarLocationsTimeout);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Door Status Timeout (ms) : %i",compareDoorStatusesTimeout);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Dir Indicators Timeout (ms) : %i",
                                                  compareDirIndicatorsTimeout);
   rrt.Log(str);
   cout << str << endl;
   sprintf(str, "  Test Failure Score: >= %i", TEST_FAILURE_SCORE);
   rrt.Log(str);
   cout << str << endl;

   // Initialize some test variables.
   alertIssued         = 1;    // none
   alertMessageIndex   = 0;
   exitRequestedByUser = false;
   score               = 0;
   testing             = true;

   // Create our initial configuration state machine.
   StartEVT_Configuration();

   // Start our test model execution.
   cobegin(4,                                    // initial coroutine count
              RoundtripCounter,                0, // no parameters
              DeviceInfoThread,                0, // no parameters
              ModelThread,                     4, //  4 parameters
                 &theEventQ,  &theEventSelfDirectedQ,
                 theModel, "EVT",
              UserInterface,                   0  // no parameters
         );

   // Delete any active state machines.
   if (currentNumberOfModelInstances > 0) {
      for (int i = currentNumberOfModelInstances - 1; i >= 0; i--) {
         delete theModel[i];
         theModel[i] = 0;
      }
   }
   currentNumberOfModelInstances = 0;

   // Delete our pipe descriptors.
   close(commandPipeFd);
   close(statusPipeFd);

   // Show the coroutine roundtrip count and average time per cycle.
   cout << endl << RoundtripCounterOutputString << endl;
   rrt.Log(RoundtripCounterOutputString);

   #ifdef SHOW_HISTOGRAM
   itHistCR.show(true); // include log copy of coroutine roundtrip histogram
   itHistCR.reset();      // clear histogram for next time
//   iatHist.show(true);  // include log copy of inter-arrival histogram
//   iatHist.reset();       // clear histogram for next time
   #endif // def SHOW_HISTOGRAM

   // Log encountered requirements for the test.
   rth.ToLog();

   // Add requirements encountered in this test run to the consolidated
   // ElevatorRequirements.csv file.
   rth.ToFile();

   // Write out transactions to / from the Elevator simulator for this test.
   pt.ToFile();

   // Get our log sequence number before ending the test.
   unsigned int sequenceNumber = rrt.GetSequenceNumber();

   int exitValue = 0; // assume success (a perfect score!)
   if (rrt.GetErrorStatus() == 0) {
      if (exitRequestedByUser) {
         exitValue = USER_ABORT_VALUE;
      } else {
         if (score < TEST_FAILURE_SCORE) {
            char str[SAFE_SIZE];
            sprintf(str, "Test %i passed.", testNumber);
            cout << str << endl;
            rrt.Log(str);
         } else {
            char str[SAFE_SIZE];
            sprintf(str, "Test %i failed.", testNumber);
            cout << str << endl;
            rrt.Log(str);
         }
      }
      rrt.Log("Test finished.");
      rrt.FinishTest();
   } else {
      cout << "Failed, error code: " << rrt.GetErrorStatus() << endl;
      char str[80]; // ample
      sprintf(str, "Test %i failed (RRT error code: %i).", testNumber,
               rrt.GetErrorStatus());
      rrt.Log(str);
      rrt.AbortTest();
      exitValue = FATAL_ABORT_VALUE;
   }

   ChronicleTestRun(TEST_RUNS_CSV_FILE, sut_name_and_version,
                     "Getz", testNumber, sequenceNumber, score,
                     score < TEST_FAILURE_SCORE, alert_list);

   return exitValue;
}

////////////////////////////////////////////////////////////////////////////////
//
// ModelInfo
//
// This routine receives status notification messages from the model and
// sends appropriate events to the Comparator component.
//
// The input message is written to the "pipe transaction log" with its
// timestamp for comparison with corresponding messages from the elevator
// simulator.
//

bool  ModelInfo(char *pMsg)
{
   bool brc = true; // assume success

   // Put the message in our pipe trace, indicating that it's from the model.
   if (pt.MessageReceived(pMsg, true) != SUCCESS) {
      brc = false;
   } else {
      //////////////////////////////// @yx ///////////////////////////////

      if (pMsg[0] == carLocation[0]
             && strlen(pMsg) == strlen(carLocation)) {
         // pMsg is <"@"><floor><shaft>.
         SendEventToId(MODELED_CAR_LOCATION, pMsg[1], // floor
                        pMsg[2],                       // car
                        &comparatorModelId, MODEL_INTERFACE);
      }

      //////////////////////////////// %sx ///////////////////////////////

      else if (pMsg[0] == doorIsOpen[0] // surrogate
             && strlen(pMsg) == strlen(doorIsOpen)) {
         // pMsg is <"%"><doorStatus><shaft>.
         SendEventToId(MODELED_DOOR_STATUS, pMsg[1],  // door status
                        pMsg[2],                       // car
                        &comparatorModelId, MODEL_INTERFACE);
      }

      //////////////////////////////// ~dx ///////////////////////////////

      else if (pMsg[0] == indicatorIsUp[0] // surrogate
             && strlen(pMsg) == strlen(indicatorIsUp)) {
         // pMsg is <"~"><dir indicator><shaft>.
         SendEventToId(MODELED_DIR_INDICATOR, pMsg[1], // dir indicator
                        pMsg[2],                        // car
                        &comparatorModelId, MODEL_INTERFACE);
      }

      /////////////////////////////// ????? //////////////////////////////

      else {
         char str[80]; // ample
         sprintf(str, "Unknown notification (length is %zi)"
                       " in ModelInfo: '%s'",
                  strlen(pMsg), pMsg);
         alert(str);
         score += fatalDiscrepancyFound;
         brc = false;
      }
   }

   return brc;
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTGetErrorStatus
//
// This utility gets the current test error status.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: The current test error status.
//

int  RRTGetErrorStatus(void)
{
   return rrt.GetErrorStatus();
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTGetLogfileHeader
//
// This utility gets the current logfile header string for this test.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: a string pointer to the logfile header string.
//

char *RRTGetLogfileHeader(void)
{
   return rrt.GetLogfileHeader();
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTGetLogfileDirectory
//
// This utility gets the current logfile directory for this test.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: a string pointer to the logfile directory path, including
//          trailing '/' char.
//

char *RRTGetLogfileDirectory(void)
{
   return rrt.GetLogfileDirectory();
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTGetSequenceNumber
//
// This utility gets the current sequential log number for this test.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: 0 and -1 are invalid sequence numbers, and show that the test
// was not initialized properly.
//

int  RRTGetSequenceNumber(void)
{
   return rrt.GetSequenceNumber();
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTGetTime
//
// This utility gets the current test time (in ms).
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: The number of ms since the test began, or 0 if not in a test.
//

unsigned int RRTGetTime(void)
{
   unsigned int rc;

   if (rrt.IsTestInProgress()) {
      rc = rrt.GetTime();
   } else {
      rc = 0;
   }
   return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTIsTestInProgress
//
// This utility determines if the test is currently in progress.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: None
//
// Returns: bool indicating if the test is in progress
//

bool  RRTIsTestInProgress(void)
{
   return rrt.IsTestInProgress();
}

////////////////////////////////////////////////////////////////////////////////
//
// RRTLog
//
// This utility sends a message to test log file.
//
// This routine provides a general interface for elements of the RRTGen module.
//
// Inputs: message (char string)
//
// Returns: None
//

void  RRTLog(const char *message)
{
   if (rrt.IsTestInProgress()) {
      rrt.Log(message);
   }
}

////////////////////////////////// Coroutine ///////////////////////////////////
//
// UserInterface
//
// Provides the user interface:
//
//    'q' quits the test.
//

void  UserInterface(void)
{
   char c;

   rrt.Log("UserInterface started.");

   while (testing) {
      if (kbhit()) {
         c = getchar();
         switch (c) {
            case 'q':
               {
                  ElevatorMessage("User requesting quit",
                                   write(commandPipeFd, userRequestedQuitTest,
                                          strlen(userRequestedQuitTest)));

                  // Put the transaction we're sending in our pipe trace.
                  if (pt.MessageSent((char *)userRequestedQuitTest)
                                                                 != SUCCESS) {
                     // Unable to log our userRequestedQuitTest message.
                     char str[80]; // ample
                     sprintf(str, "Unable to log userRequestedQuitTest message"
                              ": '%s'.", userRequestedQuitTest);
                     alert(str);
                  }
                  exitRequestedByUser = true;
               }
               break;
         }
      }
      coresume();
   }

   rrt.Log("UserInterface ended.");
}

