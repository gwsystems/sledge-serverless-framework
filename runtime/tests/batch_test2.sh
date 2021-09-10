#!/bin/bash
# Executes the runtime in GDB
# Substitutes the absolute path from the container with a path relatively derived from the location of this script
# This allows debugging outside of the Docker container
# Also disables pagination and stopping on SIGUSR1

# We are currently unclear why the image classifier isn't working properly
# Both bmp and png formats are added to the repo while debugging
# file_type=bmp
# file_type=png
file_type=jpg
batch_id=0
for image in airplane_0688.jpg car_0931.jpg car_0936.jpg cat_0865.jpg dog_0323.jpg dog_0658.jpg flower_0790.jpg motorbike_0334.jpg motorbike_0684.jpg motorbike_0706.jpg; do
	for instance in 1 2 3; do
		echo "Classifying $image"
		#curl -H 'Expect:' -H "Content-Type: Image/$file_type" --data-binary "@images/$file_type/$class$instance.$file_type" localhost:10000 2> /dev/null
                hey -disable-compression -disable-keepalive -disable-redirects -c 1 -z 120s -cpus 1 -t 0 -m GET -D "sample_images/$image" "http://10.10.1.1:10000" > "client_$batch_id.txt" &
		((batch_id++))
	done
done
pids=$(pgrep hey | tr '\n' ' ')
[[ -n $pids ]] && wait -f $pids
printf "[OK]\n"


