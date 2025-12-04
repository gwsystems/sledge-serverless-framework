import re
import os
import sys
from collections import defaultdict

sending_rate_dict = defaultdict(list)
service_rate_dict = defaultdict(list)

#get all file names which contain key_str
def file_name(file_dir, key_str):
    print(file_dir, key_str)
    file_list = []
    rps_list = []

    for root, dirs, files in os.walk(file_dir):
        print("file:", files)
        print("root:", root)
        print("dirs:", dirs)
        for file_i in files:
            if file_i.find(key_str) >= 0:
                full_path = os.path.join(os.getcwd() + "/" + root, file_i)
                #print(full_path)
                segs = file_i.split('-')
                if len(segs) < 2:
                  continue
                rps=segs[1]
                print("rps---------", rps)
                rps=rps.split(".")[0]
                file_list.append(full_path)
                rps_list.append(rps)

    file_list = sorted(file_list, key = lambda x: int(x.split('-')[-1].split(".")[0]))
    rps_list = sorted(rps_list)
    print(file_list)
    print(rps_list)
    return file_list, rps_list

def get_values(key, files_list, cpu_usages_dict):
        for file_i in files_list:
                cmd="awk '/Average:/ {print $8}' %s" % file_i
                rt=os.popen(cmd).read().strip()
                cpu_count=float(rt)/100		
                cpu_count=round(cpu_count, 2)
                print(cpu_count)
                cpu_usages_dict[key].append(cpu_count)

 
if __name__ == "__main__":
    import json
    #file_folders = ['SHINJUKU', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU_7', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU', 'DARC', 'EDF_SRSF_INTERRUPT']
    file_folders = ['EDF_INTERRUPT-disable-busy-loop-false-disable-autoscaling-true-9','EDF_INTERRUPT-disable-busy-loop-true-disable-autoscaling-false-9', 'EDF_INTERRUPT-disable-busy-loop-true-disable-autoscaling-true-27']
    #file_folders = ['EDF_INTERRUPT','EDF_SRSF_INTERRUPT_1']
    #file_folders = ['DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU']
    cpu_usages_dict = defaultdict(list)
    rps_list = []

    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], "[file key]")
        sys.exit()

    for key in file_folders:
        files_list, rps_list = file_name(key, argv[0])
        get_values(key, files_list, cpu_usages_dict)

    print("cpu usage:")
    for key, value in cpu_usages_dict.items():
        print(key, ":", value)

    js1 = json.dumps(cpu_usages_dict)
    f1 = open("cpu.txt", 'w')
    f1.write(js1)
    f1.close()


