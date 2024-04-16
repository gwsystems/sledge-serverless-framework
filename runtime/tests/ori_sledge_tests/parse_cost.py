import sys

if len(sys.argv) != 2:
    print("Usage: python script.py <filename>")
    sys.exit(1)

filename = sys.argv[1]

try:
    with open(filename, "r") as file:
        lines = file.readlines()
except FileNotFoundError:
    print("File not found:", filename)
    sys.exit(1)

total_cold_time = 0
total_warm_time = 0
count = 0

for i in range(0, len(lines), 2):
    time_str_1 = lines[i].split(": ")[1].strip()
    time_str_2 = lines[i+1].split(": ")[1].strip()
    time_value_1 = float(time_str_1[:-1])          
    time_value_2 = float(time_str_2[:-1])    

    total_cold_time += time_value_1
    total_warm_time += time_value_2            
    count += 1                             

average_cold_time = total_cold_time / count
average_warm_time = total_warm_time / count

print("Average cold time:", average_cold_time, "s")
print("Average warm time:", average_warm_time, "s")

