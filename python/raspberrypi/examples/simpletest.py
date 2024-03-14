# -*- coding: utf-8 -*
""" 
  @file  simpletest.py
  @brief  Read the basic information of the card and read and write the card memory
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
PN7150 = DFRobot_PN7150()


def setup():
    assert PN7150.connect()
    print("Connected.")

    assert PN7150.mode_rw()
    print("Switched to read/write mode.")


def loop():
    assert PN7150.start_discovery_rw()

    print("\nWaiting for card..")
    card = PN7150.wait_for_card()

    if (
        card.protocol == PROT_T1T
        or card.protocol == PROT_T2T
        or card.protocol == PROT_T3T
        or card.protocol == PROT_ISODEP
    ):
        print(" - POLL MODE: Remote T{}T card activated".format(card.protocol))
    elif card.protocol == PROT_T5T:
        print(" - POLL MODE: Remote T5T card activated")
    elif card.protocol == PROT_MIFARE:
        print(" - POLL MODE: Remote MIFARE card activated")
    else:
        print(" - POLL MODE: Undetermined target")
    print("ID: {}".format(card.nfcid1()))

    # PCD_ISO14443_3A
    if card.protocol == PROT_T2T:
        # Read sectors starting at 4 reading 4 sectors at a time
        print("Read block ...")
        for i in range(4, 24, 4):
            sector = PN7150.tag_cmd(b"\x30" + i.to_bytes(1, "big"))
            print(
                "Sector {0:02d}:".format(i),
                ":".join("{:02x}".format(x) for x in sector),
                "".join([chr(x) if 128 > x > 31 else "." for x in sector]),
            )
        sleep(1)
        # Write sectors
        print("Write and read block ...")
        for i in range(4, 24, 4):
            sector = PN7150.tag_cmd(
                b"\xA2" + i.to_bytes(1, "big") + b"\x11\x22\x33\x44"
            )
        # Read sectors starting at 4 reading 4 sectors at a time
        sleep(1)
        for i in range(4, 24, 4):
            sector = PN7150.tag_cmd(b"\x30" + i.to_bytes(1, "big"))
            print(
                "Sector {0:02d}:".format(i),
                ":".join("{:02x}".format(x) for x in sector),
                "".join([chr(x) if 128 > x > 31 else "." for x in sector]),
            )

    assert PN7150.stop_discovery()
    sleep(3)


if __name__ == "__main__":
    setup()
    while True:
        loop()
