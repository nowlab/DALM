#!/usr/bin/env ruby
# coding: utf-8

require_relative 'common'

STDIN.set_encoding("ASCII-8BIT")

ngram_prev=[]
while gets
	ngram, value = $_.chomp.split("\t")

	ngram=ngram.split
	if ngram_prev.size < ngram.size
		branch=ngram_prev.size
		ngram_prev.each_with_index{|word,i|
			if ngram[i]!=word
				branch=i
				break
			end
		}
		
		common_words = ngram_prev[0...branch]
		(branch...(ngram.size-1)).each{|i|
			inserted = common_words + ngram[branch..i]
			puts_inserted(inserted)
		}
	else # ngram_prev.size >= ngram.size
		branch = ngram.size
		ngram.each_with_index{|word, i|
			if ngram_prev[i]!=word
				branch = i
				break
			end
		}
		
		common_words = ngram[0...branch]
		(branch...(ngram.size-1)).each{|i|
			inserted = common_words + ngram[branch..i]
			puts_inserted(inserted)
		}
	end
	if ngram.size > 1 and ngram[-2]=="<#>"
		puts "#{ngram.join(" ")}\t"
		puts "#{ngram[-1]} #{ngram[0...-1].join(" ")} \x01<#>\t#{value}"
	else
		puts "#{ngram.join(" ")}\t#{value}"
	end
	ngram_prev = ngram
end

