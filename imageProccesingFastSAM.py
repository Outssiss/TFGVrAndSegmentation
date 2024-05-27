from FastSAM.fastsam import FastSAMPrompt
import torch
from PIL import Image
from ultralytics import YOLO
import paho.mqtt.client as mqtt
from PIL import Image
import numpy as np
import os

os.chdir('./fastsam')

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
    
    everything_results_top = model(top_half, device=DEVICE, retina_masks=True, imgsz=256, conf=0.6, iou=0.9, verbose=False)
    everything_results_bottom = model(bottom_half, device=DEVICE, retina_masks=True, imgsz=256, conf=0.6, iou=0.9, verbose=False)
    prompt_process_top = FastSAMPrompt(top_half, everything_results_top, device=DEVICE)
    prompt_process_bottom = FastSAMPrompt(bottom_half, everything_results_bottom, device=DEVICE)
    
    if(prompt_process_top.results[0].masks is not None and prompt_process_bottom.results[0].masks is not None):
        
        point = [(int)(1224/2), (int)(920/2)]
        
        masks_top = prompt_process_top.point_prompt(points=[point], pointlabel=[1])
        masks_bottom = prompt_process_bottom.point_prompt(points=[point], pointlabel=[1])
        
       
        """
        ann_top = prompt_process_top.everything_prompt()
        ann_bottom = prompt_process_bottom.everything_prompt()
        ann_top = prompt_process_top.text_prompt(text='person')
        ann_bottom = prompt_process_bottom.text_prompt(text='person')
        """

        masks_top_np = np.array(masks_top)
        masks_bottom_np = np.array(masks_bottom)
    
        # Remove the extra dimension from the masks
        masks_top_np = np.squeeze(masks_top_np)
        masks_bottom_np = np.squeeze(masks_bottom_np)
        
        # Convert the images to numpy arrays
        image_top_half_np = np.array(top_half)
        image_bottom_half_np = np.array(bottom_half)

        # Set the green channel to 255 where the mask is True
        image_top_half_np[masks_top_np] = [0, 255, 0, 255]
        image_bottom_half_np[masks_bottom_np] = [0, 255, 0, 255]
        
        image_top_half_np[point[1], point[0]] = [255, 0, 0, 255]
        image_bottom_half_np[point[1], point[0]] = [255, 0, 0, 255]

        # Convert the numpy arrays back to images
        top_half_rgb = Image.fromarray(image_top_half_np)
        bottom_half_rgb = Image.fromarray(image_bottom_half_np)
        
        # Combine the top and bottom halves
        combined = Image.new('RGBA', (top_half.width, top_half.height + bottom_half.height))
        combined.paste(top_half_rgb, (0, 0))
        combined.paste(bottom_half_rgb, (0, top_half.height))
        
        mqttc.publish("python_client", combined.tobytes())
    else:
        mqttc.publish("python_client", message.payload)

if __name__ == "__main__":
    models = {k: YOLO(f"{k}.pt") for k in ["FastSAM-s", "FastSAM-x"]}
    model = models["FastSAM-x"] # CHANGE TO THE PATH OF THE WANTED MODEL CHECKPOINT
    DEVICE = 'cuda:0' if torch.cuda.is_available() else 'cpu'
    
    mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message
    
    mqttc.user_data_set([])
    mqttc.connect("localhost", 1883)
    mqttc.loop_forever()