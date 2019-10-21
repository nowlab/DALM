#!/bin/bash
set -e
if [ $# -ne 4 ]; then
	echo "Usage: $0 arpa-file division-number output-dir build-type"
	exit 1
fi

ARPA=$1
OPTMETHOD=embedding
DIVNUM=$2
OUTPUT=$3
BUILD_TYPE=$4

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
if [ $BUILD_TYPE = 0 ]; then
  BIN=$SCRIPTS/../bin
elif [ $BUILD_TYPE = 1 ]; then
  BIN=$SCRIPTS/../nbin
elif [ $BUILD_TYPE = 2 ]; then
  BIN=$SCRIPTS/../n1bin
fi
MKWORDDICT=$BIN/mkworddict
MKTREEFILE="$SCRIPTS/mktreefile.sh"

BUILDER=$BIN/dalm_builder

if [ ! -d $OUTPUT ]; then
	mkdir -p $OUTPUT
else
	echo "$OUTPUT is already exists."
#	exit 1
fi

if [ -e ${WORDDICT}.bz2 ] && [ -e ${WORDIDS}.bz2 ]; then
  echo 'Zipped WORDDICT is already exists.'
  echo "Unzipping... : `date`"
  bzip2 -dc ${WORDDICT}.bz2 >${WORDDICT}
  bzip2 -dc ${WORDIDS}.bz2 >${WORDIDS}
elif [ ! -e ${WORDDICT} ] || [ ! -e ${WORDIDS} ]; then 
  echo "MKWORDDICT : `date`"
  $MKWORDDICT $ARPA $WORDDICT $WORDIDS
  bzip2 ${WORDDICT}
  bzip2 ${WORDIDS}
else
  echo 'WORDDICT is already exists'
fi

if [ "$OPTMETHOD" = "embedding" ]; then
  if [ -e ${TREE}.bz2 ]; then
    echo Zipped TREE is already exists.
    echo "Unzipping... : `date`"
    bzip2 -dc ${TREE}.bz2 >${TREE}
  elif [ ! -e ${TREE} ]; then
    echo "MKTREEFILE : `date`"
	$MKTREEFILE $ARPA $TREE
    bzip2 ${TREE}
  else
    echo TREE is already exists.
  fi
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
