import numpy as np
import matplotlib.pyplot as plt
import sys
import os

def read_raw_image(filepath, width, height):
    # 12-bit RAW is typically stored in 16-bit containers (uint16)
    try:
        raw_data = np.fromfile(filepath, dtype=np.uint16)
        return raw_data.reshape((height, width))
    except Exception as e:
        print(f"Error reading RAW image: {e}")
        return None

def demosaic_basic(bayer, pattern='RGGB'):
    # A very basic demosaicing for visualization purposes only
    # Real demosaicing in ISP is much more sophisticated
    h, w = bayer.shape
    rgb = np.zeros((h, w, 3), dtype=np.float32)
    
    if pattern == 'RGGB':
        # R
        rgb[0::2, 0::2, 0] = bayer[0::2, 0::2]
        # B
        rgb[1::2, 1::2, 2] = bayer[1::2, 1::2]
        # G (simplistic average)
        rgb[0::2, 1::2, 1] = bayer[0::2, 1::2]
        rgb[1::2, 0::2, 1] = bayer[1::2, 0::2]
    
    # Simple interpolation to fill gaps for visualization
    for c in range(3):
        mask = (rgb[:, :, c] > 0)
        # We just want a quick preview, so we can just use a simple blur or leave it as is 
        # and scale it up. For simplicity, let's just do a 2x2 block replicate for preview
    
    # Simpler preview: just take 2x2 blocks and form RGB
    preview = np.zeros((h//2, w//2, 3), dtype=np.float32)
    if pattern == 'RGGB':
        preview[:, :, 0] = bayer[0::2, 0::2]
        preview[:, :, 1] = (bayer[0::2, 1::2].astype(np.float32) + bayer[1::2, 0::2].astype(np.float32)) / 2.0
        preview[:, :, 2] = bayer[1::2, 1::2]
        
    return preview

def read_yuv420(filepath, width, height):
    try:
        # YUV420 size: Y is w*h, U is (w/2)*(h/2), V is (w/2)*(h/2)
        # Assuming 8-bit YUV output from the pipeline
        data = np.fromfile(filepath, dtype=np.uint8)
        
        y_size = width * height
        uv_size = (width // 2) * (height // 2)
        
        Y = data[0:y_size].reshape((height, width))
        U = data[y_size:y_size+uv_size].reshape((height//2, width//2))
        V = data[y_size+uv_size:].reshape((height//2, width//2))
        
        # Upsample U and V
        U_up = np.repeat(np.repeat(U, 2, axis=0), 2, axis=1)
        V_up = np.repeat(np.repeat(V, 2, axis=0), 2, axis=1)
        
        # Convert to RGB (BT.709 approximation)
        Y_f = Y.astype(np.float32)
        U_f = U_up.astype(np.float32) - 128.0
        V_f = V_up.astype(np.float32) - 128.0
        
        R = Y_f + 1.5748 * V_f
        G = Y_f - 0.1873 * U_f - 0.4681 * V_f
        B = Y_f + 1.8556 * U_f
        
        RGB = np.stack([R, G, B], axis=-1)
        RGB = np.clip(RGB, 0, 255) / 255.0
        
        return RGB
    except Exception as e:
        print(f"Error reading YUV image: {e}")
        return None

def main():
    width = 2592
    height = 1536
    
    raw_path = "input/ColorChecker_2592x1536_12bits_RGGB.raw"
    yuv_path = "input/output.yuv"
    
    print(f"Reading RAW image from {raw_path}...")
    raw_img = read_raw_image(raw_path, width, height)
    
    raw_preview = None
    if raw_img is not None:
        # Normalize assuming 12-bit max is 4095
        print("Creating basic RAW preview...")
        raw_preview = demosaic_basic(raw_img) / 4095.0
        raw_preview = np.clip(raw_preview, 0, 1.0)
        # Apply simple gamma for visibility
        raw_preview = np.power(raw_preview, 1/2.2)

    yuv_preview = None
    if os.path.exists(yuv_path):
        print(f"Reading YUV output image from {yuv_path}...")
        yuv_preview = read_yuv420(yuv_path, width, height)
    else:
        print(f"YUV output {yuv_path} not found. Run the C++ ISP pipeline first to generate it.")
        
    # Plotting
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    
    if raw_preview is not None:
        axes[0].imshow(raw_preview)
        axes[0].set_title("Before: Input RAW (Basic Preview)")
        axes[0].axis('off')
    else:
        axes[0].text(0.5, 0.5, 'RAW Image Not Found', ha='center')
        axes[0].axis('off')
        
    if yuv_preview is not None:
        axes[1].imshow(yuv_preview)
        axes[1].set_title("After: ISP Pipeline Output (YUV420)")
        axes[1].axis('off')
        
        # Save standalone JPEG of the final output
        try:
            plt.imsave("input/output.jpg", yuv_preview)
            print("Saved standalone output image to input/output.jpg")
        except Exception as e:
            print(f"Could not save standalone JPEG: {e}")
            
    else:
        axes[1].text(0.5, 0.5, 'Output Image Not Found\n(Run ISP Pipeline)', ha='center')
        axes[1].axis('off')
        
    plt.tight_layout()
    plt.savefig("isp_before_after.jpg", dpi=150)
    print("Saved comparison visualization to isp_before_after.jpg")
    
    # Depending on environment, might want to show it
    # plt.show()

if __name__ == "__main__":
    main()
