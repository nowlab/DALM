#!/bin/sh

if [ $# -ne 3 ]; then
	echo "Usage: $0 arpa-file division-number output-dir"
	exit 1
fi

ARPA=$1
DIVNUM=$2
OUTPUT=$3

TREE=$OUTPUT/`basename $ARPA`.tree.txt
WORDDICT=$OUTPUT/`basename $ARPA`.worddict.txt
WORDIDS=$OUTPUT/`basename $ARPA`.wordids.txt

DUMPEDARPA=$OUTPUT/`basename $ARPA`.arpa.dump
DUMPEDTREE=$OUTPUT/`basename $ARPA`.tree.dump

DABINMODEL=$OUTPUT/dalm.bin
WORDBIN=$OUTPUT/words.bin
INI=$OUTPUT/dalm.ini

SCRIPTS=`dirname $0`
BIN=$SCRIPTS/../bin
MKWORDDICT=$SCRIPTS/mkworddict.sh
MKTREEFILE="ruby $SCRIPTS/mktreefile.rb"
DUMPER=$BIN/trie_dumper
BUILDER=$BIN/dalm_builder

if [ ! -d $OUTPUT ]; then
	mkdir -p $OUTPUT
else
	echo "$OUTPUT already exists. exit."
	exit 1
fi

echo "MKWORDDICT : `date`"
$MKWORDDICT $ARPA $WORDDICT $WORDIDS

echo "MKTREEFILE : `date`"
$MKTREEFILE $ARPA $TREE

echo "TRIE_DUMPER : `date`"
$DUMPER --arpa $ARPA $DUMPEDARPA
$DUMPER --tree $TREE $DUMPEDTREE

echo "CLEANING(TREE) : `date`"
rm $TREE

echo "DALM_BUILDER : `date`"
$BUILDER $DUMPEDARPA $DUMPEDTREE $WORDDICT $WORDIDS $DABINMODEL $WORDBIN $DIVNUM

echo "GENERATING INIFILE : `date`"
echo "TYPE=0" > $INI
echo "MODEL=$DABINMODEL" >> $INI
echo "WORDS=$WORDBIN" >> $INI
echo "ARPA=$ARPA" >> $INI

echo "CLEANING(DUMPFILES) : `date`"
rm $DUMPEDARPA $DUMPEDTREE

echo "CLEANING(WORD FILES) : `date`"
rm $WORDDICT $WORDIDS

echo "DONE : `date`"

