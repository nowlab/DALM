#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage $0 arpa-file output-tree-file"
    exit
fi

ARPA=$1
OUT=$2

CUR=$(dirname $0)
ORDER=$(ruby $CUR/getorder.rb $ARPA)
echo "Create BST from $ARPA to $OUT as ${ORDER}-gram language models."
MKTREE1="ruby ${CUR}/mktree1.rb"
MKTREE2="ruby ${CUR}/mktree2.rb"
MKTREE3="ruby ${CUR}/mktree3.rb ${ORDER} ${OUT}"
SORT="sort -S 40%"


LC_ALL=C \
$MKTREE1 <$ARPA \
| $SORT \
| $MKTREE2 \
| $SORT \
| $MKTREE3

