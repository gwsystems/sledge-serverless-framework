# Deadline Description

How to use with deadline_description and workload_mix_realworld

1. SSH into the server and run the deadline description experiment. The code snippet below is naming the experiment benchmark, so it is first deleting any previous experiments that would cause a name collision.

```sh
ssh sean@192.168.7.26
cd ~/projects/sledge-serverless-framework/runtime/experiments/deadline_description/
rm -rf ./res/benchmark
./run.sh -n=benchmark
```

2. This generates a spec.json in the results directory at `./res/benchmark/spec.json`. Copy this to the `workload_mix_realword directory` on the server.

```sh
cp ./res/benchmark/fifo_nopreemption/spec.json ../workload_mix_realworld/
cd ../workload_mix_realworld/
```

3. And then on your client, go to the workload_mix_realworld directory and use scp to copy the `spec.json` file from the server. The client needs this file to understand which ports the various modules are going to be running on.

```sh
cd ~/projects/sledge-serverless-framework/runtime/experiments/workload_mix_realworld/
scp sean@192.168.7.26:~/projects/sledge-serverless-framework/runtime/experiments/workload_mix_realworld/spec.json spec.json
```

4. If the deadline interval was modified, you may need to manually modify the `mix.csv` to properly map to the module names in `spec.json`. Once complete, the experiment is ready to run

5. On the server, start the runtime using one of the configurations expressed as .env files.

```sh
./run.sh -s -e=fifo_nopreemption.env --name=myrun
```

6. On the client, run the client driver script targeting the server.

```sh
./run.sh -t=192.168.7.26
```

The client does not have the concept of an "env file" given that that is a server config, but if you want to namespace output from the client, you can define a directory as the name. This would write the client results to `./res/myrun/client/fifo_nopreemption/`.

```sh
./run.sh -t=192.168.7.26 --name=myrun/client/fifo_nopreemption/
```

7. Repeat for steps 5,6 for each desired scheduling policy.

8. The results are on the server. You may want to copy them to your client to more easily inspect.

```sh
scp -r sean@192.168.7.26:~/projects/sledge-serverless-framework/runtime/experiments/workload_mix_realworld/res/myrun/* ./res/myrun
```

9. If you are assembling a spreadsheet, you might want to also copy the results from the `deadline_description` experiment. The `deadlines.csv` file and the execution_time files are useful for understanding the performance characteristics of a single workload.
