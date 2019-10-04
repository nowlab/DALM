#!/usr/bin/env ruby
# coding: utf-8

STDIN.set_encoding("ASCII-8BIT")

if ARGV.size != 2
    $stderr.puts "Usage: #{__FILE__} order output-tree-file"
end

order = ARGV.shift.to_i
output = ARGV.shift

def puts_inserted(fpout, inserted)
	if inserted[-1]=="<#>"
        throw "Irregular ARPA file. Please check your cutoff parameter. B( #{inserted[0...-1].join(" ")} ) is not found, although ARPA has P( * | #{inserted[0...-1].join(" ")} )."
	else
		fpout.puts "#{inserted.join(" ")}\t"
	end
end

ngramnums = Array.new(order, 0)

open("#{output}.tmp", "w:ASCII-8BIT"){|fpout|
    ngram_prev = []
    value_prev = nil
    while gets
    	ngram, value = $_.chomp.split("\t")
    	ngram = ngram.split
    	#$stderr.puts ngram_prev.inspect, value_prev.inspect

	    if ngram_prev.size > 2 and ngram_prev[-2]=="<#>" and ngram_prev[-1]=="\x01<#>"
	    	if ngram_prev.size == ngram.size and ngram_prev[0...-1]==ngram[0...-1]
	    		fpout.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{value_prev}"
	    	else
		    	fpout.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{-value_prev.to_f}"
	    	end
		    ngramnums[ngram_prev.size-2]+=1
    	elsif ngram_prev.size > 1 and ngram_prev[-2]=="<#>" and value_prev.nil?
		    #remove
	    elsif ngram_prev.size > 0
	    	ngram_back = ngram_prev[-1]
	    	ngram_prev[-1]="\x01 #{ngram_back}"
	    	fpout.puts "#{ngram_prev.join(" ")}\t#{value_prev}"
	    	ngramnums[ngram_prev.size-1]+=1
    	end

	    ngram_prev = ngram
	    value_prev = value
    end

    if ngram_prev.size > 2 and ngram_prev[-2]=="<#>" and ngram_prev[-1]=="\x01<#>"
    	fpout.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{-value_prev.to_f}"
    	ngramnums[ngram_prev.size-2]+=1
    elsif ngram_prev.size > 1 and ngram_prev[-2]=="<#>" and value_prev.nil?
    	#remove
    elsif ngram_prev.size > 0
    	ngram_back = ngram_prev[-1]
    	ngram_prev[-1]="\x01 #{ngram_back}"
    	fpout.puts "#{ngram_prev.join(" ")}\t#{value_prev}"
    	ngramnums[ngram_prev.size-1]+=1
    end
}

open(output, "w:ASCII-8BIT"){|fp|
	fp.puts "\\data\\"
	ngramnums.each_index{|i|
		fp.puts "ngram #{i+1}=#{ngramnums[i]}"
	}
	fp.puts ""
	fp.puts "\\n-grams:"
}

system("LC_ALL=C sort -S 40% #{output}.tmp | LC_ALL=C sed -e 's/\x01 //g' >> #{output}")
system("LC_ALL=C rm #{output}.tmp")

open(output,"a:ASCII-8BIT"){|fp|
	fp.puts ""
	fp.puts "\\end\\"
}

