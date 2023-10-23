import re
import os
import sys
from collections import defaultdict

#get all file names which contain key_str
def file_name(file_dir, key_str):
    print(file_dir, key_str)
    file_list = []
    rps_list = []

    for root, dirs, files in os.walk(file_dir):
        print(files, root, dirs)
        for file_i in files:
            if file_i.find(key_str) >= 0:
                full_path = os.path.join(os.getcwd() + "/" + root, file_i)
                #print(full_path)
                segs = file_i.split('-')
                if len(segs) < 2:
                  continue
                rps=segs[1]
                rps=rps.split(".")[0]
                file_list.append(full_path)
                rps_list.append(rps)
        
    file_list = sorted(file_list, key = lambda x: int(x.split('-')[-1].split(".")[0]))
    rps_list = sorted(rps_list)
    print(file_list)
    print(rps_list)
    return file_list, rps_list

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

		# Use the regular expressions to find the values
		latency_match = re.search(latency_rule, rt)
		slow_down_match = re.search(slow_down_rule, rt)
		latency_99_9_match = re.search(latency_99_9_rule, rt)
		slow_down_99_9_match = re.search(slow_down_99_9_rule, rt)
		latency_99_99_match = re.search(latency_99_99_rule, rt)
		slow_down_99_99_match = re.search(slow_down_99_99_rule, rt)
		
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

if __name__ == "__main__":
    import json
    #file_folders = ['SHINJUKU', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    file_folders = ['SHINJUKU_5', 'SHINJUKU_100', 'SHINJUKU_200', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU']
    latency = defaultdict(list)
    slow_down = defaultdict(list)
    slow_down_99_9 = defaultdict(list)
    latency_99_9 = defaultdict(list)
    slow_down_99_99 = defaultdict(list)
    latency_99_99 = defaultdict(list)

    rps_list = []

    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], "[file key]")
        sys.exit()

    for key in file_folders:
        files_list, rps_list = file_name(key, argv[0])
        get_values(key, files_list, latency, slow_down, slow_down_99_9, latency_99_9, slow_down_99_99, latency_99_99)

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
