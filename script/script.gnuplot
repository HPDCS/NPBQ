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
# script.gnuplot
#
#  Created on: Nov 20, 2015
#      Author: Romolo Marotta
#
###########################################	 	 
reset

set term postscript eps enhanced color font "Helvetica" 16

set style line 1 pt 7 ps 0.8

set style line 2 lt 2 lw 3
set size 0.8,0.8
set offset 0.5,0.5,0.5,0.5


set title filename

set xlabel '#Threads'
set grid x

set ylabel 'CPU Time(s)'
set grid y



set output filename.'_1.eps'
set key top left
plot \
filename.'-N.dat'\
	using 1:4:5	w yerrorbars 	linecolor rgb "#000099" lt 1 lw 1.5 	 title "", '' \
	using 1:4	w linespoints 	linecolor rgb "#000099" lt 2 lw 1.5 pt 6 title "NBQueue", \
filename.'-L.dat'\
	using 1:4:5	w yerrorbars 	linecolor rgb "#009900" lt 1 lw 1.5 	 title "", '' \
	using 1:4	w linespoints 	linecolor rgb "#009900" lt 2 lw 1.5 pt 6 title "LList", \
filename.'-C.dat'\
	using 1:4:5	w yerrorbars 	linecolor rgb "#990000" lt 1 lw 1.5 	 title "", '' \
	using 1:4	w linespoints 	linecolor rgb "#990000" lt 2 lw 1.5 pt 6 title "CalQueue"

#set yrange [:3000000]
#
#set ylabel '#OPS/Time'
#set output filename.'_2.eps'
#set key bottom left
#plot \
#filename.'-N.dat'\
#	using 1:6:7	w yerrorbars 	linecolor rgb "#000099" lt 1 lw 1.5 	 title "", '' \
#	using 1:6	w linespoints 	linecolor rgb "#000099" lt 2 lw 1.5 pt 6 title "NBQueue", \
#filename.'-L.dat'\
#	using 1:6:7	w yerrorbars 	linecolor rgb "#009900" lt 1 lw 1.5 	 title "", '' \
#	using 1:6	w linespoints 	linecolor rgb "#009900" lt 2 lw 1.5 pt 6 title "LList", \
#filename.'-C.dat'\
#	using 1:6:7	w yerrorbars 	linecolor rgb "#990000" lt 1 lw 1.5 	 title "", '' \
#	using 1:6	w linespoints 	linecolor rgb "#990000" lt 2 lw 1.5 pt 6 title "CalQueue"
#	
#set yrange [:500000]
#	
#set ylabel '#OPS/CPU Time'
#set output filename.'_3.eps'
#set key bottom left
#plot \
#filename.'-N.dat'\
#	using 1:8:9	w yerrorbars 	linecolor rgb "#000099" lt 1 lw 1.5 	 title "", '' \
#	using 1:8	w linespoints 	linecolor rgb "#000099" lt 2 lw 1.5 pt 6 title "NBQueue", \
#filename.'-L.dat'\
#	using 1:8:9	w yerrorbars 	linecolor rgb "#009900" lt 1 lw 1.5 	 title "", '' \
#	using 1:8	w linespoints 	linecolor rgb "#009900" lt 2 lw 1.5 pt 6 title "LList", \
#filename.'-C.dat'\
#	using 1:8:9	w yerrorbars 	linecolor rgb "#990000" lt 1 lw 1.5 	 title "", '' \
#	using 1:8	w linespoints 	linecolor rgb "#990000" lt 2 lw 1.5 pt 6 title "CalQueue"
