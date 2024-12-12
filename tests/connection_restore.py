import subprocess
import time
import sys
import os
import threading

CONNECT_TIMEOUT_S = 15
DISCONNECT_MESSAGE = "Closing session because it has expired"
CONNECT_MESSAGE = "Z_OPEN(Ack)"
ZENOH_PORT = "7447"

DIR_EXAMPLES = "build/examples"
TEST_COMMANDS = [
    # Active client
    ["stdbuf", "-o0", f'{DIR_EXAMPLES}/z_pub', '-e', f'tcp/127.0.0.1:{ZENOH_PORT}'],
    # Passive client
    ["stdbuf", "-o0", f'{DIR_EXAMPLES}/z_sub', '-e', f'tcp/127.0.0.1:{ZENOH_PORT}'],
]


def run_process(command, output_collector, process_list):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    process_list.append(process)
    for line in iter(process.stdout.readline, ''):
        print("--", line.strip())
        output_collector.append(line.strip())
    process.stdout.close()
    process.wait()


def terminate_processes(process_list):
    for process in process_list:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()


def block_connection():
    subprocess.run(["iptables", "-A", "INPUT", "-p", "tcp", "--dport", ZENOH_PORT, "-j", "DROP"], check=True)
    subprocess.run(["iptables", "-A", "OUTPUT", "-p", "tcp", "--sport", ZENOH_PORT, "-j", "DROP"], check=True)


def unblock_connection():
    subprocess.run(["iptables", "-D", "INPUT", "-p", "tcp", "--dport", ZENOH_PORT, "-j", "DROP"], check=False)
    subprocess.run(["iptables", "-D", "OUTPUT", "-p", "tcp", "--sport", ZENOH_PORT, "-j", "DROP"], check=False)


def wait_message(client_output, message):
    start_time = time.time()
    while time.time() - start_time < CONNECT_TIMEOUT_S:
        if any(CONNECT_MESSAGE in line for line in client_output):
            return True
        time.sleep(1)
    else:
        return False


def test_connection(client_command, timeout, wait_disconnect):
    print(f"Test {client_command} for timeout {timeout}")
    client_output = []
    process_list = []
    blocked = False
    try:
        client_thread = threading.Thread(target=run_process, args=(client_command, client_output, process_list))
        client_thread.start()

        if wait_message(client_output, CONNECT_MESSAGE):
            print("Initial connection successful.")
        else:
            print("Connection failed.")
            return False

        client_output.clear()
        print("Blocking connection...")
        block_connection()
        blocked = True
        time.sleep(timeout)

        if wait_disconnect:
            if wait_message(client_output, DISCONNECT_MESSAGE):
                print("Connection lost successfully.")
            else:
                print("Failed to block connection.")
                return False

        client_output.clear()
        print("Unblocking connection...")
        unblock_connection()
        blocked = False
        time.sleep(timeout)

        if wait_message(client_output, CONNECT_MESSAGE):
            print("Connection restored successfully.")
        else:
            print("Failed to restore connection.")
            return False

        print(f"Test for timeout {timeout} passed.")
        return True
    finally:
        if blocked:
            unblock_connection()
        terminate_processes(process_list)


if __name__ == "__main__":
    for cmd in TEST_COMMANDS:
        # timeout more than sesson timeout
        if not test_connection(cmd, 8, False):
            sys.exit(1)
        # timeout less than sesson timeout
        if not test_connection(cmd, 15, False):
            sys.exit(1)
