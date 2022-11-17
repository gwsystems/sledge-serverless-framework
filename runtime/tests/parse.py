import re
import os
import sys
from collections import defaultdict

#get all file names which contain key_str
def file_name(file_dir, key_str): 
    throughput_table = defaultdict(list)
    for root, dirs, files in os.walk(file_dir):
        if root != os.getcwd():
            continue
        for file_i in files:
            if file_i.find(key_str) >= 0:
                segs = file_i.split('-')      
                if len(segs) < 3:
                  continue
                #print(file_i)
                cores_num = segs[0]
                concurrency = segs[2].split(".")[0]
                #print("core:", cores_num, " concurrency:", concurrency)
                get_values(cores_num, concurrency, file_i, throughput_table)
                #file_table[key].append(file_i)
        s_result = sorted(throughput_table.items())
        for i in range(len(s_result)):
            print(s_result[i])

def get_values(core, concurrency, file_name, throughput_table):
    cmd='grep "Requests/sec:" %s | awk \'{print $2}\'' % file_name
    #cmd='python3 ~/sledge-serverless-framework/runtime/tests/meet_deadline_percentage.py %s 50' % file_name
    rt=os.popen(cmd).read().strip()
    #print(file_name, rt)
    throughput_table[int(core)].append(rt)
    

if __name__ == "__main__":
    import json
    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " file containing key word")
        sys.exit()

    file_name(os.getcwd(), argv[0])

    #for key, value in files_tables.items():
    #    get_values(key, value, miss_deadline_rate, total_latency, running_times, preemption_count, total_miss_deadline_rate) 

