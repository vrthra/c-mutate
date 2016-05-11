#!/usr/bin/env ruby

#        -:    0:Source:programs/wc.c
#        -:    0:Graph:wc.gcno
#        -:    0:Data:wc.gcda
#        -:    0:Runs:1
#        -:    0:Programs:1

File.readlines(ARGV[0]).each do |l|
  case l
  when /^        -:    0:.*$/
    #skip non lines
  when /^        -: *[0-9]:.*$/
    # skip non exec
  when /^ *([0-9]+): *([0-9]+):.*$/
    puts $2.strip
  end
end
