import os
from utils import execute_program, log_info

script_root = os.path.dirname(os.path.abspath(__file__))
exe_path = os.path.join(script_root, "..", "bin", "win/Release" if os.name == 'nt' else "linux", "GIFMirage")


def process_mirage():
    return_code = execute_program(exe_path, [
        os.path.join(script_root, "..", "images", "气气.gif"),
        os.path.join(script_root, "..", "images", "马达.gif"),
        "-o", os.path.join(script_root, "mirage", "gif-gif"),
        "-p", "0"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(exe_path, [
        os.path.join(script_root, "..", "images", "3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp"),
        os.path.join(script_root, "..", "images", "气气.gif"),
        "-o", os.path.join(script_root, "mirage", "webp-gif"),
        "-x", "500",
        "-y", "300"
    ])
    if return_code != 0:
        exit(return_code)

    return_code = execute_program(exe_path, [
        os.path.join(script_root, "..", "images", "马达.gif"),
        os.path.join(script_root, "..", "images", "7808ce7d382f950d32732a52c8dc972d3d27a9a8.png"),
        "-o", os.path.join(script_root, "mirage", "gif-png"),
        "-f", "20",
        "-d", "100"
    ])
    if return_code != 0:
        exit(return_code)

    for s in range(5):
        for w in range(1, 5):
            for suffix in ["C", "R"]:
                mode = f"S{s}W{w}{suffix}"
                return_code = execute_program(exe_path, [
                    os.path.join(script_root, "..", "images", "气气.gif"),
                    os.path.join(script_root, "..", "images", "马达.gif"),
                    "-o", os.path.join(script_root, "mirage", mode),
                    "-m", mode,
                    "-p", "12"
                ])
                if return_code != 0:
                    exit(return_code)


if __name__ == "__main__":
    process_mirage()
    log_info("All tests passed.")
