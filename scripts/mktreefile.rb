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
		tmpfp.puts "#{bowpath.join(" ")} <#>\t#{bow}"
	end
end
arpafp.close
tmpfp.close

system("LC_ALL=C sort #{output}.tmp > #{output}.tmp2")
system("LC_ALL=C rm #{output}.tmp")
open("#{output}.tmp2","r:ASCII-8BIT"){|fp|
	open("#{output}.tmp3","w:ASCII-8BIT"){|fpout|
		pre = []
		preword = nil
		while fp.gets
			$_.chomp!

			ngram, value = $_.split("\t")
			ngram = ngram.split
			if pre.size == ngram.size-1 and pre == ngram[0..-2]
				preword = ngram[-1]
				ngram[-1] = "\x01 #{ngram[-1]}"
				fpout.puts "#{ngram.join(" ")}\t#{value}"
				ngramnums[ngram.size-1]+=1
			else
				branch = pre.size
				pre.each_index{|i|
					if pre[i] != ngram[i]
						branch = i
						break
					end
				}
				if branch == pre.size and preword and ngram[-2] and preword == ngram[-2]
					branch+=1
				end

				(branch...(ngram.size-1)).each{|i|
					if i > 0
						fpout.puts "#{ngram[0..(i-1)].join(" ")} \x01 #{ngram[i]}\t"
					else
						fpout.puts "\x01 #{ngram[i]}\t"
					end
					ngramnums[i]+=1
				}
				preword = ngram[-1]
				ngram[-1] = "\x01 #{ngram[-1]}"
				fpout.puts "#{ngram.join(" ")}\t#{value}"
				ngramnums[ngram.size-1]+=1
				pre = ngram[0..-2]
			end
		end
	}
}

system("LC_ALL=C rm #{output}.tmp2")
open(output, "w:ASCII-8BIT"){|fp|
	fp.puts "\\data\\"
	ngramnums.each_index{|i|
		fp.puts "ngram #{i+1}=#{ngramnums[i]}"
	}
	fp.puts ""
	fp.puts "\\n-grams:"
}

system("LC_ALL=C sort #{output}.tmp3 | sed -e 's/\x01 //g' >> #{output}")
system("LC_ALL=C rm #{output}.tmp3")

open(output,"a:ASCII-8BIT"){|fp|
	fp.puts ""
	fp.puts "\\end\\"
}

