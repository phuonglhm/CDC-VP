#!/usr/bin/env python3

import sys
import os
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")

try:
    import numpy as np
    import matplotlib.pyplot as plt
except ModuleNotFoundError as exc:
    print(
        f"Missing Python dependency: {exc.name}\n"
        "Install the visualization dependencies, for example:\n"
        "  sudo apt install python3-numpy python3-matplotlib\n"
        "or, if pip is available for python3:\n"
        "  python3 -m pip install numpy matplotlib",
        file=sys.stderr,
    )
    sys.exit(1)


def read_metadata(yuv_path):
    """Read metadata JSON sidecar file next to the YUV file."""
    json_path = yuv_path + ".json"
    if os.path.exists(json_path):
        try:
            with open(json_path, 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"Warning: Could not read metadata from {json_path}: {e}")
    return None


def ensure_output_dir():
    """Create and return the canonical generated-output folder."""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    return OUTPUT_DIR


def read_raw_image(filepath, width, height):
    """Read 12-bit RAW image from file."""
    try:
        raw_data = np.fromfile(filepath, dtype=np.uint16)
        return raw_data.reshape((height, width))
    except Exception as e:
        print(f"Error reading RAW image: {e}")
        return None


def demosaic_basic(bayer, pattern='RGGB'):
    """Basic nearest-neighbor demosaicing for visualization purposes only.
    Pattern names match the CFA layout convention:
      RGGB: R at (even,even), B at (odd,odd)
      GRBG: G at (even,even), R at (even,odd)
      BGGR: B at (even,even), G at (odd,odd) on diagonal
      GBRG: G at (even,even), B at (even,odd)
    """
    h, w = bayer.shape
    # Build full RGB at original resolution using simple nearest-neighbor
    rgb = np.zeros((h, w, 3), dtype=np.float32)
    if pattern == 'RGGB':
        rgb[0::2, 0::2, 0] = bayer[0::2, 0::2]                     # R
        rgb[0::2, 1::2, 1] = bayer[0::2, 1::2]                     # Gr
        rgb[1::2, 0::2, 1] = bayer[1::2, 0::2]                     # Gb
        rgb[1::2, 1::2, 2] = bayer[1::2, 1::2]                     # B
    elif pattern == 'GRBG':
        rgb[0::2, 1::2, 0] = bayer[0::2, 1::2]                     # R
        rgb[0::2, 0::2, 1] = bayer[0::2, 0::2]                     # Gr
        rgb[1::2, 1::2, 1] = bayer[1::2, 1::2]                     # Gb
        rgb[1::2, 0::2, 2] = bayer[1::2, 0::2]                     # B
    elif pattern == 'BGGR':
        rgb[0::2, 0::2, 2] = bayer[0::2, 0::2]                     # B
        rgb[0::2, 1::2, 1] = bayer[0::2, 1::2]                     # Gb
        rgb[1::2, 0::2, 1] = bayer[1::2, 0::2]                     # Gr
        rgb[1::2, 1::2, 0] = bayer[1::2, 1::2]                     # R
    elif pattern == 'GBRG':
        rgb[0::2, 1::2, 2] = bayer[0::2, 1::2]                     # B
        rgb[0::2, 0::2, 1] = bayer[0::2, 0::2]                     # Gb
        rgb[1::2, 1::2, 1] = bayer[1::2, 1::2]                     # Gr
        rgb[1::2, 0::2, 0] = bayer[1::2, 0::2]                     # R
    # Fill missing channels with nearest-neighbor interpolation
    for c in range(3):
        # Simple fill: replicate neighbors
        channel = rgb[..., c]
        # Forward fill horizontally
        for x in range(1, w):
            channel[:, x] = np.where(channel[:, x] == 0, channel[:, x-1], channel[:, x])
        for x in range(w-2, -1, -1):
            channel[:, x] = np.where(channel[:, x] == 0, channel[:, x+1], channel[:, x])
        for y in range(1, h):
            channel[y] = np.where(channel[y] == 0, channel[y-1], channel[y])
        for y in range(h-2, -1, -1):
            channel[y] = np.where(channel[y] == 0, channel[y+1], channel[y])
        rgb[..., c] = channel
    return rgb


