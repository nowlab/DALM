#!/usr/bin/env ruby
# coding: utf-8

if __FILE__ == $0
    STDIN.set_encoding("ASCII-8BIT")

    if ARGV.size != 2
        $stderr.puts "Usage: #{__FILE__} order output-tree-file"
    end

    order = ARGV.shift.to_i
    output = ARGV.shift

    ngramnums = Array.new(order+1, 0)

    open("#{output}.tmp", "w:ASCII-8BIT"){|fp|
        ngram_prev = []
        value_prev = nil
        while gets
            ngram, value = $_.chomp.split("\t")
            ngram = ngram.split
            #$stderr.puts ngram_prev.inspect, value_prev.inspect

            if ngram_prev.size > 2 and ngram_prev[-2]=="<#>" and ngram_prev[-1]=="\x01<#>"
                if ngram_prev.size == ngram.size and ngram_prev[0...-1]==ngram[0...-1]
                    fp.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{value_prev}"
                else
                    fp.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{-value_prev.to_f}"
                end
                ngramnums[ngram_prev.size-2]+=1
            elsif ngram_prev.size > 1 and ngram_prev[-2]=="<#>" and value_prev.nil?
                #remove
            elsif ngram_prev.size > 0
                ngram_back = ngram_prev[-1]
                ngram_prev[-1]="\x01 #{ngram_back}"
                fp.puts "#{ngram_prev.join(" ")}\t#{value_prev}"
                ngramnums[ngram_prev.size-1]+=1
            end

            ngram_prev = ngram
            value_prev = value
        end

        if ngram_prev.size > 2 and ngram_prev[-2]=="<#>" and ngram_prev[-1]=="\x01<#>"
            fp.puts "#{ngram_prev[1...-1].join(" ")} \x01 #{ngram_prev[0]}\t#{-value_prev.to_f}"
            ngramnums[ngram_prev.size-2]+=1
        elsif ngram_prev.size > 1 and ngram_prev[-2]=="<#>" and value_prev.nil?
            #remove
        elsif ngram_prev.size > 0
            ngram_back = ngram_prev[-1]
            ngram_prev[-1]="\x01 #{ngram_back}"
            fp.puts "#{ngram_prev.join(" ")}\t#{value_prev}"
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

    system("LC_ALL=C sort -S 40% #{output}.tmp -T $(dirname #{output}.tmp) --compress-program=zstd | LC_ALL=C sed -e 's/\x01 //g' | pv >>#{output}")
    system("LC_ALL=C rm #{output}.tmp")

    open(output,"a:ASCII-8BIT"){|fp|
        fp.puts ""
        fp.puts "\\end\\"
    }
end
