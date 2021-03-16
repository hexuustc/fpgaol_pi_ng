#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#include <exception>
#include <thread>
#include <iostream>
#include <string>
#include <bitset>
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QJsonDocument>
#include <QIODevice>
#include <QMutex>

#include "fpga.h"
#include "pigpio.h"
#include "gpio_defs.h"

#define WF_FILENAME "/home/pi/docroot/waveform.vcd"
#define WD_PIN 2
#define WD_MASK (1 << 2)

#define LOOP_SLEEP_US (93 * 1000)
#define LIGHT_THRESH (LOOP_SLEEP_US / 8 / 3)

static int pig_pid;
static int notify_handle;
static volatile int g_reset_counts = 0;
static volatile uint32_t num_count[8][16];
static uint32_t prev_level = 0, prev_tick = 0, cur_led = 0;
static uint32_t seg_value = 0, seg_idx = 0;

static bool debugging;
static bool notifying = false;
FPGA *fpga_instance = nullptr;
QMutex mx;

/*
 * This watchdog wave is used to wake up the monitor thread
 * every 5 ms. We use this for the following reasons:
 * - If the 7-seg signal keeps unchanged, this wave helps
 *   calculate the duration of this signal.
 * - With this GPIO changing, the pig2vcd program is able to 
 *   detect level changes upon program successful, so we can 
 *   get a complete waveform.
 */
static int start_watchdog() {
	gpioWaveClear();

	gpioPulse_t pulse[2];

	pulse[0].gpioOn = WD_MASK;
	pulse[0].gpioOff = 0;
	pulse[0].usDelay = 5 * 1000;

	pulse[1].gpioOn = 0;
	pulse[1].gpioOff = WD_MASK;
	pulse[1].usDelay = 5 * 1000;

	gpioWaveAddNew();

	gpioWaveAddGeneric(2, pulse);

	int wave_id = gpioWaveCreate();

	if (wave_id >= 0)
	{
		gpioWaveTxSend(wave_id, PI_WAVE_MODE_REPEAT);
		return 0;
	}
	else
	{
		return -1;
		// Wave create failed.
	}
}

int init_gpio() {
	int iret = gpioInitialise();

	if (iret < 0) return iret;

	for (int i = 0; i < 8; i++) gpioSetMode(LED[i], PI_INPUT);

	for (int i = 0; i < 7; i++) gpioSetMode(SEG[i], PI_INPUT);

	for (int i = 0; i < 8; i++) gpioSetMode(SW[i], PI_OUTPUT);

	for (int i = 0; i < 1; i++) gpioSetMode(BUTTON[i], PI_OUTPUT);

	gpioSetMode(WD_PIN, PI_OUTPUT);

	start_watchdog();

	qInfo() << "GPIO initialized!\n";

	return 0;
}

static void uart_fn(QSerialPort *serial_port) {
   qInfo() << "Uart thread started\n";

   while (true) {
	   if (!notifying) return;
	   if (serial_port->waitForReadyRead(500)) {
		   mx.lock();
		//    puts("in 1");
		   QByteArray data = serial_port->readAll();
		   mx.unlock();
		//    puts("out 1");
		   if (!data.isEmpty()) {
			   QJsonObject json;
			   json["type"] = "MSG";
			   json["values"] = QString(data);
			//    printf("%s\n",data.toStdString().data());
			   auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);

			   if (debugging) qDebug() << "Send: " << msg;

			   fpga_instance->call_send_fpga_msg(msg);
		   }
	   }
   }
}

/*
 * This is the thread to monitor LED and 7-seg level changes,
 * it's the main loop of our program
 */
