#! /bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 arpa-file output-worddict output-wordids"
	exit 1
fi

ARPA=$1
WORDDICT=$2
WORDIDS=$3

LOOKUPUNIGRAMS="ruby `dirname $0`/lookup_unigrams.rb"
TEMP=`pwd`/dalmtemp

if [ ! -d ${TEMP} ]; then
	mkdir ${TEMP}
else
	echo "TEMPORARY DIRECTORY EXISTS. EXIT."
	exit 1
fi

LC_ALL=C ${LOOKUPUNIGRAMS} $1 ${TEMP}/unigrams.txt

pushd ${TEMP} > /dev/null

LC_ALL=C sort -k1nr -k3nr unigrams.txt > probsortuni.temp
LC_ALL=C nl probsortuni.temp > lineprob.temp
LC_ALL=C cut -f 1,3 lineprob.temp > line_prob.temp
LC_ALL=C sort -t "	" -k 2 line_prob.temp > line_word.temp

popd > /dev/null

LC_ALL=C cut -f 1 ${TEMP}/line_word.temp | tr -d ' ' > $WORDIDS
LC_ALL=C cut -f 2 ${TEMP}/line_word.temp | tr -d ' ' > $WORDDICT

rm -r ${TEMP}
