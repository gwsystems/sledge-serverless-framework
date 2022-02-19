import re
import os
import sys
from collections import defaultdict

#get all file names which contain key_str
def file_name(file_dir, key_str): 
    file_table = defaultdict(list)
    for root, dirs, files in os.walk(file_dir):
        if root != os.getcwd():
            continue
        for file_i in files:
            if file_i.find(key_str) >= 0:
                segs = file_i.split('_')      
                if len(segs) < 2:
                  continue
                print(file_i)
                key = segs[0]+"_" + segs[1]
                percentage=segs[-1].split(".")[0]
                print("percentage is ______________", percentage)
                file_table[key].append(file_i)
    for key,value in file_table.items():
        print(value)
        s_value = sorted(value, key = lambda x: int(x.split('_')[-1].split(".")[0]))
        file_table[key] = s_value
        #print("key is:", key, " value is:", value)
    #for key,value in file_table.items():
    #    print("key is:", key, " value is:", value)
    
    return file_table

def get_values(key, value, miss_deadline_rate, total_latency, running_times, preemption_count, total_miss_deadline_rate):
	for i in range(len(value)):
		file_name = value[i]
		print("file name++++++++++++++++++++++++++", file_name)
		before_dot = file_name.split(".")[0]
		joint_f_name = before_dot + "_total_time.txt"

		cmd='python3 ~/sledge-serverless-framework/runtime/tests/meet_deadline_percentage.py %s 50' % file_name
		rt=os.popen(cmd).read().strip()
		cmd2='mv total_time.txt %s' % joint_f_name
		os.popen(cmd2)
		#print(rt)
		rule=r'(.*?) miss deadline rate:(.*?) miss count is'
		finds=re.findall(rule, rt)
		finds_size=len(finds)
		for j in range(finds_size):
			func_name=finds[j][0]
			miss_rate=finds[j][1]
			key1=key+"_"+func_name
			percentage=file_name.split('_')[-1].split('.')[0]
			print("miss rate:", key1, percentage, miss_rate)
			miss_deadline_rate[key1][percentage]=round(float(miss_rate), 2)
	
		rule2=r'function times: (.*)'
		finds=re.findall(rule2, rt)
		for j in range(len(finds)):
			func_name=finds[j].split(" ")[0]
			latency=finds[j].split(" ")[1]
			running_time=finds[j].split(" ")[2]
			key1=key+"_"+func_name
			print("total latency:", func_name, latency)
			total_latency[key1][percentage]=latency
			running_times[key1][percentage]=running_time

		rule3=r'scheduling counter: (.*)'
		finds=re.findall(rule3, rt)
		schedule_count=finds[0].strip()
		preemption_count[key][percentage]=schedule_count
		#print("finds preemption count is ", schedule_count)

		rule4=r'miss deadline percentage: (.*)'
		finds=re.findall(rule4, rt)
		total_miss_rate=finds[0].strip()
		total_miss_deadline_rate[key][percentage]=total_miss_rate


if __name__ == "__main__":
    import json
    miss_deadline_rate = defaultdict(defaultdict)
    total_latency = defaultdict(defaultdict)
    running_times = defaultdict(defaultdict)
    preemption_count = defaultdict(defaultdict)
    total_miss_deadline_rate = defaultdict(defaultdict)

    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " function key word, 5k or 305k")
        sys.exit()

    files_tables = file_name(os.getcwd(), argv[0])
    n = len(files_tables)
    d = {}
    i = 0

    for key, value in files_tables.items():
        get_values(key, value, miss_deadline_rate, total_latency, running_times, preemption_count, total_miss_deadline_rate) 

    for key, value in miss_deadline_rate.items():
        print("miss deadline rate:", key, value)

    for key, value in total_latency.items():
        print("total latency:", key, value)

    for key, value in preemption_count.items():
        print("preemption:", key, value)

    js = json.dumps(miss_deadline_rate)
    f= open("miss_deadline_rate.txt", 'w')
    f.write(js)
    f.close()


    js2 = json.dumps(total_latency)
    f2= open("total_latency.txt", 'w')
    f2.write(js2)
    f2.close()


    js3 = json.dumps(preemption_count)
    f3= open("preemption.txt", 'w')
    f3.write(js3)
    f3.close()

    js4 = json.dumps(total_miss_deadline_rate)
    f4= open("total_miss_deadline_rate.txt", 'w')
    f4.write(js4)
    f4.close()

    js5 = json.dumps(running_times)
    f5 = open("execution_time.txt", "w")
    f5.write(js5)
    f5.close()

