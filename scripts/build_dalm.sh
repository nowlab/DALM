#!/bin/bash -eu

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
MKWORDDICT=$BIN/mkworddict
MKTREEFILE="$SCRIPTS/mktreefile.sh"

BUILDER=$BIN/dalm_builder

if hash pbzip2 2>/dev/null; then
    BZIP2=pbzip2
else
    BZIP2=bzip2
fi

if [ ! -d $OUTPUT ]; then
	mkdir -p $OUTPUT
else
	echo "$OUTPUT is already exists."
#	exit 1
fi

if [ -e ${WORDDICT}.bz2 ] && [ -e ${WORDIDS}.bz2 ]; then
  echo 'Zipped WORDDICT is already exists.'
  echo "Unzipping... : `date`"
  $BZIP2 -dc ${WORDDICT}.bz2 >${WORDDICT}
  $BZIP2 -dc ${WORDIDS}.bz2 >${WORDIDS}
elif [ ! -e ${WORDDICT} ] || [ ! -e ${WORDIDS} ]; then 
  echo "MKWORDDICT : `date`"
  $MKWORDDICT $ARPA $WORDDICT $WORDIDS
  $BZIP2 ${WORDDICT}
  $BZIP2 ${WORDIDS}
else
  echo 'WORDDICT is already exists'
fi

if [ "$OPTMETHOD" = "embedding" ]; then
  if [ -e ${TREE}.bz2 ]; then
    echo Zipped TREE is already exists.
    echo "Unzipping... : `date`"
    $BZIP2 -dc ${TREE}.bz2 >${TREE}
  elif [ ! -e ${TREE} ]; then
    echo "MKTREEFILE : `date`"
	$MKTREEFILE $ARPA $TREE
    $BZIP2 ${TREE}
  else
    echo TREE is already exists.
  fi
fi

echo "DALM_BUILDER : `date`"
if [ "$OPTMETHOD" = "embedding" ]; then
    echo "Skip build dalm"
#    LC_ALL=C $BUILDER embedding $ARPA $TREE $WORDDICT $WORDIDS $DABINMODEL $WORDBIN $DIVNUM
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
