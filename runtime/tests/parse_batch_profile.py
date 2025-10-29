import re
import os
import sys
from collections import defaultdict

mem_dict = defaultdict(list)
funs_dict = defaultdict(list)

cache_miss_dict = defaultdict(list)
pagefault_dict = defaultdict(list)

seperate_sending_rate_dict = defaultdict(lambda: defaultdict(list))
seperate_service_rate_dict = defaultdict(lambda: defaultdict(list))

def parse_file(file_path):
    #key is request type, value is latency
    fo = open(file_path, "r+")
    deadline_miss_count = 0
    for line in fo:
        line = line.strip()
        #latency log line
        if "avg RSS" in line:
            mem = line.split(":")[1].strip()
            mem = int(mem)
            print("mem:", mem)
            return mem

#get all file names which contain key_str
def file_name(file_dir, key_str):
    print(file_dir, key_str)
    file_list = []
    funs_list = []

    for root, dirs, files in os.walk(file_dir):
        print("file:", files)
        print("root:", root)
        print("dirs:", dirs)
        for file_i in files:
            if file_i.find(key_str) >= 0:
                full_path = os.path.join(os.getcwd() + "/" + root, file_i)
                print(full_path)
                segs = file_i.split('_')
                if len(segs) < 2:
                  continue
                funs=segs[0].strip()
                print("funs---------", funs)
                file_list.append(full_path)
                funs_list.append(int(funs))

    #file_list = sorted(file_list, key = lambda x: int(x.split('-')[-1].split(".")[0]))
    file_list = sorted(file_list, key = lambda x: int(x.split(key_str)[0].split("/")[-1]))
    funs_list = sorted(funs_list)
    print(file_list)
    print("--------------------------------", funs_list)
    return file_list, funs_list

def get_values(key, files_list, mem_dict):
    for file_i in files_list:
        mem = parse_file(file_i)	
        mem_dict[key].append(mem)


def parse_perf(file_path):
    #key is request type, value is latency
    with open(file_path, 'r') as file:
        file_content = file.read()

    # 使用正则表达式来匹配所需的数据
    cache_miss_rate_pattern = r'([\d.]+)\s+% of all cache refs'
    page_faults_pattern = r'(\d[\d,]*)\s+page-faults'

    # 查找 % of all cache refs 的值
    cache_miss_rate_match = re.search(cache_miss_rate_pattern, file_content)
    cache_miss_rate = cache_miss_rate_match.group(1) if cache_miss_rate_match else None

    # 查找 page-faults 的值
    page_faults_match = re.search(page_faults_pattern, file_content)
    page_faults = page_faults_match.group(1).replace(',', '') if page_faults_match else None

    return float(cache_miss_rate), int(page_faults)

def get_perf(key, files_list, pagefault_dict, cache_miss_dict):
    for file_i in files_list:
        cache_miss, pagefault = parse_perf(file_i)
        pagefault_dict[key].append(pagefault)
        cache_miss_dict[key].append(cache_miss)

if __name__ == "__main__":
    import json
    #file_folders = ['SHINJUKU', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    #file_folders = ['SHINJUKU_7', 'SHINJUKU_25', 'DARC', 'EDF_SRSF_INTERRUPT']
    file_folders = ['Sledge_pool_per_module', 'Sledge_no_pool', 'EdgeScale']
    #file_folders = ['EdgeScale']

    argv = sys.argv[1:]
    if len(argv) < 1:
        print("usage ", sys.argv[0], "[file key]")
        sys.exit()

    for key in file_folders:
        files_list, funs_list = file_name(key, argv[0])
        funs_dict[key] = funs_list
        get_values(key, files_list, mem_dict)
       
        files_list, funs_list = file_name(key, "_perf")
        get_perf(key, files_list, pagefault_dict, cache_miss_dict)
	
    js1 = json.dumps(mem_dict)
    f1 = open("mem.txt", 'w')
    f1.write(js1)
    f1.close()

    js2 = json.dumps(funs_dict)
    f2 = open("funs.txt", 'w')
    f2.write(js2)
    f2.close()

    js3 = json.dumps(cache_miss_dict)
    f3 = open("cache_miss.txt", 'w')
    f3.write(js3)
    f3.close()

    js4 = json.dumps(pagefault_dict)
    f4 = open("pagefault.txt", 'w')
    f4.write(js4)
    f4.close()


