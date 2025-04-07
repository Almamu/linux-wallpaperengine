import os
import re
import base64
import cv2
import numpy as np
import shutil
from bs4 import BeautifulSoup

def extract_base64_image(html_file):
    """Extracts base64-encoded image data from an HTML file."""
    with open(html_file, "r", encoding="utf-8") as f:
        soup = BeautifulSoup(f, "html.parser")
        img_tag = soup.find("img")
        if img_tag and 'src' in img_tag.attrs:
            match = re.search(r'data:image/png;base64,(.+)', img_tag['src'])
            if match:
                return match.group(1)
    return None

def categorize_image(image_data):
    """Categorizes the image as 'grey', 'no image', or 'content'."""
    if not image_data:
        return "no_image"
    
    img_array = np.frombuffer(base64.b64decode(image_data), dtype=np.uint8)
    img = cv2.imdecode(img_array, cv2.IMREAD_GRAYSCALE)
    
    if img is None:
        return "no_image"
    
    mean_value = cv2.mean(img)[0]  # Average pixel brightness
    std_dev = np.std(img)  # Standard deviation of pixel values
    
    if mean_value > 200 and std_dev < 10:
        return "grey"
    return "content"

def process_html_files(input_folder, output_html="gallery.html"):
    """Processes all HTML files, categorizes them, and generates a gallery."""
    categories = {"no_image": [], "grey": [], "content": []}
    
    for file in os.listdir(input_folder):
        if file.endswith(".html") and not file.startswith('report'):
            file_path = os.path.join(input_folder, file)
            image_data = extract_base64_image(file_path)
            category = categorize_image(image_data)
            categories[category].append((file, image_data))
    
    with open(output_html, "w", encoding="utf-8") as f:
        f.write("""
        <html>
        <head>
            <title>Image Gallery</title>
            <style>
                .gallery { display: flex; flex-wrap: wrap; }
                .thumb { margin: 10px; text-align: center; }
                img { width: 100px; height: auto; border: 1px solid black; }
            </style>
        </head>
        <body>
        <h1>Image Gallery</h1>
        """)
        
        for category, files in categories.items():
            f.write(f"<h2>{category.replace('_', ' ').title()}</h2>")
            f.write("<div class='gallery'>")
            for file_name, img_data in files:
                if img_data:
                    f.write(f"<div class='thumb'><a href='{file_name}'><img src='data:image/png;base64,{img_data}'></a></div>")
                else:
                    f.write(f"<div class='thumb'><a href='{file_name}'>[No Image]</a></div>")
            f.write("</div>")
        
        f.write("</body></html>")
    
    print(f"Gallery created at {output_html}")

if __name__ == "__main__":
    input_folder = "./output"  # Change to your directory
    process_html_files(input_folder, "output/gallery.html")
