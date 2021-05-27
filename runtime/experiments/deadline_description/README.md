# Deadline Description

How to use with deadline_description and workload_mix_realworld

1. Start the server with deadline description
2. From the client, run the script to generate the spec.json
3. Manually copy spec.json to the server in the workload_mix_realword directory, i.e.

```sh
scp spec.json sean@192.168.7.26:~/projects/sledge-serverless-framework/runtime/experiments/workload_mix_realworld/spec.json
```

4. Start workload_mix_realworld on the server with the desired scheduling policy
5. Start workload_mix_realworld from the client targeting the server
6. Repeat for steps 4,5 for each desired scheduling policy.
