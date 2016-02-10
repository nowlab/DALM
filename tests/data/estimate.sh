#!/bin/bash

$HOME/opt/kenlm/bin/lmplz -o 5 -S 80% -T . --prune 0 0 1 < kftt.2k.ja > kftt.2k.kenlm.arpa

