import re
import os
import sys
from collections import defaultdict

miss_rate_dict = defaultdict(lambda: defaultdict(list))
seperate_meet_dict = defaultdict(lambda: defaultdict(list))
total_meet_dict = defaultdict(list)
total_miss_rate_dict = defaultdict(list)
weighted_miss_rate_dict = defaultdict(list)
load_dict = defaultdict(int)
total_requests_dict = defaultdict(list)


#get all file names which contain key_str
def file_name(file_dir, key_str):
    print(file_dir, key_str)
    file_list = []
    rps_list = []
    last_num_list = []

    for root, dirs, files in os.walk(file_dir):
        print("file:", files)
        print("root:", root)
        print("dirs:", dirs)
        for file_i in files:
            if file_i.find(key_str) >= 0:
                full_path = os.path.join(os.getcwd() + "/" + root, file_i)
                print(full_path)
                segs = file_i.split('-')
                if len(segs) < 2:
                  continue
                rps=segs[1]
                print("rps---------", rps)
                rps=rps.split(".")[0]
                file_list.append(full_path)
                rps_list.append(int(rps))
                last_num=segs[2]
                last_num=last_num.split(".")[0]
                last_num_list.append(int(last_num))		

    pattern = r"server-(\d+)-\d+(\.\d+)?\.log"

    for file in file_list:
        match = re.search(pattern, file)
        if match:
            print(f"Matched: {file}, RPS: {match.group(1)}")
        else:
            print(f"Not Matched: {file}", file)

    file_list_based_throughput = sorted(file_list, key=lambda x: float(re.search(pattern, x).group(1)))
    rps_list = sorted(rps_list)
    last_num_list = sorted(last_num_list)
    # Sort the list based on the last number in the filename
    file_list_based_last_num = sorted(file_list, key=lambda x: int(x.split('-')[-1].split('.')[0]))

    #print(file_list_based_last_num)
    print("--------------------------------", rps_list)
    return file_list_based_throughput, file_list_based_last_num, rps_list, last_num_list

def get_values(key, files_list, latency_dict, slow_down_dict, slow_down_99_9_dict, latency_99_9_dict, slow_down_99_99_dict, latency_99_99_dict):
	for file_i in files_list:
		cmd='sudo python3 ./meet_deadline_percentage.py %s 99' % file_i
		rt=os.popen(cmd).read().strip()	
		print(rt)
		# Define regular expressions to match the desired values
		latency_rule = r'99 percentile latency is\s*([\d.]+)'
		slow_down_rule = r'99 percentile slow down is\s*([\d.]+)'
		latency_99_9_rule = r'99.9 percentile latency is\s*([\d.]+)'
		slow_down_99_9_rule = r'99.9 percentile slow down is\s*([\d.]+)'
		latency_99_99_rule = r'99.99 percentile latency is\s*([\d.]+)'
		slow_down_99_99_rule = r'99.99 percentile slow down is\s*([\d.]+)'

		miss_rate_rule = r"type\s+(\d+)\s+miss\s+deadline\s+rate:\s+([\d.]+)"
		total_meet_requests_rule = r"type\s+(\d+)\s+total\s+meet\s+requests:\s+([\d.]+)"
		total_requests_rule = r"type\s+(\d+)\s+meet\s+requests:\s+([\d.]+)"
		total_miss_rate_rule = r"miss\s+deadline\s+percentage:\s+([\d.]+)"
		weighted_miss_rate_rule = r"weighted\s+miss\s+rate:\s+([\d.]+)"
		#total_requests_rule = r"total\s+requests:\s+([\d.]+)"

		# Use the regular expressions to find the values
		latency_match = re.search(latency_rule, rt)
		slow_down_match = re.search(slow_down_rule, rt)
		latency_99_9_match = re.search(latency_99_9_rule, rt)
		slow_down_99_9_match = re.search(slow_down_99_9_rule, rt)
		latency_99_99_match = re.search(latency_99_99_rule, rt)
		slow_down_99_99_match = re.search(slow_down_99_99_rule, rt)
		total_miss_rate_match = re.search(total_miss_rate_rule, rt)	
		weighted_miss_rate_match = re.search(weighted_miss_rate_rule, rt)	
		total_requests_match = re.search(total_requests_rule, rt)
	
		# Check if matches were found and extract the values
		if latency_match:
			latency_value = 0
			latency_value = latency_match.group(1)
			print("99th latency is:", latency_value)
			latency_dict[key].append(latency_value)
		
		if slow_down_match:
			slow_down_value = 0
			slow_down_value = slow_down_match.group(1)
			print("99th slow down is:", slow_down_value)
			slow_down_dict[key].append(slow_down_value)

		if latency_99_9_match:
			latency_value = 0
			latency_value = latency_99_9_match.group(1)
			print("99.9th latency is:", latency_value)
			latency_99_9_dict[key].append(latency_value)
		
		if slow_down_99_9_match:
			slow_down_value = 0
			slow_down_value = slow_down_99_9_match.group(1)
			print("99.9th slow down is:", slow_down_value)
			slow_down_99_9_dict[key].append(slow_down_value)
		
		if latency_99_99_match:
			latency_value = 0
			latency_value = latency_99_99_match.group(1)
			print("99.99th latency is:", latency_value)
			latency_99_99_dict[key].append(latency_value)
		
		if slow_down_99_99_match:
			slow_down_value = 0
			slow_down_value = slow_down_99_99_match.group(1)
			print("99.99th slow down is:", slow_down_value)
			slow_down_99_99_dict[key].append(slow_down_value)

		if total_miss_rate_match:
			total_miss_rate = 0
			total_miss_rate = total_miss_rate_match.group(1)
			print("total miss rate for ", key, " is:", total_miss_rate) 
			total_miss_rate_dict[key].append(total_miss_rate)

		if weighted_miss_rate_match:
			weighted_miss_rate = 0
			weighted_miss_rate = weighted_miss_rate_match.group(1)
			print("weighted miss rate for ", key, " is:", weighted_miss_rate)
			weighted_miss_rate_dict[key].append(weighted_miss_rate)

		total_meet = 0
		for match in re.finditer(total_meet_requests_rule, rt):
			r_type, meet_requests = match.groups()
			print("type:", r_type, "meet requests:", meet_requests)
			total_meet = total_meet + int(meet_requests)
			seperate_meet_dict[key][int(r_type)].append(meet_requests)
		total_meet_dict[key].append(total_meet)	
		if total_requests_match:
			total_requests = 0
			total_requests = total_requests_match.group(1)
			print("total request for ", key, " is:", total_requests)
			total_requests_dict[key].append(total_requests)

		for match in re.finditer(miss_rate_rule, rt):
                    r_type, miss_rate = match.groups()
                    print("type:", r_type, "miss rate:", miss_rate)
                    miss_rate_dict[key][int(r_type)].append(float(miss_rate))


