#!/bin/bash

if [ $# -ne 3 ]; then
	echo "Usage: $0 arpa-file division-number output-dir"
	exit 1
fi

ARPA=$1
DIVNUM=$2
OUTPUT=$3

TREE=$OUTPUT/`basename $ARPA`.tree.txt
WORDIDS=$OUTPUT/`basename $ARPA`.wordids.txt

DUMPEDARPA=$OUTPUT/`basename $ARPA`.arpa.dump
DUMPEDTREE=$OUTPUT/`basename $ARPA`.tree.dump

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
LC_ALL=C $DUMPER --arpa $ARPA $DUMPEDARPA
LC_ALL=C $DUMPER --tree $TREE $DUMPEDTREE

echo "CLEANING A TREE FILE : `date`"
rm $TREE

echo "DALM_BUILDER : `date`"
LC_ALL=C $BUILDER $DUMPEDARPA $DUMPEDTREE $WORDDICT $WORDIDS $DABINMODEL $WORDBIN $DIVNUM

echo "GENERATING AN INIFILE : `date`"
echo "TYPE=1" > $INI
echo "MODEL=$DABINMODELFN" >> $INI
echo "WORDS=$WORDBINFN" >> $INI
echo "ARPA=`basename $ARPA`" >> $INI
echo "WORDSTXT=$WORDDICTFN" >> $INI

echo "CLEANING DUMPFILES : `date`"
rm $DUMPEDARPA $DUMPEDTREE

echo "CLEANING A WORDID FILE : `date`"
rm $WORDIDS

echo "DONE : `date`"

