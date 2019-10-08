# coding: utf-8

def puts_inserted(inserted)
	if inserted[-1]=="<#>"
        throw "Irregular ARPA file. Please check your cutoff parameter. B( #{inserted[0...-1].join(" ")} ) is not found, although ARPA has P( * | #{inserted[0...-1].join(" ")} )."
	else
		puts "#{inserted.join(" ")}\t"
	end
end

