import subprocess
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import time
import datetime
import serial
import serial.serialutil
from pynput.keyboard import Key, Listener


class AnalyzerTools:
    def __init__(self, collect, analyze):

        self.DATA_DIR= r"C:\builds\build_svd\data"

        self.stop_reading = False
        self.spin_cycle = False

        self.state = 0

        if collect:
            for i in range(1, 99):
                self.DATA = str(datetime.date.today()) + "_washer" + str(i)
                self.DATA_FILE=os.path.join(self.DATA_DIR, self.DATA + ".csv")
                if not os.path.exists(self.DATA_FILE):
                    break

            self.readData()
        
        if analyze:
            self.DATA = "2024-06-12_Washer06"
            self.DATA_FILE=os.path.join(self.DATA_DIR, self.DATA + ".csv")

            self.analyzeData()

    
    def detectSpinCycle(self):

        match self.state:

            case 0:
                return "Init"
            
            case 1:
                return "Start"



    def analyzeData(self):
        ts_raw = []
        ts_total = []

        imu_accel_x = []
        imu_accel_y = []
        imu_accel_z = []

        imu_gyro_x = []
        imu_gyro_y = []
        imu_gyro_z = []

        imu_gyro_x_avg = []
        imu_gyro_y_avg = []
        imu_gyro_z_avg = []

        imu_movement = []
        imu_movement_avg = []
        imu_spin = []

        initTime = 0
        totalTime = 0

        MOVEMENT_WARNING_TIMEOUT = 20  # rate: 2 samples/second
        MOVEMENT_STOPPED_TIMEOUT = 60 #in seconds
        #MOVEMENT_STOPPED_TIMEOUT = 120 #in seconds

        timer_no_movement_stopped = MOVEMENT_STOPPED_TIMEOUT
        timer_no_movement_warning = MOVEMENT_WARNING_TIMEOUT
        timer_no_movement_stopped_avg = MOVEMENT_STOPPED_TIMEOUT
        timer_no_movement_warning_avg = MOVEMENT_WARNING_TIMEOUT

        timer_moving = 0

        gyroX = 0
        gyroY = 0
        gyroZ = 0

        n_average = 20
        gyroX_avg = 0
        gyroY_avg = 0
        gyroZ_avg = 0

        imu_data = open(self.DATA_FILE, 'r')
        imu_data.readline() # Discard header

        while True:
            line = imu_data.readline()
            if not line:
                break

            imu_line = line.split(",")

            if len(imu_line) != 4:
                print("Bad line: {0}\n".format(imu_line))
                continue

            '''
            ts_string = imu_line[0]
            element = datetime.datetime.strptime(ts_string,"%Y-%m-%d %H:%M:%S.%f")
            timestamp = datetime.datetime.timestamp(element)
            '''
            timestamp = float(imu_line[0]) / 1000.0

            if initTime != 0:
                totalTime = timestamp - initTime
            else:
                initTime = timestamp
        
            ts_raw.append(timestamp)
            ts_total.append(totalTime)

            '''
            imu_accel_x.append(float(imu_line[1]))
            imu_accel_y.append(float(imu_line[2]))
            imu_accel_z.append(float(imu_line[3]))

            gyroX=float(imu_line[4])
            gyroY=float(imu_line[5])
            gyroZ=float(imu_line[6])
            '''

            #gyroX=float(imu_line[1])
            #gyroY=float(imu_line[2])
            #gyroZ=float(imu_line[3])

            gyroX=abs(float(imu_line[1]))
            gyroY=abs(float(imu_line[2]))
            gyroZ=abs(float(imu_line[3]))

            gyro_vals = [abs(gyroX), abs(gyroY), abs(gyroZ)]
            gyro_max = max(gyro_vals)
            gyro_min = min(gyro_vals)

            imu_gyro_x.append(gyro_max)
            #imu_gyro_x.append(abs(gyroX))
            #imu_gyro_y.append(abs(gyroY))
            #imu_gyro_z.append(abs(gyroZ))

            gyroX_avg = ((gyroX_avg * (n_average - 1)) + gyroX) / n_average
            gyroY_avg = ((gyroY_avg * (n_average - 1)) + gyroY) / n_average
            gyroZ_avg = ((gyroZ_avg * (n_average - 1)) + gyroZ) / n_average

            gyro_vals_avg = [abs(gyroX_avg), abs(gyroY_avg), abs(gyroZ_avg)]
            gyro_max_avg = max(gyro_vals_avg)
            gyro_min_avg = min(gyro_vals_avg)

            imu_gyro_x_avg.append(gyro_max_avg)
            #imu_gyro_x_avg.append(abs(gyroX_avg))
            #imu_gyro_y_avg.append(abs(gyroY_avg))
            #imu_gyro_z_avg.append(abs(gyroZ_avg))

            #if abs(gyroX) < 1.0 and abs(gyroY) < 1.0 and abs(gyroZ) < 1.0:
            if gyro_max < 1.0:
                #if totalTime > timer_no_movement_stopped and self.spin_cycle:
                if totalTime > timer_no_movement_stopped:
                    imu_movement.append(-5)
                elif totalTime > timer_no_movement_warning:
                    imu_movement.append(0)
                else:
                    imu_movement.append(5)
            else:
                timer_no_movement_stopped = totalTime + MOVEMENT_STOPPED_TIMEOUT
                timer_no_movement_warning = totalTime + MOVEMENT_WARNING_TIMEOUT 
                imu_movement.append(5)

            #if abs(gyroX_avg) < 1.0 and abs(gyroY_avg) < 1.0 and abs(gyroZ_avg) < 1.0:
            if gyro_max_avg < 1.0:
                if totalTime > timer_no_movement_stopped_avg:
                    imu_movement_avg.append(-5)
                elif totalTime > timer_no_movement_warning_avg:
                    imu_movement_avg.append(0)
                else:
                    imu_movement_avg.append(5)
            else:
                timer_no_movement_stopped_avg = totalTime + MOVEMENT_STOPPED_TIMEOUT
                timer_no_movement_warning_avg = totalTime + MOVEMENT_WARNING_TIMEOUT 
                imu_movement_avg.append(5)
            
            '''
            imu_gyro_avg.append(abs(gyroX))
            if totalTime > 120:
                gyro_mean = sum(imu_gyro_avg) / len(imu_gyro_avg) 
                if gyro_mean > 5.0:
                    self.spin_cycle = True
                    imu_spin.append(10)
                else:
                    imu_spin.append(0)

                imu_gyro_avg.pop(0)
            else:
                imu_spin.append(0)
            
            if len(imu_gyro_y) > 200:
                gyro_series = pd.Series(imu_gyro_y)
                mean_list = gyro_series.rolling(200).mean()
                gyro_mean = mean_list[len(mean_list)-1]
                if gyro_mean > 5.0:
                    self.spin_cycle = True
                    imu_spin.append(10)
                else:
                    imu_spin.append(0)
            else:
                imu_spin.append(0)
            '''

        imu_data.close()
    
        #-------------------------------------------------------------------------------------
        '''
        plt.figure(figsize=(10, 5))
        plt.grid()
        plt.xlabel("Samples")
        plt.ylabel("Time")
        plt.title("[{0}] Timestamp Data [{1} secs]".format(self.DATA, MOVEMENT_STOPPED_TIMEOUT))
        plt.plot(ts_raw, linestyle="-", marker='o', markersize=2, color='black', label="Timestamp")
        plt.legend()
        '''

        #-------------------------------------------------------------------------------------
        plt.figure(figsize=(10, 5))
        plt.grid()
        plt.xlabel("Time")
        plt.ylabel("Gyro")
        plt.title("[{0}] Gyro Data [{1} secs]".format(self.DATA, MOVEMENT_STOPPED_TIMEOUT))

        plt.plot(ts_total, imu_gyro_x, linestyle="-", marker='o', markersize=1, color='red',   label="Gyro X")
        #plt.plot(ts_total, imu_gyro_y, linestyle="-", marker='o', markersize=1, color='green', label="Gyro Y")
        #plt.plot(ts_total, imu_gyro_z, linestyle="-", marker='o', markersize=1, color='blue',  label="Gyro Z")
        plt.plot(ts_total, imu_movement, linestyle="-", marker='o', markersize=2, color='black',  label="Movement")
        plt.legend()

        #-------------------------------------------------------------------------------------
        plt.figure(figsize=(10, 5))
        plt.grid()
        plt.xlabel("Time")
        plt.ylabel("Gyro")
        plt.title("[{0}] Avegage Gyro Data [{1} secs]".format(self.DATA, MOVEMENT_STOPPED_TIMEOUT))

        plt.plot(ts_total, imu_gyro_x_avg, linestyle="-", marker='o', markersize=1, color='red',   label="Gyro X")
        #plt.plot(ts_total, imu_gyro_y_avg, linestyle="-", marker='o', markersize=1, color='green', label="Gyro Y")
        #plt.plot(ts_total, imu_gyro_z_avg, linestyle="-", marker='o', markersize=1, color='blue',  label="Gyro Z")
        plt.plot(ts_total, imu_movement_avg, linestyle="-", marker='o', markersize=2, color='black',  label="Movement")
        plt.legend()

        #-------------------------------------------------------------------------------------
        '''
        plt.figure(figsize=(10, 5))
        plt.grid()
        plt.xlabel("Time")
        plt.ylabel("Accel")
        plt.title("[{0}] Accel Data".format(self.DATA))

        plt.plot(ts_total, imu_accel_x, linestyle="-", marker='o', markersize=1, color='red',   label="Accel X")
        plt.plot(ts_total, imu_accel_y, linestyle="-", marker='o', markersize=1, color='green', label="Accel Y")
        plt.plot(ts_total, imu_accel_z, linestyle="-", marker='o', markersize=1, color='blue',  label="Accel Z")
        plt.legend()
        '''

        plt.show()

    def on_press(self, key):

        if key.char == 'q':
            print('\nYou Entered {0}'.format( key))
            self.stop_reading = True
            return False

    def readData(self):

        listener = Listener(on_press = self.on_press)
        listener.start()

        print("Writing to file: {0}\n".format(self.DATA_FILE))
        imu_data = open(self.DATA_FILE, 'w')

        samples = 0
        try:
            ser = serial.Serial("COM3", baudrate=115200, timeout=1)
            while not self.stop_reading:
                ser_data = ser.readline()
                str_data = str(ser_data, 'ascii').strip('\n\r')
                imu_data.write(str_data + '\n')
                if samples % 20 == 0:
                    imu_data.flush()
                    print(".", end='')
                    sys.stdout.flush()
                samples = samples + 1
        except serial.serialutil.SerialException as e:
            ser.close()
            print(e)
    
        imu_data.close()
        listener.stop()


if __name__ == "__main__":

    argv = sys.argv[1:]

    analyzer=AnalyzerTools(False, True)
