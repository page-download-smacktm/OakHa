#!/usr/bin/env python3
import struct
import sys
import zlib


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("arquivo nao e PNG: " + path)
    position = 8
    chunks = []
    while position < len(data):
        size = struct.unpack(">I", data[position:position + 4])[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        position += 12 + size
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload)
            if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValueError("PNG precisa ser RGBA8 sem entrelacamento: " + path)
        elif kind == b"IDAT":
            chunks.append(payload)
        elif kind == b"IEND":
            break
    decoded = zlib.decompress(b"".join(chunks))
    stride = width * 4
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_type = decoded[offset]
        current = bytearray(decoded[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for index in range(stride):
            left = current[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 255
            elif filter_type == 2:
                current[index] = (current[index] + above) & 255
            elif filter_type == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 255
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = abs(estimate - left), abs(estimate - above), abs(estimate - upper_left)
                predictor = left if distances[0] <= distances[1] and distances[0] <= distances[2] else above if distances[1] <= distances[2] else upper_left
                current[index] = (current[index] + predictor) & 255
            elif filter_type != 0:
                raise ValueError("filtro PNG desconhecido")
        rows.append([tuple(current[index:index + 4]) for index in range(0, stride, 4)])
        previous = current
    return width, height, rows


def resize(width, height, rows, target_width, target_height):
    result = []
    for y in range(target_height):
        source_y = y * height // target_height
        result.append([])
        for x in range(target_width):
            source_x = x * width // target_width
            result[-1].append(rows[source_y][source_x])
    return result


def rgb888(pixel):
    red, green, blue, alpha = pixel
    return (red, green, blue), alpha


def write_array(output, name, rows):
    pixels = [rgb888(pixel) for row in rows for pixel in row]
    output.write("const unsigned char %s_pixels[] = {\n" % name)
    flattened = [channel for pixel, _ in pixels for channel in pixel]
    for index in range(0, len(flattened), 18):
        output.write("    " + ", ".join(str(value) for value in flattened[index:index + 18]) + ",\n")
    output.write("};\n")
    output.write("const unsigned char %s_alpha[] = {\n" % name)
    for index in range(0, len(pixels), 24):
        output.write("    " + ", ".join(str(alpha) for _, alpha in pixels[index:index + 24]) + ",\n")
    output.write("};\n\n")


def main():
    if len(sys.argv) != 4:
        raise SystemExit("uso: generate_assets.py fundo.png shell.png saida.c")
    background = resize(*read_png(sys.argv[1]), 652, 432)
    shell = resize(*read_png(sys.argv[2]), 56, 42)
    with open(sys.argv[3], "w") as output:
        output.write('#include "acorn/assets.h"\n\n')
        write_array(output, "oakos_background", background)
        write_array(output, "oakos_shell_icon", shell)


if __name__ == "__main__":
    main()