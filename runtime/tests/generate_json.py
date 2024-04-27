import sys
import json

def generate_config(num_routes):
    config = []
    for i in range(1, num_routes + 1):
        route = {
            "route": f"/fib{i}",
            "request-type": i,
            "n-resas": 2,
            "path": f"fibonacci{i}.wasm.so",
            #"path": "fibonacci.wasm.so",
            "admissions-percentile": 70,
            "expected-execution-us": 1,
            "relative-deadline-us": 10,
            "http-resp-content-type": "text/plain"
        }
        config.append(route)
    return config

def main():
    if len(sys.argv) != 2:
        print("Usage: python script.py <num_routes>")
        return

    num_routes = int(sys.argv[1])
    routes = generate_config(num_routes)

    config = [{
        "name": "gwu",
        "port": 31850,
        "replenishment-period-us": 0,
        "max-budget-us": 0,
        "routes": routes
    }]

    with open('config.json', 'w') as f:
        json.dump(config, f, indent=4)

if __name__ == "__main__":
    main()