def read_yuv420(filepath, width, height):
    """Read YUV420 file and convert to RGB."""
    try:
        data = np.fromfile(filepath, dtype=np.uint8)

        y_size = width * height
        uv_size = (width // 2) * (height // 2)

        Y = data[0:y_size].reshape((height, width))
        U = data[y_size:y_size+uv_size].reshape((height//2, width//2))
        V = data[y_size+uv_size:].reshape((height//2, width//2))

        # Upsample U and V
        U_up = np.repeat(np.repeat(U, 2, axis=0), 2, axis=1)
        V_up = np.repeat(np.repeat(V, 2, axis=0), 2, axis=1)

        # Convert to RGB (BT.709)
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


def find_output_yuv():
    """Find the canonical output.yuv file.

    Prefer components/isp_tlm/output/, but keep legacy locations readable so
    existing generated files can still be visualized.
    """
    candidates = [
        os.path.join(OUTPUT_DIR, "output.yuv"),
        os.path.join("output", "output.yuv"),
        "components/isp_tlm/output/output.yuv",
        "output.yuv",
        os.path.join(SCRIPT_DIR, "output.yuv"),
        "components/isp_tlm/output.yuv",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return os.path.join(OUTPUT_DIR, "output.yuv")  # default if not found


def main():
    # Default values
    width = 2592
    height = 1536
    input_bit_depth = 12  # default if no metadata
    bayer_pattern_name = 'RGGB'

    # YUV output path - check current dir first, then canonical location
    yuv_path = find_output_yuv()

    # Try to read metadata from the JSON sidecar
    metadata = read_metadata(yuv_path)
    if metadata:
        width = metadata.get('width', width)
        height = metadata.get('height', height)
        # Accept both 'bit_depth' and new 'input_bit_depth' for backward compat
        input_bit_depth = metadata.get('input_bit_depth', metadata.get('bit_depth', input_bit_depth))
        bayer_pattern_name = metadata.get('bayer_pattern_name', bayer_pattern_name)
        print(f"Read metadata: {width}x{height} {metadata.get('format', 'yuv420p')}")
        print(f"Input bit depth: {input_bit_depth}, Bayer: {bayer_pattern_name}")
        print(f"Source: {metadata.get('source_raw', 'unknown')}")
    else:
        print("Warning: No metadata JSON found. Using default dimensions.")
        print("Run isp_run tool first to generate output.yuv with metadata.")

    # Construct RAW path from metadata if available
    if metadata and 'source_raw' in metadata:
        raw_path = metadata['source_raw']
        # Try multiple candidate locations: workspace root, script dir, cwd
        script_dir = os.path.dirname(os.path.abspath(__file__))
        workspace_root = os.path.dirname(os.path.dirname(script_dir))  # up two levels
        candidates = [
            raw_path,
            os.path.join(workspace_root, raw_path),
            os.path.basename(raw_path),
            os.path.join(script_dir, os.path.basename(raw_path)),
        ]
        # Try also with components/ prefix variants
        if raw_path.startswith("components/isp_tlm/"):
            candidates.append(os.path.join(script_dir, raw_path[len("components/isp_tlm/"):]))
        else:
            candidates.append(os.path.join(workspace_root, "components/isp_tlm", raw_path))
            candidates.append(os.path.join(script_dir, "input", os.path.basename(raw_path)))
            candidates.append(os.path.join(script_dir, os.path.basename(raw_path)))
        for c in candidates:
            if os.path.exists(c):
                raw_path = c
                break
        else:
            raw_path = os.path.basename(raw_path)
    else:
        raw_path = "input/ColorChecker_2592x1536_12bits_RGGB.raw"

    print(f"\nReading RAW image from {raw_path}...")
    raw_img = read_raw_image(raw_path, width, height)

    raw_preview = None
    if raw_img is not None:
        print("Creating basic RAW preview...")
        # Normalize by the input bit depth max value
        max_val = (1 << input_bit_depth) - 1
        raw_preview = demosaic_basic(raw_img, pattern=bayer_pattern_name) / float(max_val)
        raw_preview = np.clip(raw_preview, 0, 1.0)
        raw_preview = np.power(raw_preview, 1/2.2)

    yuv_preview = None
    if os.path.exists(yuv_path):
        print(f"Reading YUV output image from {yuv_path}...")
        yuv_preview = read_yuv420(yuv_path, width, height)
    else:
        print(f"\nERROR: YUV output {yuv_path} not found.")
        print("Run the isp_run tool first to generate the output:")
        print(f"  ./build/bremen/components/isp_tlm/tests/isp_run -i {raw_path} -o {yuv_path} -w {width} --height {height} -b 12 -p 0")
        print()

    # Plotting
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))

    if raw_preview is not None:
        axes[0].imshow(raw_preview)
        axes[0].set_title("Before: Input RAW (Basic Preview)")
        axes[0].axis('off')
    else:
        axes[0].text(0.5, 0.5, 'RAW Image Not Found', ha='center', va='center')
        axes[0].axis('off')

    if yuv_preview is not None:
        axes[1].imshow(yuv_preview)
        axes[1].set_title("After: ISP Pipeline Output (YUV420)")
        axes[1].axis('off')

        # Save standalone JPEG of the final output in the generated-output dir.
        output_jpg = os.path.join(ensure_output_dir(), "output.jpg")
        try:
            plt.imsave(output_jpg, yuv_preview)
            print(f"Saved standalone output image to {output_jpg}")
        except Exception as e:
            print(f"Could not save standalone JPEG: {e}")
    else:
        axes[1].text(0.5, 0.5, 'Output Image Not Found\n(Run isp_run tool)', ha='center', va='center')
        axes[1].axis('off')

    plt.tight_layout()
    output_cmp = os.path.join(ensure_output_dir(), "isp_before_after.jpg")
    plt.savefig(output_cmp, dpi=150)
    print(f"Saved comparison visualization to {output_cmp}")

    # Print YUV stats if available
    if yuv_preview is not None:
        print(f"\nOutput statistics:")
        print(f"  Y (luma): min={int(yuv_preview[:,:,0].min()*255)}, max={int(yuv_preview[:,:,0].max()*255)}")
        print(f"  U (chroma blue): min={int(yuv_preview[:,:,1].min()*255)}, max={int(yuv_preview[:,:,1].max()*255)}")
        print(f"  V (chroma red): min={int(yuv_preview[:,:,2].min()*255)}, max={int(yuv_preview[:,:,2].max()*255)}")


if __name__ == "__main__":
    main()
