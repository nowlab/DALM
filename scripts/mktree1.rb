#!/usr/bin/env ruby
# coding: utf-8

STDIN.set_encoding("ASCII-8BIT")

while gets
   	$_.chomp!
   	$_.strip!
   	if $_ == "\\1-grams:"
   		break
 	end
end

puts "<#>\t"
while gets
    $_.chomp!
    next if $_ == ""
    next if $_ =~ /^\\/

    prob, ngram, bow = $_.split("\t")
    prob = prob.to_f
    ngram = ngram.split
    bow = bow.to_f if not bow.nil?

    probpath = ngram[0..-2].reverse
    probpath << "<#>"
    probpath << ngram[-1]
    puts "#{probpath.join(" ")}\t#{prob}"

    if not bow.nil?
        bowpath = ngram.reverse
        puts "#{bowpath.join(" ")}\t"
        puts "#{bowpath.join(" ")} <#>\t#{bow}"
    end
end
