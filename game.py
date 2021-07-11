import asyncio
import logging
import time
import pigpio
import serial
import glob

from surrortg import Game  # First we need import the Game
from surrortg.devices import Servo
from surrortg.inputs import Joystick, Switch

TOTAL_LAPS=3

class Arduino():
    def __init__(self, game):
        self.game = game
        self.running = True
        self.serial = serial.Serial(glob.glob("/dev/ttyUSB*")[0], 57600, timeout=1)
        logging.info("Starting arduino serial reader")
        loop = asyncio.get_event_loop()
        loop.add_reader(self.serial, self.eventHandler)

    def stop(self):
        logging.info("Stopping arduino serial reader")
        loop = asyncio.get_event_loop()
        loop.remove_reader(self.serial)

    def eventHandler(self):
        val = self.serial.readline().decode("utf-8").rstrip()
        if val[0] == 's':
            self.game.gate_passed(int(val[1]))

    def gate_up(self):
        self.serial.write("gup\n".encode('utf-8'))

    def gate_down(self):
        self.serial.write("gdown\n".encode('utf-8'))

class GateSwitch(Switch):
    def __init__(self, arduino):
        self.arduino = arduino

    async def on(self, seat=0):
        self.arduino.gate_up()

    async def off(self, seat=0):
        self.arduino.gate_down()

class TiltJoystick(Joystick):
    def __init__(self, arduino):
        self.arduino = arduino

    # Listeners
    async def handle_coordinates(self, x, y, seat=0):
        #logging.info(f"x:{x} y:{y}")
        x_message = f"x{int(x) * 45 + 90}\n"
        y_message = f"y{int(y) * 45 + 90}\n"        
        self.arduino.serial.write(x_message.encode('utf-8'))
        self.arduino.serial.write(y_message.encode('utf-8'))
class PinballRace(Game):

    async def on_init(self):
        logging.info("'on_init' started")
        self.arduino = Arduino(self)
        self.joystick = TiltJoystick(self.arduino)
        self.io.register_inputs({
            "joystick_main": self.joystick
        })
        await self.on_pre_game()
        logging.info("'on_init' ended")

    async def on_pre_game(self):
        logging.info("'on_pre_game' started")
        self.lap = 0
        self.next_gate = 1
        self.gate_sequence = { 0: 1, 1: 0 }
        self.last_time = current_milli_time()
        self.lap_times = []
        self.total_time = 0
        self.io.enable_inputs()
        self.arduino.gate_up()
        logging.info("'on_pre_game' ended")

    def gate_passed(self, gate):
        logging.info(f"gate: {gate}, lap: {self.lap}, next gate: {self.next_gate}")
        if (gate == self.next_gate):
            self.next_gate = self.gate_sequence[gate]

            if (gate == 0 and self.lap == 0):
                self.do_countdown()
                self.lap += 1

            elif (gate == 0 and self.lap <= TOTAL_LAPS):
                new_time = current_milli_time()
                lap_time = round((new_time - self.last_time) / 1000, 3)
                self.last_time = new_time

                if self.lap == 0:
                    self.arduino.gate_up()
                else:
                    self.total_time = round(self.total_time + lap_time, 3)
                    self.lap_times.append(lap_time)
                    self.io.send_score(score=lap_time)
                    self.io.send_lap()
                    logging.info(f"lap {self.lap}: {lap_time} (total: {self.total_time})")

                if self.lap == TOTAL_LAPS:
                    self.arduino.gate_down()
                    self.io.send_score(score=self.total_time, final_score=True)
                    self.io.send_playing_ended()
                    logging.info(f"game ended. final time: {self.total_time}. laps: {self.lap_times}")

                self.lap += 1
    
    def do_countdown(self):
        self.arduino.gate_down()
        self.arduino.serial.write("x0\ny0\n".encode('utf-8'))
        self.io.disable_inputs()
        time.sleep(3)
        logging.info("go!")
        self.last_time = current_milli_time()
        self.io.enable_inputs()
        self.arduino.gate_up()

    async def on_finish(self):
        logging.info("'on_finish' started")
        if self.lap < TOTAL_LAPS:
            self.io.send_score(score=300, final_score=True)

        self.io.disable_inputs()
        logging.info("'on_finish' ended")
        
    async def on_exit(self, reason, exception):
        self.arduino.stop()

def current_milli_time():
    return round(time.time() * 1000)

# And now you are ready to play!
if __name__ == "__main__":
    PinballRace().run()
    logging.info("done")
    
    
