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
# launchAll.sh
#
#  Created on: Nov 20, 2015
#      Author: Romolo Marotta
#

log_dir="./tmp"
res_dir="./res"

if [ ! -d $log_dir ]; then
	mkdir $log_dir
fi


if [ ! -d $res_dir ]; then
	mkdir $res_dir
fi


rm $res_dir/*

time -f "time %E" python launch.py
echo process data
#time -f "time %E" python process_res.py
#echo generate graphs
#time -f "time %E" sh graphs.sh
