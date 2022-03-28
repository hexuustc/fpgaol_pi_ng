#include "fpga.h"
#include "peripherals.h"
#include "io_defs_hal.h"
#include <iostream>
#include <sstream>

#define WF_FILENAME "/home/pi/docroot/waveform.vcd"
#define WD_PIN 31
#define WD_MASK (1 << 31)

#define LOOP_SLEEP_US (93 * 1000)
#define LIGHT_THRESH (LOOP_SLEEP_US / 8 / 3)

static int pig_pid;
static int notify_handle;
//static volatile int g_reset_counts = 0;
//static volatile uint64_t num_count[8][16];
//static uint64_t prev_level = 0, prev_tick = 0, cur_output = 0;
//static uint64_t seg_value = 0, seg_idx = 0;

static bool debugging;
static bool notifying = false;
FPGA *fpga_instance = nullptr;
QMutex mx;

extern bool gpio_read_result[100];

/*
 * This watchdog wave is used to wake up the monitor thread
 * every 5 ms. We use this for the following reasons:
 * - If the 7-seg signal keeps unchanged, this wave helps
 *   calculate the duration of this signal.
 * - With this GPIO changing, the pig2vcd program is able to
 *   detect level changes upon program successful, so we can
 *   get a complete waveform.
 */
static int start_watchdog()
{
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

int ginputn, goutputn, gsegn;
//int available_gpio[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 38, 39, 40, 41, 42, 43, 44, 45};
int INPUT[MAX_GPIO];
uint64_t INPUT_MASK;
int OUTPUT[MAX_GPIO];
uint64_t OUTPUT_MASK;
int SEG[MAX_GPIO];
uint64_t SEG_MASK;
uint64_t GPIO_MASK;

// DEPRECATED
//int init_gpio(int inputn, int outputn, int segn)
//{
	//ginputn = inputn;
	//goutputn = outputn;
	//gsegn = segn;
	//int iret = gpioinitialise();
	//if (iret < 0)
		//return iret;

	//input_mask = 0;
	//output_mask = 0;
	//seg_mask = 0;
	//gpio_mask = 0;
	//int count = 0;
	//for (int i = 0; i < inputn; i++)
	//{
		//gpiosetmode(available_gpio[i + count], pi_input);
		//input[i] = available_gpio[i + count];
		//input_mask |= 1 << available_gpio[i + count];
	//}
	//count += inputn;
	//for (int i = 0; i < outputn; i++)
	//{
		//gpiosetmode(available_gpio[i + count], pi_output);
		//output[i] = available_gpio[i + count];
		//output_mask |= 1 << available_gpio[i + count];
	//}
	//count += outputn;
	//for (int i = 0; i < segn; i++)
	//{
		//gpiosetmode(available_gpio[i + count], pi_output);
		//seg[i] = available_gpio[i + count];
		//seg_mask |= 1 << available_gpio[i + count];
	//}
	//count += segn;
	//gpio_mask = input_mask | output_mask | seg_mask;

	//gpiosetmode(wd_pin, pi_output);

	//start_watchdog();

	//qinfo() << "gpio initialized!\n";
	//std::cout<<"intialized"<<std::endl;
	//std::cout<<inputn<<std::endl;
	//std::cout<<outputn<<std::endl;
	//std::cout<<segn<<std::endl;
	//std::cout<<input_mask<<std::endl;
	//std::cout<<output_mask<<std::endl;
	//std::cout<<seg_mask<<std::endl;
	//return 0;
//}

//static void uart_fn(int serial_port)
//{
	//qInfo() << "Uart thread started\n";

	//while (true)
	//{
		//if (!notifying)
			//return;
		//char data[1000];
		//gpioDelay(1000);
		//mx.lock();
		//int available_data = serDataAvailable(serial_port);
		//if (available_data)
		//{
			//serRead(serial_port, data, available_data);
			//data[available_data] = '\0';
			//QJsonObject json;
			//json["type"] = "MSG";
			//json["values"] = QString(data);
			//auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);

			//if (debugging)
				//qDebug() << "Send: " << msg;

			//fpga_instance->call_send_fpga_msg(msg);
		//}
		//mx.unlock();
	//}
//}

/*
 * This is the thread to monitor LED and 7-seg level changes,
 * it's the main loop of our program
 */
// for the monitoring of low-speed peripherals like LED and BTN
// contains a tens-of-millisecond busy wait
static void slow_mon_thread()
{
	uint32_t fastpoll_ms = 5;
	uint32_t slowpoll_rate = 20;
	//uint32_t sslowpoll_rate = 200;
	uint32_t slow_cnt = 0;
	//uint32_t sslow_cnt = 0;

	//uint64_t count[goutputn][16];
	//uint64_t output;

	//QJsonObject output_json, seg_json;
	//QJsonArray output_val, seg_val;

	//for (int i = 0; i < goutputn; i++)
		//output_val.append(0);
	//for (int i = 0; i < gsegn; i++)
		//seg_val.append(-1);

	//output_json["type"] = "output";
	//seg_json["type"] = "seg";
	//output_json["values"] = output_val;
	//seg_json["values"] = seg_val;

	qInfo() << "Monitor thread started\n";
	while (1)
	{
		uint32_t iol = gpioRead_Bits_0_31();
		uint32_t ioh = gpioRead_Bits_32_53();
		for (int i = 0; i < 32; i++)
			gpio_read_result[i] = (iol >> i) % 2;
		for (int i = 0; i < 32; i++)
			gpio_read_result[32+i] = (ioh >> i) % 2;
		slow_cnt++;
		//sslow_cnt++;
		//qDebug() << "LOOP";

		// fast poll, like 7-seg display
		int hexplay_id = periphstr2id_map.find("HEXPLAY")->second;
		std::vector<Periph*>& v = periph_arr.find(hexplay_id)->second;
		for(int i = 0; i < (int)v.size(); i++) {
			Periph* p = v[i];
			HEXPLAY* q = static_cast<HEXPLAY*>(p);
			q->poll();
		}


		// slow poll, like LED
		if (slow_cnt == slowpoll_rate) {
			slow_cnt = 0;
			//qDebug() << "SLOWPOLL";
			int led_id = periphstr2id_map.find("LED")->second;
			std::vector<Periph*>& v1 = periph_arr.find(led_id)->second;
			for(int i = 0; i < (int)v1.size(); i++) {
				Periph* p = v1[i];
				LED* q = static_cast<LED*>(p);
				q->poll();
			}

			int uart_id = periphstr2id_map.find("UART")->second;
			std::vector<Periph*>& v2 = periph_arr.find(uart_id)->second;
			for(int i = 0; i < (int)v2.size(); i++) {
				Periph* p = v2[i];
				UART* q = static_cast<UART*>(p);
				q->poll();
			}
		}

		// very slow poll, just sync everything periodically

		gpioDelay(fastpoll_ms * 1000);

		if (!notifying) {
			qDebug() << "Monitor thread return";
			return;
		}

		//memcpy(count, (void *)num_count, sizeof(count));

		//g_reset_counts = 1;

		// Has outputs changed?
		//output = ((gpioRead_Bits_32_53() << 32) | gpioRead_Bits_0_31()) & OUTPUT_MASK;
		// printf("LED : %04X\n", led);
		//if (output ^ cur_output)
		//{
			
			//std::cout<<"led value chanched"<<std::endl;
			//std::cout<<output<<std::endl;
			//std::cout<<gpioRead_Bits_0_31()<<std::endl;
			//cur_output = output;

			//for (int i = 0; i < goutputn; i++)
			//{
				//output_val[i] = (int)((cur_output >> OUTPUT[i]) & 1);
			//}

			//output_json["values"] = output_val;

			//auto msg = QJsonDocument(output_json).toJson(QJsonDocument::Compact);

			//if (debugging)
				//qDebug() << "Send: " << msg;

			//fpga_instance->call_send_fpga_msg(msg);
		//}

		////   printf("Total: %d\n", cur_total_count);
		//continue;
		//bool changed = false;
		//for (int i = 0; i < goutputn; i++)
		//{
			//uint64_t max = 0, max_num = 0, total = 0;
			//for (int j = 0; j < 16; j++)
			//{
				//total += count[i][j];
				//if (count[i][j] > max)
				//{
					//max = count[i][j];
					//max_num = j;
				//}
			//}

			////  printf("Index: %d, display: %d, light: %u, total: %u\n", i, max_num, max, total);

			//int this_val;
			//if (total < LIGHT_THRESH)
			//{
				//this_val = -1;
			//}
			//else
			//{
				//this_val = max_num;
			//}

			//if (seg_val[goutputn - 1 - i] != this_val)
			//{
				//seg_val[goutputn - 1 - i] = this_val;
				//changed = true;
			//}
		//}

		//if (changed)
		//{
			//seg_json["values"] = seg_val;
			//auto msg = QJsonDocument(seg_json).toJson(QJsonDocument::Compact);

			//if (debugging)
				//qDebug() << "Send: " << msg;

			//fpga_instance->call_send_fpga_msg(msg);
		//}

		//// printf("\n");
	}
}

/*
 * This is the function called by pigpio every 1 ms,
 * it calculates each duration when AN is `i` and
 * D is `j`
 */

// fast threads being called
// to handle mid-speed peripherals like 7-seg displays

//static void fast_callback(const gpioSample_t *samples, int numSamples)
//{
	//const int *SEG_INDEX = SEG + 4;

	//if (g_reset_counts)
	//{
		//g_reset_counts = 0;
		//memset((void *)num_count, 0, sizeof(num_count));
	//}

	//if (prev_tick == 0)
		//prev_tick = samples[0].tick; // Initialize

	//for (int s = 0; s < numSamples; s++)
	//{
		//auto level = samples[s].level;
		//// printf("%x\n",level);
		//if ((level ^ prev_level) & (GPIO_MASK | WD_MASK))
		//{
			//num_count[seg_idx][seg_value] += (samples[s].tick - prev_tick);
			//prev_tick = samples[s].tick;
			//prev_level = level;

			//seg_idx = (level >> SEG_INDEX[0]) & 1;
			//seg_idx |= ((level >> (SEG_INDEX[1])) & 1) << 1;
			//seg_idx |= ((level >> (SEG_INDEX[2])) & 1) << 2;

			//seg_value = (level >> SEG[0]) & 1;
			//seg_value |= ((level >> SEG[1]) & 1) << 1;
			//seg_value |= ((level >> SEG[2]) & 1) << 2;
			//seg_value |= ((level >> SEG[3]) & 1) << 3;
		//}
	//}
//}

/*
 * Program the FPGA.
 * FPGAOL 1 uses djtgcfg or openocd
 * FPGAOL 2 uses ustcfg
 */
int FPGA::program_device()
{
	system("unzip -o /home/pi/bistream/bitstream.zip -d /home/pi/bistream/");
	int wstatus = system("ustcfg1 prog -f /home/pi/bistream/bitstream.bit");
	system("rm -rf /home/pi/bistream/bitstream.bit");
	return WEXITSTATUS(wstatus);
}

/*
 * initialize all customized peripherals and related hw ports
 * called after customization finish immediately to start acquisition
 * ---
 * it opens the notification for all needed GPIO pins,
 * and spawns pig2vcd to generate waveform
 */
int FPGA::start_notify(QString msg)
{
	// TODO: init orders?
	if (gpioInitialise() < 0)
		qDebug() << "ERR: gpioInitialise returned negative!";
	//start_watchdog();
	//notify_handle = gpioNotifyOpen();
	//qDebug() << "pigpio notify_handle: " << notify_handle;
	//char fifo[20];
	//sprintf(fifo, "/dev/pigpio%d", notify_handle);

	platform_dependent_gpio_init();

	std::stringstream xdc_ss;
	xdc_ss << "# FPGAOL AUTO GEN XDC V1.0" << std::endl;
	xdc_ss << "set_property -dict {PACKAGE_PIN B8 IOSTANDARD LVCMOS33} [get_ports {clk}];" << std::endl;
	xdc_ss << "create_clock -add -name sys_clk_pin -period 10.00 -waveform {0 5} [get_ports {clk}];" << std::endl;

	QJsonObject j_reply;
	QString msgback;

	int pipin_cnt = 0;
	auto json = QJsonDocument::fromJson(msg.toUtf8());
	QJsonArray periphs = json.object().value("periphs").toArray();
	foreach (const QJsonValue &entry, periphs) {
		std::string p_type = entry.toObject().value("type").toString().toStdString();
		std::map<std::string, int>::iterator itr = periphstr2id_map.find(p_type);
		if (itr == periphstr2id_map.end()) {
			qDebug() << "ERR: unknown periph type!!";
			goto fail;
		}
		// after this point, map.find is guarenteed to be valid
		int p_id = itr->second;
		int p_idx = entry.toObject().value("idx").toInt();
		std::vector<Periph*>& vec = periph_arr.find(p_id)->second;
		int p_pincnt = pincnt_map.find(p_id)->second;
		int p_needpoll = needpoll_map.find(p_id)->second;
		int p_pin_arr[PERIPH_MAX_PINS];
		// FPGAOL 2 has simple pin allocation
		// just avoid UART/SPI pins
		// we have a lot to waste
		for (int i = 0; i < p_pincnt; i++) {
			if (pipin_cnt >= IO_MAX) {
				qDebug() << "ERR: too many pins!";
				j_reply["id"] = 0;
				j_reply["type"] = "XDC";
				j_reply["payload"] = "# no enough pins! please reduce peripheral count";
				msgback = QJsonDocument(j_reply).toJson(QJsonDocument::Compact);
				fpga_instance->call_send_fpga_msg(msgback);
				goto fail;
			}
			if (gpio_arr[pipin_cnt].special) {
				std::cout << "Skip special pin" << std::endl;
				i--;
				pipin_cnt++;
			}
			else p_pin_arr[i] = pipin_cnt++;
		}
		// create Periph
		if (p_id == LED_ID) {
			std::cout << "LED " << p_idx;
			gpioSetMode(p_pin_arr[0], PI_INPUT);
			vec.push_back(new LED(p_id, p_idx, p_needpoll, p_pincnt, p_pin_arr));
			// TODO: this is only semi-automated, should integrate pin name into another map
			xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_pin_arr[0]] << " IOSTANDARD LVCMOS33} [get_ports {led[" << p_idx << "]}];" << std::endl;
		} else if (p_id == BTN_ID) {
			std::cout << "BTN " << p_idx;
			gpioSetMode(p_pin_arr[0], PI_OUTPUT);
			gpioWrite(p_pin_arr[0], 0);
			vec.push_back(new BTN(p_id, p_idx, p_needpoll, p_pincnt, p_pin_arr));
			xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_pin_arr[0]] << " IOSTANDARD LVCMOS33} [get_ports {sw[" << p_idx << "]}];" << std::endl;
		} else if (p_id == UART_ID) {
			std::cout << "UART " << p_idx;
			if (p_idx > 0) {
				qDebug() << "Only two UART is available!";
				continue;
			}
			// the second serial just fails
			xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_idx == 0 ?14:40] << " IOSTANDARD LVCMOS33} [get_ports {uart_rx" << p_idx << "}];" << std::endl;
			xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_idx == 0 ?15:41] << " IOSTANDARD LVCMOS33} [get_ports {uart_tx" << p_idx << "}];" << std::endl;
			int p_baud = entry.toObject().value("baud").toInt();
			if (!p_baud) p_baud = 115200;
			vec.push_back(new UART(p_id, p_idx, p_needpoll, p_pincnt, p_pin_arr, p_baud));
			
		} else if (p_id == HEXPLAY_ID) {
			std::cout << "HEXPLAY " << p_idx;
			for (int j = 0; j <= 6; j++)
				gpioSetMode(p_pin_arr[j], PI_INPUT);
			for (int j = 0; j <= 2; j++)
				xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_pin_arr[j]] << " IOSTANDARD LVCMOS33} [get_ports {hexplay" << p_idx << "_an[" << j << "]}];" << std::endl;
			for (int j = 0; j <= 3; j++)
				xdc_ss << "set_property -dict {PACKAGE_PIN " << pi2_io2fpin[p_pin_arr[3+j]] << " IOSTANDARD LVCMOS33} [get_ports {hexplay" << p_idx << "_d[" << j << "]}];" << std::endl;
			vec.push_back(new HEXPLAY(p_id, p_idx, p_needpoll, p_pincnt, p_pin_arr));
		} else {
			// periphs can be unsupported, but its information
			// must be still in code
			qDebug() << "ERR: unsupported periph!!";
			goto fail;
		}
	}

	// VCD monitor and generator

	//pig_pid = fork();

	//if (pig_pid == 0)
	//{
		//// TODO: pay attention to VCD
		//// this is probably broken now
		//int fd = open(fifo, O_RDONLY, 0);
		//int out_fd = open(WF_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0);
		//chmod(WF_FILENAME, 0666);
		//dup2(fd, STDIN_FILENO);
		//dup2(out_fd, STDOUT_FILENO);

		//int a = execl("/usr/local/bin/pig2vcd", "pig2vcd", (char *)0);
		//if (a)
			//perror("EXE error: ");
		//return 0;
	//}

	//for(auto i : INPUT)
	//{
		//write_gpio(i, 0);
	//}
	int ret;
	
	//std::cout<< "Clear returned: " << ret1 << " and " << ret2<<std::endl;
	std::cout<< "Clear returned: " << INPUT_MASK<<std::endl;
	std::cout<< "Clear returned: " << gpioRead_Bits_0_31()<<std::endl;
	
	//if (m_debug)
		//qDebug() << "Clear returned: " << ret1 << " and " << ret2;

	//ret = gpioNotifyBegin(notify_handle, 0xFFFFFFFFU);
	//if (m_debug)
		//qDebug() << "NotifyBegin returned: " << ret;

	// per-ms sample callback -- for fast tasks
	//ret = gpioSetGetSamplesFunc(fast_callback, GPIO_MASK | WD_MASK);
	//if (m_debug)
		//qDebug() << "SetGetSFN returned: " << ret;

	//serial_port = serOpen("/dev/serial0", 115200, NULL);
	//if (m_debug)
		//qDebug() << "SerialOpen returned: " << serial_port;

	qInfo() << "Notify started";
	notifying = true;
	std::cout << "Started" << std::endl;

	// slow monitor threads -- for slow tasks
	monitor_thrd = std::thread(slow_mon_thread);
	//uart_thrd = std::thread(uart_fn, serial_port);


	// everything OK, generate a XDC constraint file 
	// and send back to frontend
	qDebug() << xdc_ss.str().c_str();
	j_reply["id"] = 0;
	j_reply["type"] = "XDC";
	j_reply["payload"] = xdc_ss.str().c_str();
	msgback = QJsonDocument(j_reply).toJson(QJsonDocument::Compact);
	fpga_instance->call_send_fpga_msg(msgback);

	return 0;
