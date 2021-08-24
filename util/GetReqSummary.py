# GetReqSummary.py -- Prepares a summary report from an RRTGen requirements
#                     encountered / met .csv file.
#                                                          <by Cary WR Campbell>
#
################################ Copyright Notice ##############################
#
# This software is copyrighted ? 2007 - 2015 by Codecraft, Inc.
#
# The following terms apply to all files associated with the software
# unless explicitly disclaimed in individual files.
#
# The authors hereby grant permission to use, copy, modify, distribute,
# and license this software and its documentation for any purpose, provided
# that existing copyright notices are retained in all copies and that this
# notice is included verbatim in any distributions. No written agreement,
# license, or royalty fee is required for any of the authorized uses.
# Modifications to this software may be copyrighted by their authors
# and need not follow the licensing terms described here, provided that
# the new terms are clearly indicated on the first page of each file where
# they apply.
#
# IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
# DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
# IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
# NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
# MODIFICATIONS.
#
# GOVERNMENT USE: If you are acquiring this software on behalf of the
# U.S. government, the Government shall have only "Restricted Rights"
# in the software and related documentation as defined in the Federal
# Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
# are acquiring the software on behalf of the Department of Defense, the
# software shall be classified as "Commercial Computer Software" and the
# Government shall have only "Restricted Rights" as defined in Clause
# 252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
# authors grant the U.S. Government and others acting in its behalf
# permission to use and distribute the software in accordance with the
# terms specified in this license.
#
################################################################################
#
# Scans an RRTGen .csv file that contains requirements encountered / met data
# for multiple test runs and prepares a summary report.  A .csv summary file is
# produced.  An output file path and name may be optionally provided.
#
# The syntax is:  python GetReqSummary.py input_file [output_file]
#
# The input data is formatted as follows:
#
#   Requirement ID,Met,Encountered
#    1716,  0,  9
#    1717,  9,  9
#    1718,  3,  3
#      :   :   :
#

# NB: This constant needs to be updated with eash new f/w release.
TOTAL_REQ_COUNT = 12 # as of 2/21/2007

scriptVersion  = "Version 1.0"

defaultOutputFileName = "ReqSummary.csv"

fileSeparatorForWindows = '\\'
fileSeparatorForUnix    = '/'

# We're running on Unix
fileSeparator = fileSeparatorForUnix

################################################################################
#
# DoGetReqSummary()
#

def DoGetReqSummary():
   from locale import atoi, atof
   import string
   import sys
   import time
   import os

   try:

      ##
      ## Handle command line parameters (if any).
      ##

      verboseMode = False
      helpRequested = False
      outputFile = defaultOutputFileName
      argCount = len( sys.argv[1:] )

      # Check args.
      if argCount == 0:
         # There is just the command.  We need at least an input file.
         print "\nInput requirements .csv file not specified."
         sys.exit(911)
      else:
         # There are some args.
         gotSource = False # initial value
         for arg in sys.argv[1:]:
            if arg == '?' or arg == '-?' or arg == '-h' or arg == '-H':
               helpRequested = True
            elif arg == "-v" or arg == "-V":
              verboseMode = True
            elif gotSource == False:
               gotSource = True
               sourceFile = arg
            else: # it's the target file
               outputFile = arg

      if not helpRequested and not gotSource:
         print "\nInput requirements .csv file not specified."
         sys.exit(911)

      if helpRequested:
         print "\nGetReqSummary.py produces a summary report from\n"           \
             + "requirements encountered / met data for multiple test runs.\n" \
             + "\nSyntax:\n"                                                   \
             + "\n   python getreqsummary.py [-v] source [target]\n"           \
             + "\nwhere:\n"                                                    \
             + "   -v       displays additional execution information,\n"      \
             + "   source   is the input .csv file path+name\n"                \
             + "   target   (optional) file path+name for the summary file.\n"
         sys.exit( 411 )

      ##
      ## Open input file.
      ##

      if verboseMode:
         print "\nOpening input file:", sourceFile
      In = open( sourceFile, 'r' )

      ##
      ## Open output file.
      ##

      if verboseMode:
         print "Opening output file:", outputFile
      Out = open( outputFile, 'w' )

      ##
      ## Scan the input file.
      ##

      if verboseMode:
         print "Processing %s." % sourceFile

      rc = 0 # assume success
      Out.write("#  This file was created by GetReqSummary.py %s from:\n" \
                                                                % scriptVersion)
      if sourceFile[0] == fileSeparator or sourceFile[1] == ':':
         Out.write("#  %s\n" % sourceFile)
      else:
         Out.write("#  %s\n" % (os.getcwd() + fileSeparator + sourceFile))
      Out.write("#\n#  %s\n#\n" % time.ctime() )

      summaryData = {}
      firstLine = True
      for line in In:
         if firstLine:
            firstLine = False
         else:
            #if verboseMode:
            #   print line[:-1]
            data = line[:-1].split(',')
            if len(data) == 3 and type(atoi(data[0])) == type(123)  \
                              and type(atoi(data[1])) == type(123)  \
                              and type(atoi(data[2])) == type(123):
               requirement = atoi(data[0])
               met         = atoi(data[1])
               encountered = atoi(data[2])
               if summaryData.has_key(requirement):
                  summaryData[requirement] = [summaryData[requirement][0]+met, \
                                              summaryData[requirement][1]      \
                                                                 + encountered]
               else:
                  summaryData[requirement] = [met,encountered]

      sortedData = summaryData.items()
      sortedData.sort()

      if verboseMode:
         print "Number of items in summaryData is %i. " % (len(summaryData))
         print "summaryData:"
         for item in sortedData:
            print item

      ##
      ## Close input file.
      ##

      if verboseMode:
         print "Closing input file."
      In.close()

      ##
      ## Write out the summary data to the console.
      ##

      print "\nRequirements covered by this test run (met/encountered):"
      indent = " "
      line = indent
      j = 0
      requirementsMet = 0
      for key, value in sortedData:
         if value[0]:
            requirementsMet += 1
         line += "%5i (%5i/%5i) " % (key, value[0], value[1])
         j += 1
         if j >= 4:
            print line
            line = indent
            j = 0
      if j > 0:
         print line

      print "\nSummary 'requirements met' coverage: %i of %i (%i percent)." \
             % (requirementsMet, TOTAL_REQ_COUNT,                       \
             int( 100.0 * requirementsMet / TOTAL_REQ_COUNT + 0.5))

      print " " # blank line

      ##
      ## Write out the summary .csv file.
      ##

      Out.write("Requirement ID,Met,Encountered\n")

      for key, value in sortedData:
         Out.write("%i,%i,%i\n" % (key, value[0], value[1]))

      ##
      ## Close output file.
      ##

      if verboseMode:
         print "Closing output file."
      Out.close()

   except:

      print "\nGetReqSummary aborted."
      rc = 3

   ##
   ## Return a result.
   ##

   if verboseMode:
      if rc == 0:
         print "\nNo errors found."
      else:
         print "\nReturn code: %d" % rc

   sys.exit( rc )


# Now do the check (if we're running from the command line).
if __name__ == '__main__':
   DoGetReqSummary()

