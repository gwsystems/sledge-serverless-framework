import os
import sys
import numpy as np
from collections import defaultdict
#import matplotlib.pyplot as plt
#cmd1='grep "outbound|80||%s" istio-log/%s | grep "8012::capacity::"|awk -F "::" \'{print $2 " " $4}\'' % (function, files[i])
def def_value():
	return 0

def parse_file(file_dir):
	running_time_dict = defaultdict(def_value)
	queuing_times_dict = defaultdict(def_value)
	runnable_times_dict = defaultdict(def_value)
	blocked_times_dict = defaultdict(def_value)
	initializing_times_dict = defaultdict(def_value)
	
	running_times = [] 
	queuing_times = []
	runnable_times = []
	blocked_times = []
	initializing_times = []
	fo = open(file_dir, "r+")
	next(fo)
	for line in fo:
		t = line.split(",")
		id = t[0]
		running_time_dict[id] += int(t[8])
		queuing_times_dict[id] += int(t[5])
		runnable_times_dict[id] += int(t[7])
		blocked_times_dict[id] += int(t[9])
		initializing_times_dict[id] += int(t[6])
	for key,value in running_time_dict.items():	
		running_times.append(value)
	for key,value in queuing_times_dict.items():
		queuing_times.append(value)
	for key,value in runnable_times_dict.items():
		runnable_times.append(value)
	for key,value in blocked_times_dict.items():
		blocked_times.append(value)
	for key,value in initializing_times_dict.items():
		initializing_times.append(value)
	#return  np.median(running_times), np.median(queuing_times), np.median(runnable_times), np.median(blocked_times), np.median(initializing_times)
	print(np.median(running_times), np.median(queuing_times), np.median(runnable_times), np.median(blocked_times), np.median(initializing_times))

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

