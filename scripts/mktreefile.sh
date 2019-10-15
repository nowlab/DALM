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
SORT="sort -S 40% -T $(dirname ${OUT}) --compress-program=zstd"

echo "Generate all paths on BST."
${MKTREE1} <${ARPA} | pv >${OUT}.tmp1
echo "Sorting paths."
LC_ALL=C ${SORT} ${OUT}.tmp1 >${OUT}.tmp2
rm ${OUT}.tmp1
echo "Extract all paths for each vertexes."
${MKTREE2} <${OUT}.tmp2 | pv >${OUT}.tmp3
rm ${OUT}.tmp2
echo "Sorting paths."
LC_ALL=C ${SORT} ${OUT}.tmp3 >${OUT}.tmp4
rm ${OUT}.tmp3
echo "Set probability values for each leaves."
${MKTREE3} <${OUT}.tmp4
rm ${OUT}.tmp4
