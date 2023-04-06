import sys
import json
from collections import defaultdict
import numpy as np

def def_list_value():
	return [0, 0, 0, 0, 0]
def def_value():
        return 0

def count_miss_or_meet_deadline_requests(file_dir, percentage):
	throughput = 0;
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

	### init overhead
	### queuelength
	queuelength_dict = defaultdict(list)
	###
	running_times = defaultdict(list) 
	queuing_times = defaultdict(list)
	thread_running_times = defaultdict(list)
	total_times = defaultdict(list)
	thread_times = defaultdict(list)
	thread_throughput = defaultdict(list)
	t2_cleanup = defaultdict(list)
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
		if "meet" in line:
			meet_deadline += 1
			name = line.split(" ")[7]
			cleanup = line.split(" ")[9]
			name = name[1:]
			tid = line.split(" ")[1]
			request_counter[name] += 1
			total_time = int(line.split(" ")[4])
			total_time_dist[name].append(total_time)
			thread_times[tid].append(total_time)	
			if total_time > max_latency_dist[name]:
				max_latency_dist[name] = total_time
			meet_deadline_dist[name] += 1
			exe_time = int(line.split(" ")[5])
			running_times[name].append(exe_time);
			queue_time = int(line.split(" ")[6])
			queuing_times[name].append(queue_time);	
			thread_running_times[tid].append(exe_time);	
			t2_cleanup[tid].append(cleanup)
		if "miss" in line:
			miss_deadline += 1
			name = line.split(" ")[7]
			cleanup = line.split(" ")[9]
			name = name[1:]
			tid = line.split(" ")[1]
			total_time = int(line.split(" ")[4])
			if total_time > max_latency_dist[name]:
                                max_latency_dist[name] = total_time
			request_counter[name] += 1
			total_time_dist[name].append(total_time)
			thread_times[tid].append(total_time)
			miss_deadline_dist[name] += 1
			exe_time = line.split(" ")[5]
			exe_time = int(line.split(" ")[5])
			running_times[name].append(exe_time);
			queue_time = int(line.split(" ")[6])
			queuing_times[name].append(queue_time);
			thread_running_times[tid].append(exe_time);
			t2_cleanup[tid].append(cleanup)
			#print("name:", name)
		if "throughput" in line:
			throughput = line.split(" ")[1]
		### calculate the execution time
		#if "memory" in line or "total_time" in line or "min" in line or "miss" in line or "meet" in line or "time " in line or "scheduling count" in line or "thread id" in line:
                #	continue
		if "pthroughput" in line:
			tid = line.split(" ")[2]
			throughput = line.split(" ")[3]
			thread_throughput[tid].append(throughput)

	miss_deadline_percentage = (miss_deadline * 100) / (miss_deadline + meet_deadline)
	print("meet deadline num:", meet_deadline)
	print("miss deadline num:", miss_deadline)
	print("total requests:", meet_deadline + miss_deadline)
	print("miss deadline percentage:", miss_deadline_percentage)
	print("throughput:", throughput)

	for key,value in request_counter.items():
		print(key, ":", str(value), "proportion:", (100*value)/(meet_deadline + miss_deadline))
	for key,value in total_time_dist.items():
		a = np.array(value)
		p = np.percentile(a, int(percentage))
		print(key + " " + percentage + " percentage is:" + str(p) + " mean is:" + str(np.mean(value)) + " max latency is:" + str(max_latency_dist[key]))
	#total_cpu_times = 0
	#for key,value in meet_deadline_dist.items():
	#	total_cpu_times += value * fun_execution_time[key]
	#	miss_value = miss_deadline_dist[key]
	#	total_request = miss_value + value
	#	miss_rate = (miss_value * 100) / total_request
	#	
	#	print(func_name_dict[key] + " miss deadline rate:" + str(miss_rate) + " miss count is:" + str(miss_value) + " total request:" + str(total_request))
	#print("effective total cpu times:", total_cpu_times)
	#for key,value in real_time_workload_times_dist.items():
	#	real_time_workload_times_dist[key] = [x - min_time for x in value]

	for key,value in running_times.items():
		#print("function times:", func_name_with_id[key], np.median(total_times[key]), np.median(running_times[key]), np.median(queuing_times[key]), np.median(runnable_times[key]), np.median(blocked_times[key]), np.median(initializing_times[key]))
		print("function :", key, "median total:", np.median(total_time_dist[key]), "exec:", np.median(running_times[key]), "queue:", np.median(queuing_times[key]))
		print(len(value))
		print(len(queuing_times[key]))
		print(len(total_time_dist[key]))	
	js = json.dumps(total_time_dist)
	f = open("total_time.txt", 'w')
	f.write(js)
	f.close()

	js1 = json.dumps(queuing_times)
	f1 = open("queuing_time.txt", 'w')
	f1.write(js1)
	f1.close()

	js5 = json.dumps(thread_running_times)
	f5 = open("running_time.txt", 'w')
	f5.write(js5)
	f5.close()

	js2 = json.dumps(thread_times)
	f2 = open("thread_time.txt", 'w')
	f2.write(js2)
	f2.close()

	js3 = json.dumps(thread_throughput)
	f3 = open("thread_throughput.txt", 'w')
	f3.write(js3)
	f3.close()

	js4 = json.dumps(t2_cleanup)
	f4 = open("cleanup.txt", 'w')
	f4.write(js4)
	f4.close()

if __name__ == "__main__":
	argv = sys.argv[1:]
	if len(argv) < 1:
		print("usage ", sys.argv[0], " <file dir>" " <percentage>")
		sys.exit()
	count_miss_or_meet_deadline_requests(argv[0], argv[1])
