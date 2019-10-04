#!/usr/bin/env ruby
# coding: utf-8

if ARGV.size < 1
    $stderr.puts "Usage: ruby #{__FILE__} arpa-file"
end

arpa = ARGV.shift

cnt = 0
open("#{arpa}", "r:ASCII-8BIT"){|fp|
    while fp.gets
    	$_.chomp!
    	$_.strip!
    	if $_ =~ /^ngram *?\d+ *?= *?\d+$/
            cnt += 1
    	elsif $_ == "\\1-grams:"
    		break
    	end
    end
}
puts cnt
