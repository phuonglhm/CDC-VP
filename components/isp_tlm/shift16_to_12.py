import numpy as np
import argparse


def convert_16_to_12(input_path, output_path):
    # Read the raw binary file as little-endian 16-bit unsigned integers
    # ('<u2' ensures we explicitly read it as little-endian, which is standard)
    print(f"Reading {input_path}...")
    data_16bit = np.fromfile(input_path, dtype="<u2")

    # Bitwise shift right by 4:
    # 16-bit range (0 - 65535) -> 12-bit range (0 - 4095)
    data_12bit = np.right_shift(data_16bit, 4)

    # Save the modified data back to a new binary file
    print(f"Saving 12-bit RAW to {output_path}...")
    data_12bit.tofile(output_path)
    print("Done!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert 16-bit RAW to 12-bit RAW")
    parser.add_argument("-i", "--input", required=True, help="Input 16-bit .raw file")
    parser.add_argument("-o", "--output", required=True, help="Output 12-bit .raw file")

    args = parser.parse_args()
    convert_16_to_12(args.input, args.output)
