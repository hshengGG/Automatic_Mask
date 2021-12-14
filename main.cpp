#include "mbed.h"
#include "rtos.h"
#include "LSM9DS1.h" //IMU
#include "XNucleo53L0A1.h" //lidar
#include "Servo.h"
#include "PinDetect.h" //Push button
#include "sneeze2.h" //sound file for sneezing
#define sample_freq 44100.0

//get and set the frequency from wav conversion tool GUI
int i=0;
AnalogOut DACout(p18);
Ticker sampletick;
//interrupt routine to play next audio sample from array in flash
void audio_sample ()
{
    DACout.write_u16(sound_data[i]);
    i++;
    if (i>= NUM_ELEMENTS) {
        i = 0;
        sampletick.detach();
    }
}

// initiation and global variables
DigitalOut led1(LED1);
DigitalOut led2(LED2);
Mutex serialmutex;
Serial device(p13, p14);  // tx, rx
PinDetect pb(p11);
Serial pc(USBTX, USBRX);
DigitalOut lidar(p26);
Servo myservo(p23);
volatile bool maskup = false;
volatile int sneeze;
volatile int last;

void vr_thread(void const *args)
{
    char rchar=0;
    while (1) {
//        serialmutex.lock();
        device.putc('i'); //Start Recognition
        device.putc('C'); //Use Wordset 3 - the numbers 1..10
//Use built-in speaker independent numbers (0..10) and listen for a number
        while (device.readable()!=0) {}
        if (device.getc()=='s') {
            device.putc(' ');
            rchar=device.getc();
            if (rchar=='C') {
                maskup = true;
                led1 = 1;
            }
            if (rchar=='D') {
                maskup = false;
                led1 = 0;
            }
        }
        Thread::wait(1000);
    }
}

void servo_thread(void const *args)
{
    while (1) {
        if (maskup) {
            myservo = 0.4f;
        } else {
            myservo = 0.0f;
        }
//        pc.printf("servo");
        Thread::wait(100);
    }
}

void flip_mask()
{
    maskup = !maskup;
}

int main()
{
    pb.mode(PullUp);
    pb.attach_asserted(&flip_mask);
    pb.setSampleFrequency();

    LSM9DS1 lol(p9, p10, 0xD6, 0x3C); //IMu
    lol.begin();
    if (!lol.begin()) {
        pc.printf("Failed to communicate with LSM9DS1.\n");
    }
    lol.calibrate();
    device.putc('b');
    while (device.getc()!='o') {
        device.putc('b');
        Thread::wait(200);
    }

    Thread tvr(vr_thread); //start the voice recognition thread
    Thread tservo(servo_thread); // start the servo thread

    while(1) {
        last = lol.ay;
        lol.readAccel(); //read IMU
        pc.printf("accel: %d %d %d\n\r", lol.ax, lol.ay, lol.az);
        if (abs(lol.ay - last) >= 5000) {
            maskup = true; //turn on mask when you sneeze
            sampletick.attach(&audio_sample, 1.0 / sample_freq);

        }
        Thread::wait(500);
    }

}
