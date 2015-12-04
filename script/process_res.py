#!/usr/bin/env python

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
# process_res.py
#
#  Created on: Nov 20, 2015
#      Author: Romolo Marotta
#
###########################################	

import math

from os import walk

out_log = open("res/aggregated", "w")

err_log = open("res/error_log", "w")

for (dirpath, dirnames, filenames) in walk("tmp"):
	for f in filenames:
		if f == "aggregated":
			continue
		if f[0] not in ["N","C","L"]:
			continue
		f = open("tmp/"+f)
		l = []
		for line in f.readlines():
			l += [line]
		if len(l) != 1:
			o = err_log
		else:
			o = out_log
		for line in l:
			o.write(line.strip()+" ")
		o.write("\n")
out_log.close()

f = open("res/aggregated")
d = {}
for line in f.readlines():
	try:
		line = line.strip().split(",")
		name = ""
		r = 0
		u = 0
		s = 0
		t = 0
		o = 0
		e = 0
		for elem in line:
			try:
				k,v = elem.split(":")
			except:
				err_log.write(str( line )+"\n")
				continue
			if k == "T":
				t = int(v)
			elif k == "OPS":
				name += str(v) + "-"
				o = int(v)
			elif k == "PRUNE_PER":
				name += str(v) + "-"
			elif k == "PRUNE_T":  
				name += str(v) + "-"
			elif k == "P_DEQUEUE":
				name += str(v) + "-"
			elif k == "MEAN_INTERARRIVAL_TIME":   
				name += str(v) + "-"
			elif k == "PROB_DIST":   
				name += str(v) + "-"
			elif k == "SIZE":     
				name += str(v) + "-"
			elif k == "B_WIDTH":
				name += str(v) + "-"
			elif k == "MALLOC_T":
				pass
			elif k == "FREE_T":
				pass
			elif k == "R":
				r = float(v)
			elif k == "U":
				u = float(v)
			elif k == "D":
				da = v
			elif k == "EMPTY_QUEUE":
				e = int(v)
				name += str(v) + "-"
			elif k == "CHECK":
				if v != "0" and e == 0:
					err_log.write(str( line )+"\n")
					continue
			elif k == "S":
				name += da
				s = float(v)
				if not d.has_key(name):
					d[name] = {}
				if not d[name].has_key(t):
					d[name][t] = []
				d[name][t] += [(r,u,s,o)]
	except:
		err_log.write(str( line )+"\n")
		
err_log.close()
f.close()

for k,v in d.items():
	filename = "res/"+k.replace(".","_")+".dat"
	f = open(filename,"w")
	f.write("T, R, U+S, OPS/R, OPS/(U+S)\n")
	print filename
	for k1 in sorted(v.keys()):
		count = len(v[k1])
		r_avg = 0
		us_avg = 0
		opsr_avg = 0
		opsus_avg = 0
		r_dev = 0
		us_dev = 0
		opsr_dev = 0
		opsus_dev = 0
		for l in v[k1]:
			if l[0] > (l[1]+l[2])/k1:
				print "STRANO- R:"+str( l[0] )+"\tU:"+str(l[1]+l[2])
			#f.write(str(k1)+", "+str(l[0])+", "+str(l[1]+l[2])+", "+str(l[3]/l[0])+", "+str(l[3]/(l[1]+l[2]))+"\n")
			r_avg 		+= l[0]
			us_avg 		+= l[1]+l[2]
			try:
				opsr_avg 	+= l[3]/l[0]
			except:
				print str(k1)+"\t"+str(v[k1])
			opsus_avg 	+= l[3]/(l[1]+l[2])
		r_avg 		/= count
		us_avg 		/= count
		opsr_avg 	/= count
		opsus_avg 	/= count
		for l in v[k1]:
			r_dev  		+= (l[0]			-r_avg		)*(l[0]				- r_avg		)
			us_dev 		+= (l[1]+l[2]		- us_avg 	)*(l[1]+l[2]		- us_avg   	)
			opsr_dev 	+= (l[3]/l[0]		- opsr_avg 	)*(l[3]/l[0]		- opsr_avg 	)
			opsus_dev 	+= (l[3]/(l[1]+l[2])- opsus_avg )*(l[3]/(l[1]+l[2])	- opsus_avg )
		r_dev  		/= count
		us_dev 		/= count
		opsr_dev 	/= count
		opsus_dev 	/= count
		r_dev  		 = math.sqrt(r_dev  		)
		us_dev 		 = math.sqrt(us_dev 		)
		opsr_dev   	 = math.sqrt(opsr_dev   	)
		opsus_dev  	 = math.sqrt(opsus_dev  	)
		
		f.write(str(k1)+", "+\
			str(r_avg)+", "+str(r_dev)+", "+\
			str(us_avg)+", "+str(us_dev)+", "+\
			str(opsr_avg)+", "+str(opsr_dev)+", "+\
			str(opsus_avg)+", "+str(opsus_dev)+"\n")
		
		
	f.close()

err_log.close()

