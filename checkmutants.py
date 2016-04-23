#!/usr/bin/python

import os
import shutil
import subprocess
import time
import signal
import re
import getopt, sys
import glob
from optparse import OptionParser
from time import localtime, strftime
import string
import datetime
import myutils
import itertools

opt='-DCONFIG_YAFFS_DIRECT -DCONFIG_YAFFS_YAFFS2 -DCONFIG_YAFFS_PROVIDE_DEFS -DCONFIG_YAFFSFS_PROVIDE_VALUES -DNO_Y_INLINE -DCONFIG_YAFFS_USE_PTHREADS'

mutants=glob.glob('mutant*.c')
mutants.sort()
for mutant in mutants:
	myutils.exec_cmd('rm -rf a.o')
	myutils.exec_cmd('gcc %s -c %s -I ./inc -o a.o'%(opt, mutant))
	if not os.path.exists('a.o'):
		myutils.exec_cmd('mv %s x_%s'%(mutant, mutant))



