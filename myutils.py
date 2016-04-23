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

#---------------------------------------------------------------------------------------------------------------------------------
class Timer:
    tstart = 0
    tend = 0
    def start(self):
        self.tstart = time.time()
    def end(self):
        self.tend = time.time()
    def elapsed(self):
        return (time.time() - self.tstart)

#---------------------------------------------------------------------------------------------------------------------------------
def ensuredir(dir):
    if not os.path.exists(dir):
        os.makedirs(dir)
#---------------------------------------------------------------------------------------------------------------------------------
def countFileLines(filename) :
    count_lines = 0
    if not os.path.exists(filename) :
        return 0
    else :
        file = open(filename)
        for line in file.readlines():
            if len(line) > 1:
                count_lines = count_lines+1
        file.close()
        return count_lines
#---------------------------------------------------------------------------------------------------------------------------------
def getFileLines(filename) :
    lines = []
    if os.path.exists(filename):
        file = open(filename)
        lines=file.readlines()
        file.close()
    return lines
#---------------------------------------------------------------------------------------------------------------------------------
def readSingleLine(filename) :
    res = ''
    if os.path.exists(filename):
        file = open(filename)
        for line in file.readlines():
            res = line.replace('\n','')
            break
        file.close()
    return res
#---------------------------------------------------------------------------------------------------------------------------------        
def appendline2file(filename, content) :
    file = open(filename, 'a+')
    file.write(content+"\n")
    file.close()
#---------------------------------------------------------------------------------------------------------------------------------
def appendlines2file(filename, content) :
    file = open(filename, 'a+')
    for str in content:
        file.write(str+"\n")
    file.close()
#---------------------------------------------------------------------------------------------------------------------------------        
def rotatefile(filename):
    file = open(filename)
    lines = file.readlines()
    for i in range( len(lines[0]) ):
       str = ''
       for line in lines:
           if len(line) > 2:
               str = str + line[i]
       appendline2file('output', str)

    file.close()
    exec_cmd('cp -f %s %s'%('output', filename))
    rmfiles( ['output'] )

#---------------------------------------------------------------------------------------------------------------------------------
def rmfiles( files ):
    for file in files:
        exec_cmd( 'rm -rf %s'%(file))

#---------------------------------------------------------------------------------------------------------------------------------
def exec_cmd( cmd ):
    print cmd
    p = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT)
    sts = os.waitpid(p.pid, 0)[1]
    return p

