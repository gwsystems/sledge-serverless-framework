import re
import os
import sys
from collections import defaultdict

#get all file names which contain key_str
def file_name(file_dir, key_str): 
    throughput_table = defaultdict(list)
    errors_table = defaultdict(list)
    for root, dirs, files in os.walk(file_dir):
        if root != os.getcwd():
            continue
        for file_i in files:
            if file_i.find(key_str) >= 0:
                segs = file_i.split('-')      
                if len(segs) < 3:
                  continue
                #print(file_i)
                cores_num = segs[1]
                concurrency = segs[3].split(".")[0]
                print("core:", cores_num, " concurrency:", concurrency)
                get_values(cores_num, concurrency, file_i, throughput_table, errors_table)
                #file_table[key].append(file_i)
        s_result = sorted(throughput_table.items())
        #for i in range(len(s_result)):
        #    print(s_result[i], "errors request:", errors_table[s_result[i][0]])
        for i in range(len(s_result)):
            print(int(float(((s_result[i][1][0])))),end=" ")
        print();
        #sys.exit()

def get_values(core, concurrency, file_name, throughput_table, errors_table):
    print("parse file:", file_name)
    fo = open(file_name, "r+")
    total_throughput = 0

    for line in fo:
        line = line.strip()
        if "throughput is" in line:
            #i_th = float(line.split(" ")[2])
            i_th = float(line.split(" ")[2].split(",")[0])
            total_throughput += i_th

    throughput_table[int(core)].append(total_throughput)
    print("core ", core, " total throughput ", total_throughput)
    #cmd2='grep "throughput is" %s | awk \'{print $7}\'' % file_name
    #rt2=os.popen(cmd2).read().strip()
    #if len(rt2) != 0:
    #    errors = rt2.splitlines()[0]
    #    errors_table[int(core)].append(int(errors))
    #else:
    #    errors_table[int(core)].append(0)
    #print(file_name, rt2)
    

if __name__ == "__main__":
    import json
    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " file containing key word")
        sys.exit()

    file_name(os.getcwd(), argv[0])

    #for key, value in files_tables.items():
    #    get_values(key, value, miss_deadline_rate, total_latency, running_times, preemption_count, total_miss_deadline_rate) 

