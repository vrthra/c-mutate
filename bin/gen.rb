#!/usr/bin/env ruby
require 'fileutils'

src = ARGV[0]
$srclines =  File.readlines(src).map(&:chomp).freeze

$name = File.basename(src)
myname = "build/#{$name}/"
FileUtils.mkdir_p "#{myname}/mutants"
descs = "#{myname}/mutants.txt"

def mutate(mut_line_no, mut_line)
  $srclines.map.with_index(1) do |line, idx|
    if idx == mut_line_no
      mut_line
    else
      line
    end
  end
end

File.readlines(descs).each.with_index(1) do |mutated_line_descr, idx|
  case mutated_line_descr
  when /^([^:]*):(.*)/
    mymutant = "build/#{$name}/mutants/#{idx}_#{$1.strip}_#{$name}"
    File.open(mymutant, 'w+') do |f|
      f.puts mutate($1.to_i, $2)
    end
  else
    raise "Error #{idx}."
  end
end

