import sys
import json
from collections import defaultdict
import numpy as np

def def_value():
        return 0

def count_miss_or_meet_deadline_requests(file_dir, percentage):
	request_counter = defaultdict(def_value)
	total_time_dist = defaultdict(list)
	# list[0] is meet deadline number, list[1] is miss deadline number
	delays = 0
	max_latency_dist = defaultdict(def_value) 
	total_deadline = 0
	miss_deadline_dist = defaultdict(def_value) 
	meet_deadline_dist = defaultdict(def_value)
	meet_deadline = 0
	miss_deadline = 0
	max_sc = 0
	fo = open(file_dir, "r+")
	for line in fo:
		line = line.strip()
		if "meet deadline" in line:
			meet_deadline += 1
			name = line.split(" ")[8]
			request_counter[name] += 1
			total_time = int(line.split(" ")[5])
			total_time_dist[name].append(total_time)
			if total_time > max_latency_dist[name]:
				max_latency_dist[name] = total_time
			meet_deadline_dist[name] += 1
		if "miss deadline" in line:
			miss_deadline += 1
			name = line.split(" ")[11]
			total_time = int(line.split(" ")[8])
			if total_time > max_latency_dist[name]:
                                max_latency_dist[name] = total_time
			delay = int(line.split(" ")[4])
			delays += delay
			request_counter[name] += 1
			total_time_dist[name].append(total_time)
			miss_deadline_dist[name] += 1
			#print("name:", name)
		if "scheduling count" in line:
			s_c = int(line.split(" ")[3])
			if max_sc < s_c:
				max_sc = s_c
	miss_deadline_percentage = (miss_deadline * 100) / (miss_deadline + meet_deadline)
	print("meet deadline num:", meet_deadline)
	print("miss deadline num:", miss_deadline)
	print("total num:", meet_deadline + miss_deadline)
	print("miss deadline percentage:", miss_deadline_percentage)
	print("total delays:", delays)
	print("scheduling counter:", max_sc)

	for key,value in request_counter.items():
		print(key + ":" + str(value))
	for key,value in total_time_dist.items():
		a = np.array(value)
		p = np.percentile(a, int(percentage))
		print(key + " " + percentage + " percentage is:" + str(p) + " mean is:" + str(np.mean(value)) + " max latency is:" + str(max_latency_dist[key]))
	for key,value in meet_deadline_dist.items():
		miss_value = miss_deadline_dist[key]
		total_request = miss_value + value
		miss_rate = (miss_value * 100) / total_request
		print(key + " miss deadline rate:" + str(miss_rate) + " miss count is:" + str(miss_value))
	js = json.dumps(total_time_dist)
	f = open("total_time.txt", 'w')
	f.write(js)
	f.close()
	#for key,value in total_time_dist.items():
        #        print(key + ":", value)
if __name__ == "__main__":
	argv = sys.argv[1:]
	if len(argv) < 1:
		print("usage ", sys.argv[0], " file dir" " percentage")
		sys.exit()
	count_miss_or_meet_deadline_requests(argv[0], argv[1])
