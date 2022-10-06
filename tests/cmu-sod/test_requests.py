import time
import requests

url = 'http://arena0.andrew.cmu.edu:10000/depth_to_xyz'

payload = open('0_depth.png', 'rb')

img = None

response = requests.post(url, data=payload)
img = response.content
time.sleep(1)
print("single request works!")

for i in range(100):
    payload = open('0_depth.png', 'rb')
    response = requests.post(url, data=payload)
    img = response.content
    time.sleep(1)
    print(f"multi request #{i} works!")
