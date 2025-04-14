import subprocess
import os


def log_info(message):
    print(f"[INFO] {message}")


def log_error(message):
    print(f"[ERROR] {message}")


def is_windows():
    return os.name == 'nt'


def execute_program(program_name, args_list):
    try:
        if is_windows():
            encoded_args_list = [arg.encode('mbcs').decode('mbcs') for arg in args_list]
            encoding = 'mbcs'
        else:
            encoded_args_list = args_list
            encoding = 'utf-8'

        log_info(f"Executing {program_name} {' '.join(encoded_args_list)}")
        result = subprocess.run(
            [program_name] + encoded_args_list,
            capture_output=True,
            text=True,
            encoding=encoding
        )
        log_info(f"Program exited with return code: {result.returncode}")
        if result.stdout:
            log_info(result.stdout)
        if result.stderr:
            log_error(result.stderr)
        return result.returncode
    except Exception as e:
        log_error(f"An error occurred while executing the program: {e}")
        return -1
