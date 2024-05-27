import torch
from PIL import Image
from ultralytics import YOLO
import paho.mqtt.client as mqtt
from PIL import Image
import numpy as np
import os

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Failed to connect: {reason_code}. loop_forever() will retry connection")
    else:
        client.subscribe("cpp_client")
        print("Connected to MQTT")

def on_message(client, userdata, message):
    
    userdata.append(message.payload)
    
    nparr = np.frombuffer(message.payload, np.uint8)
    
    img = Image.frombuffer("RGBA", (1224,1840), nparr, "raw")
    
    top_half = img.crop((0, 0, 1224, 920))
    bottom_half = img.crop((0, 920, 1224, 1840))
    

    results_top = model.predict(top_half, conf=0.6, device="0", imgsz=320, classes=0)[0].plot()
    results_bottom = model.predict(bottom_half, conf=0.6, device="0", imgsz=320, classes=0)[0].plot()

    # Convert the numpy arrays back to images
    top_half_rgb = Image.fromarray(results_top[:, :, ::-1])
    bottom_half_rgb = Image.fromarray(results_bottom[:, :, ::-1])
    
    # Combine the top and bottom halves
    combined = Image.new('RGBA', (top_half.width, top_half.height + bottom_half.height))
    combined.paste(top_half_rgb, (0, 0))
    combined.paste(bottom_half_rgb, (0, top_half.height))
    
    mqttc.publish("python_client", combined.tobytes())

if __name__ == "__main__": 
    os.chdir('./yolov8things')
    model = YOLO("./yolov8n-seg.pt") # CHANGE TO THE PATH OF THE WANTED MODEL CHECKPOINT
    DEVICE = 'cuda:0' if torch.cuda.is_available() else 'cpu'
    
    
    mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message
    
    mqttc.user_data_set([])
    mqttc.connect("localhost", 1883)
    mqttc.loop_forever()