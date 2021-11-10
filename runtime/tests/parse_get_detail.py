import os
import sys
import numpy as np
from collections import defaultdict
#import matplotlib.pyplot as plt
#cmd1='grep "outbound|80||%s" istio-log/%s | grep "8012::capacity::"|awk -F "::" \'{print $2 " " $4}\'' % (function, files[i])
def def_value():
	return 0

def parse_file(file_dir):
	running_time_dict = defaultdict(list)
	queuing_times_dict = defaultdict(list)
	total_times_dict = defaultdict(list)
	runnable_times_dict = defaultdict(list)
	blocked_times_dict = defaultdict(list)
	initializing_times_dict = defaultdict(list)
	#execution_times_dict = defaultdict(list)
	pure_times_dict = defaultdict(list)
	real_total_times_dict = defaultdict(list)

	ids = []	
	running_times = [] 
	queuing_times = []
	total_times = []
	runnable_times = []
	blocked_times = []
	initializing_times = []
	#execution_times = []
	real_total_times = []
	fo = open(file_dir, "r+")
	next(fo)
	for line in fo:
		t = line.split(",")
		id = t[1]
		ids.append(id)
		running_time_dict[id].append(int(t[9]))
		queuing_times_dict[id].append(int(t[6]))
		total_times_dict[id].append(int(t[5]))
		runnable_times_dict[id].append(int(t[8]))
		blocked_times_dict[id].append(int(t[10]))
		initializing_times_dict[id].append(int(t[7]))
		#execution_times_dict[id].append(int(t[11]))
		real_total_times_dict[id].append(int(t[12]))
	print(running_time_dict[0])
	print("request-id,sandbox-id,completion,blocked,running,queuing,init\n")
	list_len = len(running_time_dict[0])
	ids = list(set(ids))
	for i in ids:
		len_i = len(total_times_dict[i])
		for j in range(len_i):
			print(i,j,total_times_dict[i][j],blocked_times_dict[i][j],running_time_dict[i][j],queuing_times_dict[i][j],initializing_times_dict[i][j])
	#return  np.median(running_times), np.median(queuing_times), np.median(runnable_times), np.median(blocked_times), np.median(initializing_times)

	#print(initializing_times)
if __name__ == "__main__":
    import json
    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " file dir")
        sys.exit()

    #m_running_t, m_queuing_t, m_runnable_t, m_blocked_t, m_initializing_t = parse_file(argv[0])
    parse_file(argv[0])

    #print(m_running_t, m_queuing_t, m_runnable_t, m_blocked_t, m_initializing_t) 

