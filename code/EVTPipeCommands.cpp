/**************************** EVTPipeCommands.cpp ******************************
 *
 * This software is copyrighted © 2007 - 2015 by Codecraft, Inc.
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
 *  This file contains commands and statuses for the Elevator Verification
 *  Test (EVT) and Elevator modules. These strings are shared by the EVT
 *  and Elevator builds.
 *
 *  Also included here are the notifications of significant events occurring in
 *  the Elevator simulation.  These events form one side of inputs used by the
 *  Comparator component of EVT to compare Elevator behavior with the output of
 *  the EVT elevator model.
 */

// Test start and stop commnds (from EVT -> Elevator):
const char helloMessage[]           = "Ola!";//       <= P_OK,sut_name_and_ver.
const char quitTest[]               = "q";   //       <= P_END
const char userRequestedQuitTest[]  = "z";   //       <= P_END

// Test configuration commands (from EVT -> Elevator) with responses
// (from Elevator -> EVT):
// f=float, i=integer, s=string, y=#floors, x=#elevators, g=grndFlr
// Note: lengths are in meters, times in milliseconds, velocity in meters/second.
const char endConfiguration[]       = "!EC"; //       <= EC#OK|EC#BAD
const char queryFloorLabels[]       = "?FL"; //       <= "FLabc.."
const char queryMaxDimensions[]     = "?MD"; //       <= "MD"y,x,*|g
const char setBlockClearTime[]      = "!BC"; // i     <= BC#OK|BC#BAD
const char setDoorOpenCloseTime[]   = "!DO"; // i     <= DO#OK|DO#BAD
const char setFloorHeight[]         = "!FH"; // f     <= FH#OK|FH#BAD
const char setFloorLabels[]         = "!FL"; // s     <= FL#OK|FL#BAD
const char setGroundFloorLevel[]    = "!GF"; // i     <= GF#OK|GF#BAD
const char setMaxCabinVelocity[]    = "!CV"; // f     <= CV#OK|CV#BAD
const char setMaxCloseAttempts[]    = "!CA"; // i     <= CA#OK|CA#BAD
const char setMinStoppingDistance[] = "!MS"; // f     <= MS#OK|MS#BAD
const char setNormalDoorWaitTime[]  = "!DW"; // i     <= DW#OK|DW#BAD
const char setNumberOfElevators[]   = "!NE"; // i     <= NE#OK|NE#BAD
const char setNumberOfFloors[]      = "!NF"; // i     <= NF#OK|NF#BAD

// Test stimulation commands (from EVT -> Elevator) with no response:
const char obstructDoor[]           = "Oax"; // "Hold | release shaft x door."
const char pushCarButton[]          = "Cyx"; // "Push button y in car x."
const char pushFloorButton[]        = "Fyi"; // "Push u or d button on floor y."

// Statuses (from Elevator -> EVT):
const char P_BAD[]                  = "BAD";
const char P_OK[]                   = "OK";
const char P_END[]                  = "-30-";

// Notifications (from Elevator -> EVT):
const char carLocation[]            = "@yx"; // "Car x is at floor y."
const char doorIsClosed[]           = "%|x"; // "Car x's door is closed."
const char doorIsLocked[]           = "% x"; // "Car x's door is locked."
const char doorIsAjar[]             = "%-x"; // "Car x's door is half-way open."
const char doorIsOpen[]             = "%Ox"; // "Car x's door is completely
                                             //    open."
const char indicatorIsDown[]        = "~vx"; // "Car x's indicator is 'down'."
const char indicatorIsUp[]          = "~^x"; // "Car x's indicator is 'up'."
const char indicatorIsOff[]         = "~ x"; // "Car x's indicator is off."
const char stopIsRequested[]        = "+yx"; // "Car x's floor y button is lit."
const char stopIsCleared[]          = ".yx"; // "Car x's floor y button is off."
const char floorCallIsUp[]          = "*^y"; // "Floor y up call button is lit."
const char floorCallIsDown[]        = "*vy"; // "Floor y down call button lit."
const char floorUpIsCleared[]       = "-^y"; // "Floor y up call button is off."
const char floorDownIsCleared[]     = "-vy"; // "Floor y down call button off."


