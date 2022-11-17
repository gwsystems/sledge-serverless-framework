import json
import re
import os
import sys
from collections import defaultdict

#get all file names which contain key_str
def file_name(file_dir, key_str): 
    cost_table = defaultdict(list)
    for root, dirs, files in os.walk(file_dir):
        if root != os.getcwd():
            continue
        for file_i in files:
            if file_i.find(key_str) >= 0:
                segs = file_i.split('_')      
                if len(segs) < 2:
                  print("less than 2 segs", file_i)
                  continue
                #print(file_i)
                name = segs[0]
                #print("core:", cores_num, " concurrency:", concurrency)
                get_values(name, file_i, cost_table)
                #file_table[key].append(file_i)
        #s_result = sorted(throughput_table.items())
        #for i in range(len(s_result)):
        #   print(s_result[i])
        #for i in range(len(s_result)):
        #    print(int(float(((s_result[i][1][0])))),end=" ")
        #print();
        for key, value in cost_table.items():
            print(key, len(value))
        js = json.dumps(cost_table)
        f = open("cost.txt", 'w')
        f.write(js)
        f.close()


def get_values(name, file_name, cost_table):
    fo = open(file_name, "r+")
    for line in fo:
        line = line.strip()
        if line[0].isdigit():
            str_list = line.split(" ")
            for i in range(len(str_list)):
                cost_table[name].append(int(str_list[i]))

if __name__ == "__main__":
    import json
    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], " file containing key word")
        sys.exit()

    file_name(os.getcwd(), argv[0])

    #for key, value in files_tables.items():
    #    get_values(key, value, miss_deadline_rate, total_latency, running_times, preemption_count, total_miss_deadline_rate) 