static void loop_fn() {
   uint32_t count[8][16];
   uint32_t led;

   QJsonObject led_json, seg_json;
   QJsonArray led_val, seg_val;

   for (int i = 0; i < 8; i++) led_val.append(0);
   for (int i = 0; i < 8; i++) seg_val.append(-1);

   led_json["type"] = "LED";
   seg_json["type"] = "SEG";
   led_json["values"] = led_val;
   seg_json["values"] = seg_val;

   qInfo() << "Monitor thread started\n";
   while (1)
   {
      gpioDelay(93 * 1000);

	  if (!notifying) return;

      memcpy(count, (void *)num_count, sizeof(count));

      g_reset_counts = 1;

      // Has leds changed?
      led = gpioRead_Bits_0_31() & LED_MASK;
	  // printf("LED : %04X\n", led);
      if (led ^ cur_led) {
	      cur_led = led;

		  for (int i = 0; i < 8; i++) {
			  led_val[i] = (int)((cur_led >> LED[i]) & 1);
		  }

		  led_json["values"] = led_val;

		  auto msg = QJsonDocument(led_json).toJson(QJsonDocument::Compact);

		  if (debugging) qDebug() << "Send: " << msg;

		  fpga_instance->call_send_fpga_msg(msg);
      }

      // printf("Total: %d\n", cur_total_count);

	  bool changed = false;
      for (int i=0; i < 8; i++)
      {
         uint32_t max = 0, max_num = 0, total = 0;
         for (int j = 0; j < 16; j++) {
			 total += count[i][j];
            if (count[i][j] > max) {
               max = count[i][j];
               max_num = j;
            }
         }

         // printf("Index: %d, display: %d, light: %u, total: %u\n", i, max_num, max, total);

		 int this_val;
		 if (total < LIGHT_THRESH) {
			 this_val = -1;
		 } else {
			 this_val = max_num;
		 }

		 if (seg_val[7 - i] != this_val) {
			 seg_val[7 - i] = this_val;
			 changed = true;
		 }
      }

	  if (changed) {
		  seg_json["values"] = seg_val;

		  auto msg = QJsonDocument(seg_json).toJson(QJsonDocument::Compact);

		  if (debugging) qDebug() << "Send: " << msg;

		  fpga_instance->call_send_fpga_msg(msg);
	  }

      // printf("\n");
   }

}

/*
 * This is the function called by pigpio every 1 ms,
 * it calculates each duration when AN is `i` and 
 * D is `j`
 */
static void sample_fn(const gpioSample_t *samples, int numSamples)
{
	const int *SEG_INDEX = SEG + 4;

	if (g_reset_counts)
	{
		g_reset_counts = 0;
		memset((void *)num_count, 0, sizeof(num_count));
	}

	if (prev_tick == 0) prev_tick = samples[0].tick; // Initialize

	for (int s = 0; s < numSamples; s++)
	{
		auto level = samples[s].level;
		if ((level ^ prev_level) & (GPIO_MASK | WD_MASK)) {
			num_count[seg_idx][seg_value] += (samples[s].tick - prev_tick);
			prev_tick = samples[s].tick;
			prev_level = level;

			seg_idx = (level >> SEG_INDEX[0]) & 1;
			seg_idx |= ((level >> (SEG_INDEX[1])) & 1) << 1;
			seg_idx |= ((level >> (SEG_INDEX[2])) & 1) << 2;

			seg_value = (level >> SEG[0]) & 1;
			seg_value |= ((level >> SEG[1]) & 1) << 1;
			seg_value |= ((level >> SEG[2]) & 1) << 2;
			seg_value |= ((level >> SEG[3]) & 1) << 3;
		}
	}
}

/*
 * Program the FPGA.
 * We can't call `system()` here, because the user of
 * `djtgcfg` would `root` and it can't find the FPGA
 */
int FPGA::program_device() {
	// system("unzip -o /home/pi/bistream/bitstream.zip -d /home/pi/bistream/");
	// int wstatus = system("ustcfg1 prog -f /home/pi/bistream/bitstream.bit");
	// system("rm -rf /home/pi/bistream/bitstream.bit");
	// return WEXITSTATUS(wstatus);
	system("unzip -o /home/pi/bistream/bitstream.zip -d /home/pi/bistream/");
	pid_t pid = fork();
	if (pid == 0) {
		setgid(1000);
		setuid(1000);
		putenv("HOME=/home/pi");
		execl("/usr/bin/djtgcfg", "djtgcfg", "prog", "-d", "Nexys4DDR",
				"-i", "0", "-f", "/home/pi/bistream/bitstream.bit", NULL);
	}
	int wstatus;
	waitpid(pid, &wstatus, 0);
	system("rm -rf /home/pi/bistream/bitstream.bit");
	return WEXITSTATUS(wstatus);
}

