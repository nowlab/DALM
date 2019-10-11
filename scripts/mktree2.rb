# coding: utf-8

require_relative 'common'

STDIN.set_encoding("ASCII-8BIT")

ngram_prev=[]
while gets
	ngram, value = $_.chomp.split("\t")

	ngram=ngram.split
	min_size = ngram.size < ngram_prev.size ? ngram.size : ngram_prev.size
	branch = min_size
	for i in 1..min_size do
	    index = min_size-i
	    if ngram[index] != ngram_prev[index]
	        branch = index
	    else
	        break
	    end
	end
    common_words = ngram[0...branch]
    for i in branch...ngram.size-1 do
        puts_inserted(common_words + ngram[branch..i])
    end
	if ngram.size > 1 and ngram[-2]=="<#>"
		puts "#{ngram.join(" ")}\t"
		puts "#{ngram[-1]} #{ngram[0...-1].join(" ")} \x01<#>\t#{value}"
	else
		puts "#{ngram.join(" ")}\t#{value}"
	end
	ngram_prev = ngram
end

