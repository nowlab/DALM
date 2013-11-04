#!/usr/bin/env ruby
# coding: utf-8

if ARGV.size!=2
	$stderr.puts "Usage: ruby #{__FILE__} arpa-file output-unigram-file"
	exit 1
end

arpafile = ARGV.shift
ufile = ARGV.shift

open(arpafile, "r:utf-8"){|afp|
	open(ufile, "w"){|ufp|
		while afp.gets
			$_.chomp!
			if $_ =~ /^.*?\t.*?(\t.*?|)$/
				prob, words, bow = $_.chomp.split("\t")
				words = words.split
				if words.size == 1
					ufp.puts $_
				else
					break
				end
			end
		end
	}
}

