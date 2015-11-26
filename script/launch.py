# ***************************************************************************
# 
# This file is part of NBQueue, a lock-free O(1) priority queue.
# 
#  Copyright (C) 2015, Romolo Marotta
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# ***************************************************************************/
# 
# launch.py
#
#  Created on: Nov 20, 2015
#      Author: Romolo Marotta
#


from subprocess import Popen
from time import sleep
import sys
import termios
import atexit

core = 8
all_threads = [32]#[1,2,3,4,5,6,7,8,12,16,24,28,32]
ops=50000
prob_roll=0.0
prob_dequeue=0.49
look_pool = [1.0,10.0]
data_type = ["C", "N"]#["N", "L"]#, "C"]
iterations = 1

verbose=0
log=0

init_size=32768
prune_period=50000
prune_tresh=0.9
width=1.0
collaborative = 0
safety = 0

ops*=max(all_threads)

cmd1="time"
cmd2="-f"
cmd3="R:%e,U:%U,S:%S"
cmd4="../Debug/NBPQueue"
tmp_dir="tmp"


def enable_echo(fd, enabled):
       (iflag, oflag, cflag, lflag, ispeed, ospeed, cc) \
                     = termios.tcgetattr(fd)
       if enabled:
               lflag |= termios.ECHO
       else:
               lflag &= ~termios.ECHO
       new_attr = [iflag, oflag, cflag, lflag, ispeed, ospeed, cc]
       termios.tcsetattr(fd, termios.TCSANOW, new_attr)

def get_next_set(threads, available):
	res = 0
	for i in threads:
		if i <= available:
			res = max(res, i)
	return res

def print_log(first=False):
	if not first:
		for i in range(3+buf_line):
			sys.stdout.write( "\033[1A\033[K")
	print "Number of test: "+str(num_test-count_test)+"/"+str(num_test)+" Usage Core: "+str(core-core_avail)+"/"+str(core)+"\r"
	print "-------------------------------------------------------------------------\r"
	for j in look_pool:
		for i in all_threads:
			for k in data_type:
				print " ".join([str(i),str(j),str(k),str(residual_iter[(i,j,k)]),str(instance[(i,j,k)])])+"\t",
			print "\r"
	print "-------------------------------------------------------------------------"


if __name__ == "__main__":



	print "#########################################################################"
	print "              TESTS v1.0"
	print "#########################################################################"

	threads = all_threads[:]
	m_core = max(threads)
	if(m_core > core):
		print "-------------------------------------------------------------------------\r"
		print "WARNING more threads ("+str(m_core)+") than core available ("+str(core)+"). Using time-sharing!"
		print "-------------------------------------------------------------------------\r"
		core = m_core
	buf_line = len(threads)*len(look_pool)
	residual_iter = {}
	instance = {}
	
	
	cmd = [cmd1, cmd2, cmd3, cmd4]
	
	
	
	cmd = [cmd1, cmd2, cmd3, cmd4]
	
	
	core_avail = core
	
	test_pool = {}
	file_pool = {}
	run_pool = set([])
	
	init_size	=str(init_size	)
	verbose		=str(verbose		)
	log			=str(log			)
	prune_period=str(prune_period)
	ops			=str(ops			)
	prob_roll	=str(prob_roll	)
	prob_dequeue=str(prob_dequeue)
	prune_tresh	=str(prune_tresh	)
	width		=str(width		)
	
	count_test = 0
	
	atexit.register(enable_echo, sys.stdin.fileno(), True)
	enable_echo(sys.stdin.fileno(), False)
	
	for run in range(iterations):
		for look in look_pool:
			look = str(look)
			for struct in data_type:
				for t in threads:
					if not test_pool.has_key(t):
						test_pool[t] = []
					test_pool[t] += [[struct, ops, str(t),      prune_period, prob_roll, prob_dequeue, look,      init_size,  verbose,  log,  prune_tresh,   width, str(collaborative), str(safety), str(run)]]
					count_test +=1
					#	  		 STRUCT	 OPS, THREADS PRUNE_PERIOD  PROB_ROLL  PROB_DEQUEUE  LOOK_AHEAD INIT_SIZE   VERBOSE   LOG   PRUNE_TRESHOLD BUCKET_WIDTH


	num_test = count_test
	for j in look_pool:
		for i in all_threads:
			for k in data_type:
				residual_iter[(i,j,k)] = iterations
				instance[(i,j,k)] = 0



	print_log(True)
	while count_test > 0:
		t = get_next_set(threads, core_avail)
		if t > 0 and t < 128:
			pool = test_pool[t]
			if len(pool) == 0:
				threads.remove(t)
				del test_pool[t]
			else:
				cmdline = pool[0]
				test_pool[t] = pool[1:]
				filename = tmp_dir+"/"+"-".join(cmdline)
				f = open(filename, "w")
				p = Popen(cmd+cmdline[:-1], stdout=f, stderr=f)
				residual_iter[(t,float(cmdline[6]),cmdline[0])]-=1
				instance[(t,float(cmdline[6]),cmdline[0])]+=1
				core_avail -= t
				run_pool.add( (p,t,float(cmdline[6]),cmdline[0]) )
				file_pool[p]=f
				count_test -= 1
			continue
		print_log()
		sleep(1)
		to_remove = set([])
		for	p,t,i,j in run_pool:
			if p.poll() != None:
				to_remove.add((p,t,i,j))
				file_pool[p].close()
				del file_pool[p]
				instance[(t,i,j)]-=1
				core_avail += t
		run_pool -= to_remove
		
	while core_avail < core:
		to_remove = set([])
		for	p,t,i,j in run_pool:
			if p.poll() != None:
				to_remove.add((p,t,i,j))
				file_pool[p].close()
				del file_pool[p]
				instance[(t,i,j)]-=1
				core_avail += t
		run_pool -= to_remove
		sleep(1)
		print_log()

	'''
	for k in reversed(sorted(test_pool.keys())):
		for cmdline in test_pool[k]:
			while True:
				to_remove = set([])
				for	p,t in run_pool:
					if p.poll() != None:
						to_remove.add((p,t))
						file_pool[p].close()
						del file_pool[p]
						core_avail += t
				run_pool -= to_remove
				if core_avail - k >= 0:
					filename = tmp_dir+"/"+"-".join(cmdline)
					f = open(filename, "w")
					p = Popen(cmd+cmdline[:-1], stdout=f, stderr=f)
					core_avail -= k
					run_pool.add( (p,k) )
					file_pool[p]=f
					break		
				sleep(1)
	'''
