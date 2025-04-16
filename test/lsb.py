import os
from utils import execute_program, log_info, log_error
import hashlib


def compare_file(file1, file2):
    def get_file_hash(filepath):
        with open(filepath, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()

    hash1 = get_file_hash(file1)
    hash2 = get_file_hash(file2)
    return hash1 == hash2


script_root = os.path.dirname(os.path.abspath(__file__))
enc_path = os.path.join(script_root, "..", "bin", "win/Release" if os.name == 'nt' else "linux", "GIFLsb-enc")
dec_path = os.path.join(script_root, "..", "bin", "win/Release" if os.name == 'nt' else "linux", "GIFLsb-dec")


def process_lsb_gif():
    return_code = execute_program(enc_path, [
        os.path.join(script_root, "..", "images", "气气.gif"),
        os.path.join(script_root, "..", "images", "马达.gif"),
        "-o", os.path.join(script_root, "lsb", "gif"),
        "-p", "0",
        "-m", "whatever"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(dec_path, [
        os.path.join(script_root, "lsb", "gif.gif"),
        "-o", "gif-dec.gif",
        "-d", os.path.join(script_root, "lsb")
    ])
    if return_code != 0:
        exit(return_code)

    is_same = compare_file(
        os.path.join(script_root, "..", "images", "马达.gif"),
        os.path.join(script_root, "lsb", "gif-dec.gif")
    )
    if not is_same:
        log_error("Failed.")
        exit(1)


def process_lsb_webp():
    return_code = execute_program(enc_path, [
        os.path.join(script_root, "..", "images", "3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp"),
        os.path.join(script_root, "..", "images", "马达.gif"),
        "-o", os.path.join(script_root, "lsb", "webp"),
        "-p", "12",
        "-c", "2",
        "-g",
        "-m", "none"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(dec_path, [
        os.path.join(script_root, "lsb", "webp.gif"),
        "-o", "webp-dec",
        "-d", os.path.join(script_root, "lsb")
    ])
    if return_code != 0:
        exit(return_code)

    is_same = compare_file(
        os.path.join(script_root, "..", "images", "马达.gif"),
        os.path.join(script_root, "lsb", "webp-dec.gif")
    )
    if not is_same:
        log_error("Failed.")
        exit(1)


def process_lsb_png():
    return_code = execute_program(enc_path, [
        os.path.join(script_root, "..", "images", "7808ce7d382f950d32732a52c8dc972d3d27a9a8.png"),
        os.path.join(script_root, "..", "images", "slime.jpg"),
        "-o", os.path.join(script_root, "lsb", "png"),
        "-l",
        "-c", "31",
        "-d"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(dec_path, [
        os.path.join(script_root, "lsb", "png.gif"),
        "-o", "png-dec",
        "-d", os.path.join(script_root, "lsb")
    ])
    if return_code != 0:
        exit(return_code)

    is_same = compare_file(
        os.path.join(script_root, "..", "images", "slime.jpg"),
        os.path.join(script_root, "lsb", "png-dec.jpg")
    )
    if not is_same:
        log_error("Failed.")
        exit(1)


def process_single_frame():
    return_code = execute_program(enc_path, [
        os.path.join(script_root, "..", "images", "气气.gif"),
        os.path.join(script_root, "..", "images", "马达.gif"),
        "-o", os.path.join(script_root, "lsb", "single"),
        "-s"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(dec_path, [
        os.path.join(script_root, "lsb", "single.gif"),
        "-o", "single-dec",
        "-d", os.path.join(script_root, "lsb")
    ])
    if return_code != 0:
        exit(return_code)

    is_same = compare_file(
        os.path.join(script_root, "..", "images", "马达.gif"),
        os.path.join(script_root, "lsb", "single-dec.gif")
    )
    if not is_same:
        log_error("Failed.")
        exit(1)


if __name__ == "__main__":
    process_lsb_gif()
    process_lsb_webp()
    process_lsb_png()
    process_single_frame()
    log_info("All tests passed.")
