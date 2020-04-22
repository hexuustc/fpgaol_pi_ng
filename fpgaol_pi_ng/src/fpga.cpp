#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

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

   if (debugging) qDebug() << "Monitor thread started\n";
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
		  // gpio_ws->sendTextMessage(msg);
		  // emit send_fpga_msg(msg);
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

      // TODO: Check serial
   }

}

static void sample_fn(const gpioSample_t *samples, int numSamples)
{
	const int *SEG_INDEX = SEG + 4;

	if (g_reset_counts)
	{
		g_reset_counts = 0;
		memset((void *)num_count, 0, sizeof(num_count));
	}

	if (prev_tick == 0) prev_tick = samples[0].tick; // Initialize

	// std::cout << "GPIO[31 -- 0]\t\t\t tick" << std::endl;
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

		// std::string mystring =
		// std::bitset<32>(level).to_string<char,std::string::traits_type,std::string::allocator_type>();
		// std::cout << mystring << " " << samples[s].tick << std::endl;
	}
}

int FPGA::start_notify() {
	// if (gpio_ws == nullptr || serial_ws == nullptr) return -1;

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

	ret = gpioNotifyBegin(notify_handle, GPIO_MASK);
	if (m_debug) qDebug() << "NotifyBegin returned: " << ret;

	ret = gpioSetGetSamplesFunc(sample_fn, GPIO_MASK | WD_MASK);
	if (m_debug) qDebug() << "SetGetSFN returned: " << ret;

	monitor_thrd = std::thread(loop_fn);

	if (m_debug) qDebug() << "Notify started";
	notifying = true;

	return 0;
}

int FPGA::end_notify() {
	if (notifying) {
		notifying = false;
		monitor_thrd.join();
		gpioNotifyClose(notify_handle);
		gpioWrite_Bits_0_31_Clear(SW_MASK);
		waitpid(pig_pid, NULL, 0);
		gpioSetGetSamplesFunc(NULL, GPIO_MASK | WD_MASK);
	}

	if (m_debug) qDebug() << "Notify end!";

	return 0;
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

	if (debugging) qDebug() << "GPIO initialized!\n";

	return 0;
}

FPGA::FPGA(bool debug) {
	m_debug = debug;
	debugging = debug;
	init_gpio();
	fpga_instance = this;
	// TODO: Initialize serial port
}

FPGA::~FPGA() {
	gpioTerminate();
	fpga_instance = nullptr;
}

void FPGA::call_send_fpga_msg(QString msg) {
	emit send_fpga_msg(msg);
}

void FPGA::call_send_uart_msg(QString msg) {
	emit send_uart_msg(msg);
}

int FPGA::write_gpio(int gpio, int level) {
	if (!notifying) return -1;

	if (gpio >= 8) return gpioWrite(BUTTON[gpio - 8], level);

	return gpioWrite(SW[gpio], level);
}

int FPGA::write_serial(QByteArray msg) {
	if (!notifying) return -1;

	return 0;
}

int FPGA::set_soft_clock(int freq_hz) {
	if (!notifying) return -1;

	return 0;
}
