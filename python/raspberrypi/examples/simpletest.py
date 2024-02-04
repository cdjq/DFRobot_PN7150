# -*- coding: utf-8 -*
""" 
  @file  i2c.py
  @brief  Control the voice recognition module via I2C
  @n  Get the recognized command ID and play the corresponding reply audio according to the ID;
  @n  Get and set the wake-up state duration, set mute mode, set volume, and enter the wake-up state
  @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
  @licence  The MIT License (MIT)
  @author  [qsjhyy](yihuan.huang@dfrobot.com)
  @version  V1.0
  @date  2022-12-30
  @url https://github.com/DFRobot/DFRobot_PN7150
"""
from __future__ import print_function
import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

from DFRobot_PN7150 import *
from time import sleep

"""
  @brief Module I2C communication init
  @param i2c_addr - I2C communication address
  @param bus - I2C bus
"""
PN7150 = DFRobot_PN7150(i2c_addr=PN7150_I2C_ADDR, bus=1)


def setup():
    assert PN7150.connect()
    print("Connected.")

    assert PN7150.mode_rw()
    print("Switched to read/write mode.")


def loop():
    assert PN7150.start_discovery_rw()

    print("Waiting for card..")
    card = PN7150.wait_for_card()

    # Read sectors starting at 4 reading 4 sectors at a time
    for i in range(4, 24, 4):
        sector = PN7150.tag_cmd(b"\x30" + i.to_bytes(1, "big"))
        print(
            "Sector {0:02d}:".format(i),
            ":".join("{:02x}".format(x) for x in sector),
            "".join([chr(x) if 128 > x > 31 else "." for x in sector]),
        )

    assert PN7150.stop_discovery()

    print("ID: {}".format(card.nfcid1()))

    sleep(0.5)


if __name__ == "__main__":
    setup()
    while True:
        loop()
