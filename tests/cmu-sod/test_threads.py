# import numpy as np
import requests
import threading
import time

from flask import Flask, Response

url = 'http://arena0.andrew.cmu.edu:10000/depth_to_xyz'

# app = Flask(__name__)

img = None

def get_img():
    global img
    while True:
        print("start")
        try:
            payload = open('0_depth.png', 'rb')
            response = requests.post(url, data=payload)
            img = response.content
            print("got img")
            time.sleep(0.01)
        except:
            print("failed")
            time.sleep(5)

thread = threading.Thread(target=get_img)
thread.daemon = True
thread.start()
thread.join()
