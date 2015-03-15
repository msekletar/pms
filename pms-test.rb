#!/usr/bin/env ruby

(1..10).each do |e|
  output_file = "pms.out"
  test_output_file = "test.out"
  input_size = (1 << e)
    
  Kernel.system "./test.sh #{input_size} >  #{output_file}"

  f = File.new(output_file, "r")

  line = f.gets
  numbers = line.split

  f.close

  f = File.new(test_output_file, "w")
  f << line

  tmp = []
  numbers.each do |x|
    tmp << x.to_i
  end

  numbers_sorted = tmp.sort
  numbers_sorted.each do |x|
    f << (x.to_s + "\n")
  end

  f.close

  r = Kernel.system "diff -q #{output_file} #{test_output_file}"
  puts "TEST FAILED with input size #{input_size}" if r == false
    
end
