import sys
import json
from collections import defaultdict
import numpy as np

def def_list_value():
	return [0, 0, 0, 0, 0]
def def_value():
        return 0

def count_miss_or_meet_deadline_requests(file_dir, percentage):
	### each time for a request
	request_times_dict = defaultdict(def_list_value)
	#### get execution time
	running_time_dict = defaultdict(def_value)
	queuing_times_dict = defaultdict(def_value)
	total_times_dict = defaultdict(def_value)
	runnable_times_dict = defaultdict(def_value)
	blocked_times_dict = defaultdict(def_value)
	initializing_times_dict = defaultdict(def_value)
	execution_times_dict = defaultdict(def_value)

	running_times = defaultdict(list) 
	queuing_times = defaultdict(list)
	total_times = defaultdict(list)
	runnable_times = defaultdict(list)
	blocked_times = defaultdict(list)
	initializing_times = defaultdict(list)
	execution_times = defaultdict(list)
	####
	request_counter = defaultdict(def_value)
	total_time_dist = defaultdict(list)
	total_workload_dist = defaultdict(def_value)
	total_real_time_workload_dist = defaultdict(def_value)
	real_time_workload_times_dist = defaultdict(list)
	real_time_workload_workloads_dist = defaultdict(list)
	real_time_workload_requests_dist = defaultdict(list)
	min_time = sys.maxsize 
	# list[0] is meet deadline number, list[1] is miss deadline number
	delays_dict = defaultdict(list)
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
			func = line.split(" ")[11]
			delays_dict[func].append(delay)
			request_counter[name] += 1
			total_time_dist[name].append(total_time)
			miss_deadline_dist[name] += 1
			#print("name:", name)
		if "scheduling count" in line:
			s_c = int(line.split(" ")[3])
			if max_sc < s_c:
				max_sc = s_c
		if "total workload" in line:
			thread = line.split(" ")[3]
			time = line.split(" ")[1]
			if min_time > int(time):
				min_time = int(time)
			real_time_workload = line.split(" ")[11]
			total_workload = int(line.split(" ")[8])
			total_real_time_workload = int(line.split(" ")[15])
			real_time_request = line.split(" ")[5]
			real_time_workload_times_dist[thread].append(int(time))
			real_time_workload_workloads_dist[thread].append(int(real_time_workload))
			real_time_workload_requests_dist[thread].append(int(real_time_request))

			if total_workload_dist[thread] < total_workload:
				total_workload_dist[thread] = total_workload
			if total_real_time_workload_dist[thread] < total_real_time_workload:
				total_real_time_workload_dist[thread] = total_real_time_workload
		### calculate the execution time
		if "memory" in line or "total_time" in line or "min" in line or "miss" in line or "meet" in line or "time " in line or "scheduling count" in line:
                	continue
		t = line.split(",")
		id = t[1]
		func_idx = t[2][-9]
		joined_key = id + "_" + func_idx
		running_time_dict[joined_key] += int(t[9])
		queuing_times_dict[joined_key] += int(t[6])
		total_times_dict[joined_key] += int(t[5])
		runnable_times_dict[joined_key] += int(t[8])
		blocked_times_dict[joined_key] += int(t[10])
		initializing_times_dict[joined_key] += int(t[7])

		### get each time for a request
		request_times_dict[id][0] += int(t[6]) # global queuing time
		request_times_dict[id][1] += int(t[8]) # local queuing time
		request_times_dict[id][2] += int(t[10]) # blocking time
		request_times_dict[id][3] += int(t[5]) # total latency
		### 

	miss_deadline_percentage = (miss_deadline * 100) / (miss_deadline + meet_deadline)
	print("meet deadline num:", meet_deadline)
	print("miss deadline num:", miss_deadline)
	print("total num:", meet_deadline + miss_deadline)
	print("miss deadline percentage:", miss_deadline_percentage)
	print("scheduling counter:", max_sc)

	func_name_dict = {
		"cifar10_1": "105k",
		"cifar10_2": "305k",
		"cifar10_3": "5k",
		"cifar10_4": "40k",
		"resize": "resize",
		"fibonacci": "fibonacci"
	}
	func_name_with_id = {
		"1": "105k",
                "2": "305k",
                "3": "5k",
                "4": "40k"
	}
	### get execution time
	for key,value in running_time_dict.items():
		func_idx = key.split("_")[1]
		running_times[func_idx].append(value)
	for key,value in queuing_times_dict.items():
		func_idx = key.split("_")[1]
		queuing_times[func_idx].append(value)
	for key,value in runnable_times_dict.items():
		func_idx = key.split("_")[1]
		runnable_times[func_idx].append(value)
	for key,value in blocked_times_dict.items():
		func_idx = key.split("_")[1]
		blocked_times[func_idx].append(value)
	for key,value in initializing_times_dict.items():
		func_idx = key.split("_")[1]
		initializing_times[func_idx].append(value)
	for key,value in total_times_dict.items():
		func_idx = key.split("_")[1]
		total_times[func_idx].append(value)

	for key,value in request_counter.items():
		print(func_name_dict[key], ":", str(value), "proportion:", (100*value)/(meet_deadline + miss_deadline))
	for key,value in total_time_dist.items():
		a = np.array(value)
		p = np.percentile(a, int(percentage))
		print(func_name_dict[key] + " " + percentage + " percentage is:" + str(p) + " mean is:" + str(np.mean(value)) + " max latency is:" + str(max_latency_dist[key]))
	for key,value in meet_deadline_dist.items():
		miss_value = miss_deadline_dist[key]
		total_request = miss_value + value
		miss_rate = (miss_value * 100) / total_request
		
		print(func_name_dict[key] + " miss deadline rate:" + str(miss_rate) + " miss count is:" + str(miss_value) + " total request:" + str(total_request))
	for key,value in real_time_workload_times_dist.items():
		real_time_workload_times_dist[key] = [x - min_time for x in value]

	for key,value in running_times.items():
		#print("function:", key, func_name_with_id[key], key)
		print("function:", key)
		print(np.median(total_times[key]), np.median(running_times[key]), np.median(queuing_times[key]), np.median(runnable_times[key]), np.median(blocked_times[key]), np.median(initializing_times[key]))
	for key, value in delays_dict.items():  
		new_value = [i/1000 for i in value]
		p99 = np.percentile(new_value, 99)
		print("function:", key, " delays:", p99)
		
	total_workload = 0
	with open("total_workload.txt", "w") as f:
		for key,value in total_workload_dist.items():
			total_workload += value
			#print("thread " + key + " total workload:" + str(value))
			pair = [key + " "]
			pair.append(str(value))
			f.writelines(pair)
			f.write("\n")
	print("total workload is:", total_workload)

	with open("total_real_time_workload.txt", "w") as f:
                for key,value in total_real_time_workload_dist.items():
                        #print("thread " + key + " total real time workload:" + str(value))
                        pair = [key + " "]
                        pair.append(str(value))
                        f.writelines(pair)
                        f.write("\n")

	js = json.dumps(total_time_dist)
	f = open("total_time.txt", 'w')
	f.write(js)
	f.close()
	js2 = json.dumps(real_time_workload_times_dist)
	f2 = open("real_workload_times.txt", 'w')
	f2.write(js2)
	f2.close()

	js3 = json.dumps(real_time_workload_workloads_dist)
	f3 = open("real_workload_workloads.txt", 'w')
	f3.write(js3)
	f3.close()

	js4 = json.dumps(real_time_workload_requests_dist)
	f4 = open("real_workload_requests.txt", 'w')
	f4.write(js4)
	f4.close()

	js5 = json.dumps(running_times)
	f5 = open("running_time.txt", 'w')
	f5.write(js5)
	f5.close()

	js6 = json.dumps(total_times)
	f6 = open("total_time2.txt", 'w')
	f6.write(js6)
	f6.close()

	js7 = json.dumps(delays_dict)
	f7 = open("delays.txt", 'w')
	f7.write(js7)
	f7.close()

	js8 = json.dumps(request_times_dict)
	f8 = open("request_time.txt", 'w')
	f8.write(js8)
	f8.close()
	
	for key,value in total_time_dist.items():
                print(key + ": time list length is ", len(value))
if __name__ == "__main__":
	argv = sys.argv[1:]
	if len(argv) < 1:
		print("usage ", sys.argv[0], " file dir" " percentage")
		sys.exit()
	count_miss_or_meet_deadline_requests(argv[0], argv[1])