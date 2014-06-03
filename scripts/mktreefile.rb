#!/usr/bin/env ruby
# coding: utf-8

if ARGV.size != 2
	$stderr.puts "Usage: ruby #{__FILE__} arpa-file output-tree-file"
	exit 1
end

arpa = ARGV.shift
output = ARGV.shift

arpafp = open(arpa,"r:ASCII-8BIT")
tmpfp = open(output+".tmp","w:ASCII-8BIT")

ngramnums = [0]

while arpafp.gets
	$_.chomp!
	$_.strip!
	if $_ =~ /^ngram *?\d+ *?= *?\d+$/
		ngramnums << 0
	elsif $_ == "\\1-grams:"
		break
	end
end

tmpfp.puts "<#>\t"
while arpafp.gets
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
	tmpfp.puts "#{probpath.join(" ")}\t#{prob}"

	if not bow.nil?
		bowpath = ngram.reverse
		tmpfp.puts "#{bowpath.join(" ")}\t"
		tmpfp.puts "#{bowpath.join(" ")} <#>\t#{bow}"
	end
end
arpafp.close
tmpfp.close

def puts_inserted(fpout, inserted)
	if inserted[-1]=="<#>"
		fput.puts "#{inserted.join(" ")}\t0.0"
	else
		fpout.puts "#{inserted.join(" ")}\t"
	end
end

system("LC_ALL=C sort #{output}.tmp > #{output}.tmp2")
system("LC_ALL=C rm #{output}.tmp")

open("#{output}.tmp2","r:ASCII-8BIT"){|fp|
	open("#{output}.tmp3","w:ASCII-8BIT"){|fpout|
		ngram_prev=[]
		while fp.gets
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
					puts_inserted(fpout, inserted)
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
					puts_inserted(fpout, inserted)
				}
			end
			if ngram.size > 1 and ngram[-2]=="<#>"
				fpout.puts "#{ngram.join(" ")}\t"
				fpout.puts "#{ngram[-1]} #{ngram[0...-1].join(" ")} \x01<#>\t#{value}"
			else
				fpout.puts "#{ngram.join(" ")}\t#{value}"
			end
			ngram_prev = ngram
		end
	}
}

system("LC_ALL=C rm #{output}.tmp2")
system("LC_ALL=C sort #{output}.tmp3 > #{output}.tmp4")
system("LC_ALL=C rm #{output}.tmp3")

open("#{output}.tmp4","r:ASCII-8BIT"){|fp|
	open("#{output}.tmp5","w:ASCII-8BIT"){|fpout|
		ngram_prev = []
		value_prev = nil
		while fp.gets
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
}

system("LC_ALL=C rm #{output}.tmp4")
open(output, "w:ASCII-8BIT"){|fp|
	fp.puts "\\data\\"
	ngramnums.each_index{|i|
		fp.puts "ngram #{i+1}=#{ngramnums[i]}"
	}
	fp.puts ""
	fp.puts "\\n-grams:"
}

system("LC_ALL=C sort #{output}.tmp5 | sed -e 's/\x01 //g' >> #{output}")
system("LC_ALL=C rm #{output}.tmp5")

open(output,"a:ASCII-8BIT"){|fp|
	fp.puts ""
	fp.puts "\\end\\"
}

