#!/usr/bin/python
import pstats
import sys
fn = sys.argv[1]
p = pstats.Stats(fn)
p.strip_dirs().sort_stats('cumulative').print_stats()
