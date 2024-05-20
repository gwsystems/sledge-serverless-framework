import json
import argparse

def generate_json(route_replicas):
    data = [
        {
            "name": "gwu",
            "port": 31850,
            "replenishment-period-us": 0,
            "max-budget-us": 0,
            "route-replicas": route_replicas,
            "routes": [
                {
                    "route": "/fib",
                    "request-type": 1,
                    "n-resas": 1,
                    "path": "fibonacci.wasm.so",
                    "admissions-percentile": 70,
                    "expected-execution-us": 5,
                    "relative-deadline-us": 50,
                    "http-resp-content-type": "text/plain"
                }
            ]
        }
    ]
    return data

def main():
    parser = argparse.ArgumentParser(description='Generate JSON file.')
    parser.add_argument('route_replicas', type=int, help='Value for route-replicas')
    parser.add_argument('output_file', type=str, help='Output JSON file')
    
    args = parser.parse_args()
    
    json_data = generate_json(args.route_replicas)
    
    with open(args.output_file, 'w') as f:
        json.dump(json_data, f, indent=4)

if __name__ == "__main__":
    main()

