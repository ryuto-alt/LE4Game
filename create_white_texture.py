from PIL import Image

# Create a 1x1 white pixel image
img = Image.new('RGBA', (1, 1), color=(255, 255, 255, 255))
img.save('Resources/Images/white1x1.png')
print("Created white1x1.png")

# Also create colored versions for UI elements
colors = {
    'green1x1.png': (0, 255, 0, 255),
    'red1x1.png': (255, 0, 0, 255),
    'yellow1x1.png': (255, 255, 0, 255),
    'gray1x1.png': (128, 128, 128, 255),
    'black1x1.png': (0, 0, 0, 255),
}

for filename, color in colors.items():
    img = Image.new('RGBA', (1, 1), color=color)
    img.save(f'Resources/Images/{filename}')
    print(f"Created {filename}")

print("All texture files created successfully!")