if __name__ == "__main__":
    import json
    #file_folders = ['SHINJUKU', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    file_folders = ['SHINJUKU', 'DARC', 'EDF_INTERRUPT']
    #file_folders = ['SHINJUKU']
    latency = defaultdict(list)
    slow_down = defaultdict(list)
    slow_down_99_9 = defaultdict(list)
    latency_99_9 = defaultdict(list)
    slow_down_99_99 = defaultdict(list)
    latency_99_99 = defaultdict(list)

    rps_list = []

    argv = sys.argv[1:]
    if len(argv) < 2:
        print("usage ", sys.argv[0], "[file key] [file order, 1 is throughput, 2 is last number]")
        sys.exit()
    
    file_order = argv[1]
    for key in file_folders:
        files_list, file_list_based_last_num, rps_list, last_num_list = file_name(key, argv[0])
        load_dict[key] = rps_list
        if file_order == "1":
            get_values(key, files_list, latency, slow_down, slow_down_99_9, latency_99_9, slow_down_99_99, latency_99_99)
        else:
            get_values(key, file_list_based_last_num, latency, slow_down, slow_down_99_9, latency_99_9, slow_down_99_99, latency_99_99)

    print("99 latency:")
    for key, value in latency.items():
        print(key, ":", value)
    print("99 slow down:")
    for key, value in slow_down.items():
        print(key, ":", value)

    js1 = json.dumps(latency)
    f1 = open("99_latency.txt", 'w')
    f1.write(js1)
    f1.close()
    
    js2 = json.dumps(slow_down)
    f2 = open("99_slow_down.txt", 'w')
    f2.write(js2)
    f2.close() 

    js4 = json.dumps(latency_99_9)
    f4 = open("99_9_latency.txt", 'w')
    f4.write(js4)
    f4.close()

    js5 = json.dumps(slow_down_99_9)
    f5 = open("99_9_slow_down.txt", 'w')
    f5.write(js5)
    f5.close()

    js6 = json.dumps(latency_99_99)
    f6 = open("99_99_latency.txt", 'w')
    f6.write(js6)
    f6.close()

    js7 = json.dumps(slow_down_99_99)
    f7 = open("99_99_slow_down.txt", 'w')
    f7.write(js7)
    f7.close()

    js3 = json.dumps(rps_list)
    f3 = open("rps.txt", 'w')
    f3.write(js3)
    f3.close()
 
    js8 = json.dumps(miss_rate_dict)
    f8 = open("seperate_miss_rate.txt", 'w')
    f8.write(js8)
    f8.close()

    js9 = json.dumps(total_miss_rate_dict)
    f9 = open("total_miss_rate.txt", 'w')
    f9.write(js9)
    f9.close()

    js10 = json.dumps(load_dict)
    f10 = open("sload.txt", 'w')
    f10.write(js10)
    f10.close()

    js11 = json.dumps(weighted_miss_rate_dict)
    f11 = open("weighted_miss_rate.txt", 'w')
    f11.write(js11)
    f11.close()

    js12 = json.dumps(total_requests_dict)
    f12 = open("total_requests.txt", 'w')
    f12.write(js12)
    f12.close()

    js13 = json.dumps(total_meet_dict)
    f13 = open("total_meet.txt", 'w')
    f13.write(js13)
    f13.close()

    js14 = json.dumps(seperate_meet_dict)
    f14 = open("seperate_meet.txt", 'w')
    f14.write(js14)
    f14.close()

    js15 = json.dumps(last_num_list)
    f15 = open("last_num.txt", 'w')
    f15.write(js15)
    f15.close()
