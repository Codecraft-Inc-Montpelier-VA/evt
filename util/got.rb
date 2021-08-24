#!/usr/bin/env ruby
# got.rb
#
#  Syntax: ruby got.rb ["optional-parameters-for-elevator_verification_test"]
#
#  This script runs the debug mode version of the elevator verification
#  test (EVT). The test exit code determines our behavior:
#
#       Exit code    Our Action
#       ---------    ----------------------------------------------------------
#              99      Exit requested by user.  Both the current test in EVT
#                      and this script are terminated.
#
#        ... else      Continue running tests.
#
#  If the RRTGen Executive Dashboard (red) website is active, the results of
#  a test run are reported at its completion.  The WEB_URL constant gives the
#  web address of the red web application.
#
#  Created by Cary Campbell on 2007-05-26.
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

require 'optparse'
require 'csv'
require 'net/http'
require 'pp'                                         # **??** TBD Remove! *??**

# This value must match EVT.
COMMAND             = 'evt'
LOG_FILE_PREFIX     = 'RRT'
#DIRECTORY           = 'debug'
DIRECTORY           = 'release'
WEB_URL             = 'http://localhost:3000'
#WEB_URL             = 'https://tranquil-ravine-72173.herokuapp.com/'
URL_PATH            = '/trials'
POST_HASH_NAME      = URL_PATH.slice(1..-1).chomp('s') #=> 'trial'
TEMP_TEST_RUNS_FILE = 'ElevatorTestRuns.csv'
TEMP_FILE_PREFIX    = '__rrt_temp__'
USER_ABORT_VALUE    = 99

class GotArguments < Hash
  def initialize(args)
    super()
    self[:increment_execution] = false
    self[:log_instrumentation] = ''
    self[:loop_execution]      = false
    self[:single_execution]    = false
    self[:test_number]         = 0

    options = OptionParser.new do |opts|
      opts.banner = "Usage: #$0 [options]"

      opts.on('-a', '--additional-information',
                'include additional information in log') do
        self[:log_instrumentation] = 'li'
      end

      opts.on('-l', '--loop-execution TEST_NUMBER',
                'loop execution of TEST_NUMBER') do |test_no|
        self[:loop_execution] = true
        self[:test_number] = test_no.to_i
        self[:test_number] = 0 if self[:test_number].to_s != test_no
      end

      opts.on('-s', '--start-execution TEST_NUMBER',
                'start incremental execution at TEST_NUMBER') do |test_no|
        self[:increment_execution] = true
        self[:test_number] = test_no.to_i
        self[:test_number] = 0 if self[:test_number].to_s != test_no
      end

      opts.on('-x', '--single-execution TEST_NUMBER',
                'single execution of TEST_NUMBER') do |test_no|
        self[:single_execution] = true
        self[:test_number] = test_no.to_i
        self[:test_number] = 0 if self[:test_number].to_s != test_no
      end

      opts.on_tail('-h', '--help', 'display this help and exit') do
        puts opts
        exit
      end
    end

    options.parse(args)
  end
end

arguments = GotArguments.new(ARGV)

# Validating the entered parameters.
arguments_sum = 0
arguments_sum += 1 if arguments[:loop_execution]
arguments_sum += 1 if arguments[:increment_execution]
arguments_sum += 1 if arguments[:single_execution]
if arguments_sum > 1
  puts ''
  puts "Don't use more than one of -x, -l, and -s in the same execution"
  puts ''
  exit
elsif arguments_sum == 0
  puts ''
  puts "Specify one of -x or -l or -s option for execution"
  puts ''
  exit
end
if arguments[:test_number] == 0
  puts ''
  puts "Specify a positive (non-zero) test number for execution"
  puts ''
  exit
end

li          = arguments[:log_instrumentation]
looping     = arguments[:loop_execution]
single_play = arguments[:single_execution]
test_number = arguments[:test_number]

def with_pleasant_exceptions
  begin
    yield
  rescue SystemExit
    raise
  rescue Exception => ex
    $stderr.puts(ex.message)
  end
end

