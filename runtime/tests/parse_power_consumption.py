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

def get_values(key, files_list, core_energy_consum_dict, pkg_energy_consum_dict):
	for file_i in files_list:
		with open(file_i, 'r') as file:
			print(file_i)
			lines = file.readlines()

		core_line_index = next(i for i, line in enumerate(lines) if "Domain CORE" in line)
		pkg_line_index = next(i for i, line in enumerate(lines) if "Domain PKG" in line)

		core_value = float(lines[core_line_index+1].split()[-2])
		pkg_value = float(lines[pkg_line_index+1].split()[-2])

		#print("Domain CORE 的值:", core_value)
		#print("Domain PKG 的值:", pkg_value)

		core_energy_consum_dict[key].append(core_value)
		pkg_energy_consum_dict[key].append(pkg_value)

 
if __name__ == "__main__":
    import json
    #file_folders = ['SHINJUKU', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU_7', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU', 'DARC', 'EDF_SRSF_INTERRUPT']
    file_folders = ['EDF_INTERRUPT-disable-busy-loop-true-disable-autoscaling-true-27', 'EDF_INTERRUPT-disable-busy-loop-true-disable-autoscaling-false-27']
    #file_folders = ['EDF_INTERRUPT','EDF_SRSF_INTERRUPT_1']
    #file_folders = ['DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU']
    core_energy_consum_dict = defaultdict(list)
    pkg_energy_consum_dict = defaultdict(list)
    rps_list = []

    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], "[file key]")
        sys.exit()

    for key in file_folders:
        files_list, rps_list = file_name(key, argv[0])
        get_values(key, files_list, core_energy_consum_dict, pkg_energy_consum_dict)

    print("core consume:")
    for key, value in core_energy_consum_dict.items():
        print(key, ":", value)
    for key, value in pkg_energy_consum_dict.items():
        print(key, ":", value)

    js1 = json.dumps(core_energy_consum_dict)
    f1 = open("core_consume.txt", 'w')
    f1.write(js1)
    f1.close()
   
    js2 = json.dumps(pkg_energy_consum_dict)
    f2 = open("pkg_consume.txt", 'w')
    f2.write(js2)
    f2.close()

