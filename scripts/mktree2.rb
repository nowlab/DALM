#!/usr/bin/env ruby
# coding: utf-8

def puts_inserted(inserted)
	if inserted[-1]=="<#>"
        throw "Irregular ARPA file. Please check your cutoff parameter. B( #{inserted[0...-1].join(" ")} ) is not found, although ARPA has P( * | #{inserted[0...-1].join(" ")} )."
	else
		puts "#{inserted.join(" ")}\t"
	end
end

if __FILE__ == $0
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
end


