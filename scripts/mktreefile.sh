#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage $0 arpa-file output-tree-file"
    exit
fi

ARPA=$1
OUT=$2

CUR=$(dirname $0)
ORDER=$(ruby ${CUR}/getorder.rb ${ARPA})
echo "Create BST from $ARPA to $OUT for DALM as ${ORDER}-gram language models."
MKTREE1="ruby ${CUR}/mktree1.rb"
MKTREE2="ruby ${CUR}/mktree2.rb"
MKTREE3="ruby ${CUR}/mktree3.rb ${ORDER} ${OUT}"
SORT="sort -S 40% -T $(dirname ${OUT})"

echo "Generate all paths on BST."
${MKTREE1} <${ARPA} | pv >${OUT}.tmp1
echo "Extract all paths for each vertexes."
LC_ALL=C ${SORT} ${OUT}.tmp1 | ${MKTREE2} | pv >${OUT}.tmp2
rm ${OUT}.tmp1
echo "Set probability values for each leaves."
LC_ALL=C ${SORT} ${OUT}.tmp2 | ${MKTREE3}
rm ${OUT}.tmp2