fail:
	return -1;
}

// free all Periph in array
int FPGA::end_notify()
{
	if (notifying)
	{
		notifying = false;
		monitor_thrd.join();
		//uart_thrd.join();
		//gpioNotifyClose(notify_handle);
		gpioWrite_Bits_0_31_Clear(0xFFFFFFFFU);
		gpioWrite_Bits_32_53_Clear(0xFFFFFFFFU);
		//gpioWrite_Bits_0_31_Clear(INPUT_MASK);
		//gpioWrite_Bits_32_53_Clear(INPUT_MASK >> 32);
		//serClose(serial_port);

		//char cmd[50];
		//sprintf(cmd, "sudo kill -9 %d", pig_pid);
		//system(cmd);
		//waitpid(pig_pid, NULL, 0);
		gpioSetGetSamplesFunc(NULL, 0);
	}
	gpioTerminate();
	platform_dependent_gpio_fini();

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

FPGA::FPGA(bool debug)
{
	m_debug = debug;
	debugging = debug;
	fpga_instance = this;
	//if (init_gpio(0, 0, 0) != 0)
	if (gpioInitialise() < 0)
	{
		throw std::runtime_error("GPIO initialization falied!");
	}
	std::cout << "Init" << std::endl;
	// system("sudo chmod 666 /dev/serial0");
	// connect(serial_port,&QSerialPort::readyRead,this,FPGA::readData);
	// serial_port.setPortName("/dev/serial0");
	// serial_port.setBaudRate(QSerialPort::Baud115200);
}

FPGA::~FPGA()
{
	gpioTerminate();
	fpga_instance = nullptr;
}

void FPGA::call_send_fpga_msg(QString msg)
{
	emit send_fpga_msg(msg);
}

//int FPGA::write_gpio(int gpio, int level)
//{
	//if (!notifying)
		//return -1;

	//return gpioWrite(gpio, level);

	//qDebug() << "WARNING: writing GPIO into undefined place";
//}

//int FPGA::write_serial(QByteArray msg)
//{
	//if (!notifying)
		//return -1;

	//mx.lock();
	//int status = serWrite(serial_port, (char *)msg.toStdString().data(), msg.toStdString().length());
	//mx.unlock();
	//return status;
//}

//int FPGA::set_soft_clock(int freq_hz)
//{
	//if (!notifying)
		//return -1;

	//return 0;
//}
