from time import localtime, strftime, sleep
import pigpio
import json
import os
import socket
import subprocess

PORT = 7836
HOST = ''  # Get localhost.
DEVICE = 'Nexys4DDR'

s = socket.socket()
s.bind((HOST, PORT))
s.listen(5)

SW  =  [8,9,10,11,12,13,14,15]
# JB5 and JB11 are Ground pins, JB6 and JB12 are VCC pins.

# Correspoding to: JA1, JA2, JA3, JA4, JA7, JA8, JA9, JA10.
LED   =  [0,1,2,3,4,5,6,7]
# JA5 and JA11 are Ground pins, JA6 and JA12 are VCC pins.

# Correspoding to:
SEG   =  [16,17,18,19,20,21]

# Correspoding to: JD1
BUTTON   =  [22]

# Correspoding to: JD2
SOFT_CLOCK   =  [23]

PINS_TO_READ = LED + SEG
PINS_TO_WRITE = BUTTON + SOFT_CLOCK + SW

WATCHDOG_PIN = 2
TIMEOUT_INTERVAL = 1000  # 1s
HOLD_TIME_LIMIT = 600  # 10min

hostname = socket.gethostname()
url = 'http://192.168.1.100/fpga/release/' + hostname + '/'
failover_url = 'http://192.168.1.100/fpga/fail/' + hostname + '/'
pi = pigpio.pi()
cbs = []
logging.basicConfig(format='%(asctime)s line:%(lineno)s,  %(message)s', level=logging.INFO)
logger = logging.getLogger(__name__)
clockInterval = [-1, -1]


for i in PINS_TO_READ:
    pi.set_mode(i, pigpio.INPUT)

for i in PINS_TO_WRITE:
    pi.set_mode(i, pigpio.OUTPUT)
    # Make sure the pins are initialised to 0.
    pi.write(i, 0)

def end_notify(pi, release_pi=False):
    """Reset output pins of the Pi, and cancel callbacks."""
    for i in PINS_TO_WRITE:
        # Make sure the pins are restored to 0.
        pi.write(i, 0)

    # Cancel callbacks
    for cb in cbs:
        cb.cancel()

    pi.set_watchdog(WATCHDOG_PIN, 0)  # Cancel watchdog.

    logger.info('End notify.')

def process_bank1(bank1):
    ret_val, index = 0, 0
    for i in PINS_TO_READ:
        ret_val += ((bank1 >> i) & 1) << index
        index += 1
    return ret_val

def start_notify(socket, pi):
    """Set callback."""
    # A time stamp.
    start_tick = pi.get_current_tick()

    def read_all_pins(gpio, level, tick):
        """The function to call every `Pi.TIMEOUT_INTERVAL` ms."""
        try:
            # Send WebSocket packet .
            socket.write_message(json.dumps({
                'gpio': -1,
                # NOTE: We encoded ALL the output bits of the FPGA here.
                'level': process_bank1(pi.read_bank_1()),
                'tick': pigpio.tickDiff(start_tick, tick),
                })
             )
        except Exception as e:
            # May raise ConnectionClosedError
            logger.info(e)

    # 'tick' should be get_current_tick() but the deviation is minor.
    read_all_pins(0, 0, start_tick)

    def get_read_gpio_index(gpio):
        """Convert Pi GPIO index to encoded gpio"""
        if gpio in SEG:
            # Segments are indexed 8-15, see README.md for detail
            # return SEG.index(gpio) + 8
            return SEG.index(gpio)
        else:
            # LEDs are indexed 0-7, see README.md for detail
            return LED.index(gpio)

    def send_notify_socket(gpio, level, tick):
        """The callback function when any of the LEDs change."""
        try:
            # Send WebSocket packet .
            socket.write_message(json.dumps(
               {
                   'gpio': get_read_gpio_index(gpio),
                   # NOTE: We only send ONE output bit of the FPGA here.
                   'level': level,
                   'tick': pigpio.tickDiff(start_tick, tick),
               })
             )
        except Exception as e:
            # May raise ConnectionClosedError
            logger.info(e)

    for i in PINS_TO_READ:
        cbs.append(
            pi.callback(i, pigpio.EITHER_EDGE, send_notify_socket)
        )

    cbs.append(
        pi.callback(WATCHDOG_PIN, pigpio.EITHER_EDGE, read_all_pins)
    )
    pi.set_watchdog(WATCHDOG_PIN, TIMEOUT_INTERVAL)
    logger.info('Start notify.')

def get_write_gpio_index(gpio):
    """Convert zero-indexed GPIOs to real Pi GPIOs"""
    if gpio < 8:
        # Switches are indexed 0-7, see README.md for detail
        return SW[gpio]
    elif gpio == 8:
        # Buttons begin from 8, see README.md for detail
        return BUTTON[gpio - 8]
    else:
        return -1
        
class FpgaolSocket(WebSocketHandler):
    def open(self):
        logger.info("WebSocket opened")

    def on_message(self, message):
        # Receive WebSocket packet and change corresponding input to FPGA.
        text_data_json = json.loads(message)

        gpio = int(text_data_json['gpio'])
        level = int(text_data_json['level'])

        if gpio == -1:
            # Stop notifications.
            end_notify(pi)
        elif gpio == -2:
            # Start notifications.
            start_notify(self, pi)
        elif gpio == -3:
            # Set Soft Clock0. We have only one soft clock now
            setClockStatus(0, level)
        else:
            # Do not ignore switch when soft clock is on anymore
            gpio = get_write_gpio_index(gpio)
            level = int(text_data_json['level'])
            if gpio != -1:
                pi.write(gpio, level)

    def on_close(self):
        end_notify(pi, release_pi=True)

    def check_origin(self, origin):
        return True


def setClockStatus(clockId, interval):
    clockInterval[clockId] = interval


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