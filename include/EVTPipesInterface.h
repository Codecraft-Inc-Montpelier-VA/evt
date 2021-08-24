////////////////////////////////////////////////////////////////////////////////
//
//  EVTPipesInterface.h -- a header file to define pipes for the Elevator test.
//
//                                                        <by Cary WR Campbell>
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

#define ElevatorCommandsPipe        "/tmp/ElevatorCommandsPipe"
#define ElevatorStatusPipe          "/tmp/ElevatorStatusPipe"

const int  MAX_BUF_SIZE             =  255;
const int  MAX_NAME_AND_VERSION_LEN =   40;
const int  pipeWakeupDelay          = 1000; // ms

// Pipe commands:
// f=float, i=integer, s=string, y=#floors, x=#elevators, g=grndFlr
// Note: lengths are in meters, times in milliseconds, velocity in meters/second.
extern const char endConfiguration[];
extern const char helloMessage[];
extern const char queryFloorLabels[];
extern const char queryMaxDimensions[];
extern const char quitTest[];
extern const char setBlockClearTime[];
extern const char setDoorOpenCloseTime[];
extern const char setFloorHeight[];
extern const char setFloorLabels[];
extern const char setGroundFloorLevel[];
extern const char setMaxCabinVelocity[];
extern const char setMaxCloseAttempts[];
extern const char setMinStoppingDistance[];
extern const char setNormalDoorWaitTime[];
extern const char setNumberOfElevators[];
extern const char setNumberOfFloors[];
extern const char userRequestedQuitTest[];

// Test stimulation commands (from EVT -> Elevator) with no response:
extern const char obstructDoor[];
extern const char pushCarButton[];
extern const char pushFloorButton[];

// Pipe statuses:
extern const char P_BAD[];
extern const char P_OK[];
extern const char P_END[];

// Notifications:
extern const char carLocation[];
extern const char doorIsClosed[];
extern const char doorIsLocked[];
extern const char doorIsAjar[];
extern const char doorIsOpen[];
extern const char indicatorIsDown[];
extern const char indicatorIsUp[];
extern const char indicatorIsOff[];
extern const char stopIsRequested[];
extern const char stopIsCleared[];
extern const char floorCallIsUp[];
extern const char floorCallIsDown[];
extern const char floorUpIsCleared[];
extern const char floorDownIsCleared[];


