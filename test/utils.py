import subprocess


def log_info(message):
    print(f"[INFO] {message}")


def log_error(message):
    print(f"[ERROR] {message}")


def execute_program(program_name, args_list):
    try:
        log_info(f"Executing {program_name} {' '.join(args_list)}")
        result = subprocess.run([program_name] + args_list, capture_output=True, text=True)
        log_info(f"Program exited with return code: {result.returncode}")
        if result.stdout:
            log_info(result.stdout)
        if result.stderr:
            log_error(result.stderr)
        return result.returncode
    except Exception as e:
        log_error(f"An error occurred while executing the program: {e}")
        return -1
