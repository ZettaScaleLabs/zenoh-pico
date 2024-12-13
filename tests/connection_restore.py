import subprocess
import time
import sys
import threading

ROUTER_INIT_TIMEOUT_S = 3
WAIT_MESSAGE_TIMEOUT_S = 15
DISCONNECT_MESSAGE = "Closing session because it has expired"
CONNECT_MESSAGE = "Z_OPEN(Ack)"
ZENOH_PORT = "7447"

ROUTER_ARGS = ['-l', f'tcp/0.0.0.0:{ZENOH_PORT}', '--no-multicast-scouting']

DIR_EXAMPLES = "build/examples"
ACTIVE_CLIENT_COMMAND = ["stdbuf", "-o0", f'{DIR_EXAMPLES}/z_pub', '-e', f'tcp/127.0.0.1:{ZENOH_PORT}']
PASSIVE_CLIENT_COMMAND = ["stdbuf", "-o0", f'{DIR_EXAMPLES}/z_sub', '-e', f'tcp/127.0.0.1:{ZENOH_PORT}']


def run_process(command, output_collector, process_list):
    print(f"Run {command}")
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    process_list.append(process)
    for line in iter(process.stdout.readline, ''):
        print("--", line.strip())
        if output_collector is not None:
            output_collector.append(line.strip())
    process.stdout.close()
    process.wait()


def run_background(command, output_collector, process_list):
    thread = threading.Thread(target=run_process, args=(command, output_collector, process_list))
    thread.start()


def terminate_processes(process_list):
    for process in process_list:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
    process_list.clear()


def block_connection():
    subprocess.run(["iptables", "-A", "INPUT", "-p", "tcp", "--dport", ZENOH_PORT, "-j", "DROP"], check=True)
    subprocess.run(["iptables", "-A", "OUTPUT", "-p", "tcp", "--sport", ZENOH_PORT, "-j", "DROP"], check=True)


def unblock_connection():
    subprocess.run(["iptables", "-D", "INPUT", "-p", "tcp", "--dport", ZENOH_PORT, "-j", "DROP"], check=False)
    subprocess.run(["iptables", "-D", "OUTPUT", "-p", "tcp", "--sport", ZENOH_PORT, "-j", "DROP"], check=False)


def wait_message(client_output, message):
    start_time = time.time()
    while time.time() - start_time < WAIT_MESSAGE_TIMEOUT_S:
        if any(message in line for line in client_output):
            return True
        time.sleep(1)
    return False


def test_connection(router_command, client_command, timeout, wait_disconnect):
    print(f"Drop test {client_command} for timeout {timeout}")
    client_output = []
    process_list = []
    blocked = False
    try:
        run_background(router_command, None, process_list)
        time.sleep(ROUTER_INIT_TIMEOUT_S)

        run_background(client_command, client_output, process_list)

        # Two iterations because there was an error on the second reconnection
        for _ in range(2):
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

            if wait_message(client_output, CONNECT_MESSAGE):
                print("Connection restored successfully.")
            else:
                print("Failed to restore connection.")
                return False

        print(f"Drop test {client_command} for timeout {timeout} passed")
        return True
    finally:
        if blocked:
            unblock_connection()
        terminate_processes(process_list)


def test_restart(router_command, client_command, timeout):
    print(f"Restart test {client_command} for timeout {timeout}")
    client_output = []
    router_process_list = []
    client_process_list = []
    try:
        run_background(router_command, None, router_process_list)
        time.sleep(ROUTER_INIT_TIMEOUT_S)

        run_background(client_command, client_output, client_process_list)

        if wait_message(client_output, CONNECT_MESSAGE):
            print("Initial connection successful.")
        else:
            print("Connection failed.")
            return False

        client_output.clear()
        print("Stop router...")
        terminate_processes(router_process_list)
        time.sleep(timeout)

        print("Start router...")
        run_background(router_command, None, router_process_list)

        if wait_message(client_output, CONNECT_MESSAGE):
            print("Connection restored successfully.")
        else:
            print("Failed to restore connection.")
            return False

        print(f"Restart test {client_command} for timeout {timeout} passed")
        return True
    finally:
        terminate_processes(client_process_list + router_process_list)


def test_connction_drop(router_command):
    # timeout more than sesson timeout
    # if not test_connection(router_command, ACTIVE_CLIENT_COMMAND, 8, False):
    #    sys.exit(1)
    # timeout less than sesson timeout
    if not test_connection(router_command, ACTIVE_CLIENT_COMMAND, 15, True):
        sys.exit(1)
    # timeout more than sesson timeout
    # if not test_connection(router_command, PASSIVE_CLIENT_COMMAND, 8, False):
    #    sys.exit(1)
    # timeout less than sesson timeout
    if not test_connection(router_command, PASSIVE_CLIENT_COMMAND, 15, True):
        sys.exit(1)


def test_router_restart(router_command):
    if not test_restart(router_command, ACTIVE_CLIENT_COMMAND, 15):
        sys.exit(1)
    if not test_restart(router_command, PASSIVE_CLIENT_COMMAND, 15):
        sys.exit(1)


def main():
    if len(sys.argv) != 2:
        print("Usage: sudo python3 ./connection_restore.py /path/to/zenohd")
        sys.exit(1)

    router_command = [sys.argv[1]] + ROUTER_ARGS

    test_connction_drop(router_command)
    test_router_restart(router_command)


if __name__ == "__main__":
    main()