def postit(address, cmd, parms)
puts("postit entry:" )
print("address: ")
puts(address)
print("cmd: ")
puts(cmd)
print("parms: ")
puts(parms)
  url = URI.parse(address)
print("url: ")
puts(url)
  req = Net::HTTP::Post.new(cmd)
  req.set_form_data(parms)
print("req: ")
puts(req)
  #req.add_field 'Accept', 'application/xml'
  res = Net::HTTPResponse.new '1.0', Net::HTTPNotFound, 'OK'
  with_pleasant_exceptions do
    res = Net::HTTP.new(url.host, url.port).start { |http| http.request(req) }
  end
print("res: ")
puts(res)
print("res.code: ")
puts(res.code)
  case res.code
  when 200, 302 # HTTPOK, HTTPFound
    # OK
  else
  #    res.error!
  end
  return res
end

if $0 == __FILE__
  exit_code = 0
  until exit_code == USER_ABORT_VALUE
    puts ''
    puts "================================ [Test #{test_number}] \
================================"
    puts ''
    system("#{DIRECTORY}/#{COMMAND} #{test_number} #{li}")

    # Get the test run results from the temp file (including any that
    # may still be left from previous test runs).  Send red web site the
    # test run results and upload the .log and .ptl logs.
    web_error        = false
    csv_header       = ['']
    to_red_count     = 0
    array_of_rows = CSV.read(TEMP_TEST_RUNS_FILE)
    array_of_rows.each do |row|
      if csv_header[0].length == 0
        csv_header = row
      else
        data =
        {
          'trial[sut]'      => row[0].lines('/')[-1], # omitting paths
          'trial[machine]'  => row[1],
#          'trial[run]'      => COMMAND.upcase + row[2].strip,
          'trial[try_finger]'      => COMMAND.upcase + row[2].strip,
#          'trial[log]'      => LOG_FILE_PREFIX + row[3].strip,
           'trial[log_finger]'      => LOG_FILE_PREFIX + row[3].strip,
          'trial[score]'    => row[4].to_i,
          'trial[verdict]'  => row[5],
          'trial[alerts]'   => row[6],
          'trial[end_time]' => row[7] + " " + row[8]
        }
        res = postit(WEB_URL, URL_PATH, data)
        if res.code == '200' or res.code == '302' # Net::HTTPOK or Net::HTTPFound
print("res['location']: ")
puts(res['location'])
          if res['location'] != nil
             encoded_url = URI.encode(res['location'])
             #          loc = URI::parse(res['location']).path
                       loc = URI::parse(encoded_url).path
          else
            loc = nil
          end
#          puts "Test run #{data['trial[machine]']}:#{data['trial[log]']} of " \
#               "#{data['trial[run]']} successfully added to red as #{loc}."
          puts "Test run #{data['trial[machine]']}:#{data['trial[log_finger]']} of " \
                "#{data['trial[try_finger]']} successfully added to red as #{loc}."
          to_red_count += 1
          if to_red_count == 1
            File.rename(TEMP_TEST_RUNS_FILE, TEMP_FILE_PREFIX \
                                                          + TEMP_TEST_RUNS_FILE)
          end
        elsif res.code == Net::HTTPNotFound
          web_error = true
          puts "red is not active."
        else
          web_error = true
          puts "Code = #{res.code}"
          puts "Message = #{res.message}"
          res.each {|key, val| printf "%-14s = %-40.40s\n", key, val}
          pp res.body
        end
      end
# abort
      break if web_error
    end
    if web_error && to_red_count > 0
      k = 1 # start at the one past the one correctly written to red.
      CSV.open(TEMP_TEST_RUNS_FILE, "w") do |csv|
        if k == 1
          csv << array_of_rows[0] # file header row
        end
        csv << array_of_rows[to_red_count + k]
        k += 1
      end
    end

    # We've sent the run data to red or to a file, so we can safely delete
    # the renamed temp file.
    temp_file = TEMP_FILE_PREFIX + TEMP_TEST_RUNS_FILE
 #   File.delete temp_file if File.exists? temp_file

    test_number += 1 unless looping
    exit_code = $?.exitstatus
    break if single_play
    sleep 2 # allow the elevator simulator to cycle
  end
end
