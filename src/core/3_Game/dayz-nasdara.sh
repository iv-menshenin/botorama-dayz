#!/bin/bash

cd /mnt/deep-space/Steam/steamapps/common/DayZServer/

./DayZServer -cpuCount="6" "-config=./nasdaraDZ.cfg" "-mod=@Botorama;" "-profiles=./profiles-nasdara" "-adminlog" "-port=2302"
