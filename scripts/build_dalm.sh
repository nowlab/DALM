#!/bin/bash
set -e
if [ $# -ne 3 ]; then
	echo "Usage: $0 arpa-file division-number output-dir"
	exit 1
fi

ARPA=$1
OPTMETHOD=embedding
DIVNUM=$2
OUTPUT=$3

TREE=$OUTPUT/`basename $ARPA`.tree.txt
WORDIDS=$OUTPUT/`basename $ARPA`.wordids.txt

DABINMODELFN=dalm.bin
WORDBINFN=words.bin
WORDDICTFN=words.txt

DABINMODEL=$OUTPUT/$DABINMODELFN
WORDBIN=$OUTPUT/$WORDBINFN
WORDDICT=$OUTPUT/$WORDDICTFN

INI=$OUTPUT/dalm.ini

SCRIPTS=`dirname $0`
BIN=$SCRIPTS/../bin
MKWORDDICT=$SCRIPTS/mkworddict.sh
MKTREEFILE="ruby $SCRIPTS/mktreefile.rb"

BUILDER=$BIN/dalm_builder

if [ ! -d $OUTPUT ]; then
	mkdir -p $OUTPUT
else
	echo "$OUTPUT already exists. exit."
	exit 1
fi

echo "MKWORDDICT : `date`"
$MKWORDDICT $ARPA $WORDDICT $WORDIDS

if [ "$OPTMETHOD" = "embedding" ]; then
    echo "MKTREEFILE : `date`"
	$MKTREEFILE $ARPA $TREE
fi

echo "DALM_BUILDER : `date`"
if [ "$OPTMETHOD" = "embedding" ]; then
    LC_ALL=C $BUILDER embedding $ARPA $TREE $WORDDICT $WORDIDS $DABINMODEL $WORDBIN $DIVNUM
fi

echo "GENERATING AN INIFILE : `date`"
echo "TYPE=1" > $INI
echo "MODEL=$DABINMODELFN" >> $INI
echo "WORDS=$WORDBINFN" >> $INI
echo "ARPA=`basename $ARPA`" >> $INI
echo "WORDSTXT=$WORDDICTFN" >> $INI

if [ "$OPTMETHOD" = "embedding" ]; then
       echo "CLEANING A TREE FILE : `date`"
       rm $TREE
fi
echo "CLEANING A WORDID FILE : `date`"
rm $WORDIDS

echo "DONE : `date`"
