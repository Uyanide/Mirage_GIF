import sys
from PIL import Image, ImageChops
import numpy as np


def compare_images(img1_path, img2_path):
    img1 = Image.open(img1_path).convert('RGB')
    img2 = Image.open(img2_path).convert('RGB')

    if img1.size != img2.size:
        print("size mismatch")
        return

    diff = ImageChops.difference(img1, img2)
    diff_np = np.array(diff)
    mse = np.mean(diff_np ** 2)
    psnr = 10 * np.log10((255 ** 2) / mse) if mse != 0 else float('inf')

    print(f"MSE: {mse:.2f}")
    print(f"PSNR: {psnr:.2f} dB")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(1)
    else:
        compare_images(sys.argv[1], sys.argv[2])
