#!/bin/sh

perl scripts/stackcollapse.pl "$1" > flamegraph_collapsed.txt
perl scripts/flamegraph.pl flamegraph_collapsed.txt > flamegraph.svg