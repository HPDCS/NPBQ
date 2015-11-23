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
# graphs.sh
#
#  Created on: Nov 20, 2015
#      Author: Romolo Marotta
#

pdf_dir="./pdf"
eps_dir="./eps"

if [ ! -d $pdf_dir ]; then
	mkdir $pdf_dir
fi

if [ ! -d $eps_dir ]; then
	mkdir $eps_dir
fi

for i in ./res/*-N.dat;  do
 o=`basename $i -N.dat`
 gnuplot -e "filename='./res/${o}'" script.gnuplot
done

for i in ./res/*.eps; do
	epstopdf $i
done
	
mv ./res/*.eps $eps_dir
mv ./res/*.pdf $pdf_dir
