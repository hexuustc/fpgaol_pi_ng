from time import localtime, strftime
import os
import socket
import subprocess

PORT = 7836
HOST = ''  # Get localhost.
DEVICE = 'Nexys4DDR'

s = socket.socket()
s.bind((HOST, PORT))
s.listen(5)


def create_dest_dir():
    """Create a different directory for each connection."""
    dir_name = os.path.join('/home/pi/FPGA/', strftime('%m%d%H%M%S', localtime()))
    os.mkdir(dir_name)
    return os.path.join(dir_name, 'top.bit')


def program_device(file_name):
    cmd = 'djtgcfg init -d {} && djtgcfg prog -d {} -f {} -i 0'.format(
        DEVICE, DEVICE, file_name
    )
    return os.system(cmd)


while True:
    conn, addr = s.accept()
    dest_filename = create_dest_dir()
    print('Start')
    with open(dest_filename, 'wb') as dest:
        data = conn.recv(32768)
        while data:
            dest.write(data)
            data = conn.recv(32768)
    l = program_device(dest_filename)
    print(l)
    status = bytes(str(l), encoding="ascii")
    conn.sendall(status)
    # conn.sendall(bytes(str(program_device(dest_filename)))
    conn.close()