/*
 * This function is called upon program success,
 * it opens the notification for all needed GPIO pins,
 * and spawns pig2vcd to generate waveform
 */
int FPGA::start_notify() {
	char fifo[20];

	int notify_handle = gpioNotifyOpen();
	sprintf(fifo, "/dev/pigpio%d", notify_handle);


	pig_pid = fork();

	if (pig_pid == 0)
	{
		int fd = open(fifo, O_RDONLY, 0);
		int out_fd = open(WF_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0);
		chmod(WF_FILENAME, 0666);
		dup2(fd, STDIN_FILENO);
		dup2(out_fd, STDOUT_FILENO);

		int a = execl("/usr/local/bin/pig2vcd", "pig2vcd", (char *) 0);
		if (a) perror("EXE error: ");
		return 0;
	}

	int ret = gpioWrite_Bits_0_31_Clear(SW_MASK);
	if (m_debug) qDebug() << "Clear returned: " << ret;

	ret = gpioNotifyBegin(notify_handle, GPIO_MASK | WD_MASK);
	if (m_debug) qDebug() << "NotifyBegin returned: " << ret;

	ret = gpioSetGetSamplesFunc(sample_fn, GPIO_MASK | WD_MASK);
	if (m_debug) qDebug() << "SetGetSFN returned: " << ret;

	ret = (int)serial_port.open(QIODevice::ReadWrite);
	if (m_debug) qDebug() << "SerialOpen returned: " << ret;

	monitor_thrd = std::thread(loop_fn);
	// uart_thrd = std::thread(uart_fn, &serial_port);

	qInfo() << "Notify started";
	notifying = true;

	return 0;
}

int FPGA::end_notify() {
	if (notifying) {
		notifying = false;
		monitor_thrd.join();
		uart_thrd.join();
		gpioNotifyClose(notify_handle);
		gpioWrite_Bits_0_31_Clear(SW_MASK);
		serial_port.close();
		waitpid(pig_pid, NULL, 0);
		gpioSetGetSamplesFunc(NULL, 0);
	}

	qInfo() << "Notify end!";

	return 0;
}

// void FPGA::readData(){
// 	mx.lock();
// 	puts("in 1");
// 	QByteArray data = serial_port.readAll();
// 	mx.unlock();
// 	puts("out 1");
// 	if (!data.isEmpty()) {
// 		QJsonObject json;
// 		json["type"] = "MSG";
// 		json["values"] = QString(data);
// 		printf("%s\n",data.toStdString().data());
// 		auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);

// 		if (debugging) qDebug() << "Send: " << msg;

// 		call_send_fpga_msg(msg);
// 	}
// }

FPGA::FPGA(bool debug) {
	m_debug = debug;
	debugging = debug;
	fpga_instance = this;
	if (init_gpio() != 0) {
		throw std::runtime_error("GPIO initialization falied!");
	}
	system("sudo chmod 666 /dev/ttyUSB1");
	// connect(serial_port,&QSerialPort::readyRead,this,FPGA::readData);
	serial_port.setPortName("/dev/ttyUSB1");
	serial_port.setBaudRate(QSerialPort::Baud115200);
}

FPGA::~FPGA() {
	gpioTerminate();
	fpga_instance = nullptr;
}

void FPGA::call_send_fpga_msg(QString msg) {
	emit send_fpga_msg(msg);
}

// void FPGA::call_send_uart_msg(QString msg) {
// 	emit send_fpga_msg(msg);
// }

int FPGA::write_gpio(int gpio, int level) {
	if (!notifying) return -1;

	if (gpio == 8) return gpioWrite(BUTTON[gpio - 8], level);

	if (gpio >= 0 && gpio < 8) return gpioWrite(SW[gpio], level);
	
	qDebug() << "WARNING: writing GPIO into undefined place";
}

int FPGA::write_serial(QByteArray msg) {
	if (!notifying) return -1;

	mx.lock();
	// puts("in 2");
	int status = serial_port.write(msg);
	// printf("status=%d\n",status);
	mx.unlock();
	// puts("out 2");
	return status;
}

int FPGA::set_soft_clock(int freq_hz) {
	if (!notifying) return -1;

	return 0;
}
