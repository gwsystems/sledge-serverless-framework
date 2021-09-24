import sys
from collections import defaultdict

def def_value():
        return 0

def count_miss_or_meet_deadline_requests(file_dir):
	request_counter =  defaultdict(def_value)
	meet_deadline = 0
	miss_deadline = 0
	max_sc = 0
	fo = open(file_dir, "r+")
	for line in fo:
		line = line.strip()
		if "meet deadline" in line:
			meet_deadline += 1
			name = line.split(" ")[5]
			request_counter[name] += 1
		if "miss deadline" in line:
			miss_deadline += 1
			name = line.split(" ")[11]
			request_counter[name] += 1
			#print("name:", name)
		if "scheduling count" in line:
			s_c = int(line.split(" ")[3])
			if max_sc < s_c:
				max_sc = s_c
	miss_deadline_percentage = (miss_deadline * 100) / (miss_deadline + meet_deadline)
	print("meet deadline num:", meet_deadline)
	print("miss deadline num:", miss_deadline)
	print("total num:", meet_deadline + miss_deadline)
	print("miss deadline percentage:", miss_deadline_percentage)
	print("scheduling counter:", max_sc)

	for key,value in request_counter.items():
		print(key + ":" + str(value))
if __name__ == "__main__":
	argv = sys.argv[1:]
	if len(argv) < 1:
		print("usage ", sys.argv[0], " file dir")
		sys.exit()
	count_miss_or_meet_deadline_requests(argv[0])
