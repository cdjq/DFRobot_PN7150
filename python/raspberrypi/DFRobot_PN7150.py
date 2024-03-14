# -*- coding: utf-8 -*
""" 
  @file  DFRobot_PN7150.py
  @note  DFRobot_PN7150 Class infrastructure, implementation of underlying methods
  @n This code reference: https://github.com/esmil/pn7150
  @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
  @licence  The MIT License (MIT)
  @author  [qsjhyy](yihuan.huang@dfrobot.com)
  @version  V1.0
  @date  2024-01-31
  @url  https://github.com/DFRobot/DFRobot_PN7150
"""
import sys
import time
import platform

from smbus2 import SMBus, i2c_msg

import logging
from ctypes import *


logger = logging.getLogger()
# logger.setLevel(logging.INFO)   # Display all print information
logger.setLevel(
    logging.FATAL
)  # If you don’t want to display too many prints, only print errors, please use this option
ph = logging.StreamHandler()
formatter = logging.Formatter(
    "%(asctime)s - [%(filename)s %(funcName)s]:%(lineno)d - %(levelname)s: %(message)s"
)
ph.setFormatter(formatter)
logger.addHandler(ph)


## i2c address
PN7150_I2C_ADDR = 0x28

_STATUS_OK = 0x00

PROT_UNDETERMINED = 0x0
PROT_T1T = 0x1
PROT_T2T = 0x2
PROT_T3T = 0x3
PROT_ISODEP = 0x4
PROT_NFCDEP = 0x5
PROT_T5T = 0x6
PROT_MIFARE = 0x80

_status = {
    # Status
    0x00: "OK",
    0x01: "REJECTED",
    0x02: "RF_FRAME_CORRUPTED",
    0x03: "FAILED",
    0x04: "NOT_INITIALIZED",
    0x05: "SYNTAX_ERROR",
    0x06: "SEMANTIC_ERROR",
    0x09: "INVALID_PARAM",
    0x0A: "MESSAGE_SIZE_EXCEEDED",
    # Discovery
    0xA0: "ALREADY_STARTED",
    0xA1: "TARGET_ACTIVATION_FAILED",
    0xA2: "TEAR_DOWN",
    # RF
    0xB0: "TRANSMISSION_ERROR",
    0xB1: "PROTOCOL_ERROR",
    0xB2: "TIMEOUT_ERROR",
}


def status(val: int):
    """!
    @brief Function for status
    @param val - status code
    """
    return _status.get(val, None) or "0x{:02x}".format(val)


# pylint: disable=too-many-branches
def dump_package(buf: bytes, end: int, prefix: str = ""):
    """!
    @brief Function dump_package.
    @param buf - Unparsed data
    @param end - Length
    @param prefix - Prefix code
    """
    fst, snd = buf[0], buf[1]
    if fst & 0xE0 == 0:
        print(
            "{}Data packet to/from {} length {}".format(prefix, buf[0] & 0x0F, buf[2])
        )
    elif fst == 0x20 and snd == 0x00:
        print(
            "{}CORE_RESET_CMD({}) Reset Configuration: {}".format(prefix, end, buf[3])
        )
    elif fst == 0x40 and snd == 0x00:
        # pylint: disable=line-too-long
        print(
            "{}CORE_RESET_RSP({}) Status: {} NCI Version: 0x{:02x} Configuration Status: 0x{:02x}".format(
                prefix, end, status(buf[3]), buf[4], buf[5]
            )
        )
    elif fst == 0x20 and snd == 0x01:
        print("{}CORE_INIT_CMD({})".format(prefix, end))
    elif fst == 0x40 and snd == 0x01:
        # 3    Status
        # 4    NFCC Features
        #      ..
        # 8    #RF Interfaces
        #      RF Interfaces
        # 9+n  Max Logical Connections
        # 10+n Max Routing Table
        #      ..
        # 12+n Max Control Packet Payload Size
        # 13+n Max Size for Large Parameters
        #      ..
        # 15+n Manufacturer ID
        # 16+n Manufacturer Specific Information
        n = buf[8]
        print(
            "{}CORE_INIT_RSP({}) Status: {} #RF Interfaces: {} Max Payload Size: {}".format(
                prefix, end, status(buf[3]), n, buf[12 + n]
            )
        )
    elif fst == 0x60 and snd == 0x06:
        print("{}CORE_CONN_CREDITS_NTF({}) #Entries: {}".format(prefix, end, buf[3]))
    elif fst == 0x60 and snd == 0x07:
        print(
            "{}CORE_GENERIC_ERROR_NTF({}) Status: {}".format(
                prefix, end, status(buf[3])
            )
        )
    elif fst == 0x60 and snd == 0x08:
        print(
            "{}CORE_INTERFACE_ERROR_NTF({}) Status: {} ConnID: {}".format(
                prefix, end, status(buf[3]), buf[4]
            )
        )
    elif fst == 0x21 and snd == 0x00:
        print(
            "{}RF_DISCOVER_MAP_CMD({}) #Mapping Configurations: {}".format(
                prefix, end, buf[3]
            )
        )
    elif fst == 0x41 and snd == 0x00:
        print(
            "{}RF_DISCOVER_MAP_RSP({}) Status: {}".format(prefix, end, status(buf[3]))
        )
    elif fst == 0x21 and snd == 0x03:
        print("{}RF_DISCOVER_CMD({}) #Configurations: {}".format(prefix, end, buf[3]))
    elif fst == 0x41 and snd == 0x03:
        print("{}RF_DISCOVER_RSP({}) Status: {}".format(prefix, end, status(buf[3])))
    elif fst == 0x21 and snd == 0x06:
        print("{}RF_DEACTIVATE_CMD({}) Mode: {}".format(prefix, end, buf[3]))
    elif fst == 0x41 and snd == 0x06:
        print("{}RF_DEACTIVATE_RSP({}) Status: {}".format(prefix, end, status(buf[3])))
    elif fst == 0x61 and snd == 0x06:
        print(
            "{}RF_DEACTIVATE_NTF({}) Type: {} Reason: {}".format(
                prefix, end, buf[3], buf[4]
            )
        )
    elif fst == 0x61 and snd == 0x05:
        # 3    RF Discovery ID
        # 4    RF Interface
        # 5    RF Protocol
        # 6    Activation RF Technology and Mode
        # 7    Max Data Packet Payload Size
        # 8    Initial Number of Credits
        # 9    #RF Technology Specific Parameters
        #      RF Technology Specific Parameters
        # 10+n Data Exchange RF Technology and Mode
        # 11+n Data Exchange Transmit Bit Rate
        # 12+n Data Exchange Receive Bit Rate
        # 13+n #Activation Parameters
        #      Activation Parameters
        print(
            "{}RF_INTF_ACTIVATED_NTF({}) ID: {} Interface: {} Protocol: {} Mode: 0x{:02x} #RFparams: {}".format(
                prefix, end, buf[3], buf[4], buf[5], buf[6], buf[9]
            )
        )
    elif fst == 0x2F and snd == 0x02:
        print("{}PROPRIETARY_ACT_CMD({})".format(prefix, end))
    elif fst == 0x4F and snd == 0x02:
        print(
            "{}PROPRIETARY_ACT_RSP({}) Status: {}".format(prefix, end, status(buf[3]))
        )
    else:
        print("{}{:02x}:{:02x} {} bytes".format(prefix, buf[0], buf[1], end))


# MT=1 GID=0 OID=0 PL=1 ResetType=1 (Reset Configuration)
NCI_CORE_RESET_CMD = b"\x20\x00\x01\x01"
# MT=1 GID=0 OID=1 PL=0
NCI_CORE_INIT_CMD = b"\x20\x01\x00"
# MT=1 GID=f OID=2 PL=0
NCI_PROP_ACT_CMD = b"\x2f\x02\x00"
# MT=1 GID=1 OID=0
NCI_RF_DISCOVER_MAP_RW = (
    b"\x21\x00\x10\x05\x01\x01\x01\x02\x01\x01\x03\x01\x01\x04\x01\x02\x80\x01\x80"
)
# MT=1 GID=1 OID=3
NCI_RF_DISCOVER_CMD_RW = b"\x21\x03\x09\x04\x00\x01\x02\x01\x01\x01\x06\x01"
# MODE_POLL | TECH_PASSIVE_NFCA,
# MODE_POLL | TECH_PASSIVE_NFCF,
# MODE_POLL | TECH_PASSIVE_NFCB,
# MODE_POLL | TECH_PASSIVE_15693,
NCI_RF_DEACTIVATE_CMD = b"\x21\x06\x01\x00"


class Card:
    """!
    @brief Class card.
    """

    def __init__(self, buf: bytearray, end: int):
        """!
        @brief Card structure init
        """
        self.card_id = buf[3]
        self.interface = buf[4]
        self.protocol = buf[5]
        self.modetech = buf[6]
        self.maxpayload = buf[7]
        self.credits = buf[8]
        self.nrfparams = buf[9]
        self.rest = buf[10:end]

    def nfcid1(self) -> str:
        """!
        @brief Function decode NFCID1 of rfts for NFC_A_PASSIVE_POLL_MODE
        """
        if self.modetech != 0x00:
            return None

        id_length = self.rest[2]

        return ":".join("{:02x}".format(x) for x in self.rest[3 : 3 + id_length])


class DFRobot_PN7150(object):
    """!
    @brief Define DFRobot_PN7150 basic class
    """

    def __init__(self, i2c_addr=PN7150_I2C_ADDR, bus=1):
        """!
        @brief Module I2C communication init
        @param i2c_addr - I2C communication address
        @param bus - I2C bus
        """
        if platform.node() == "unihiker":
            bus = 4
        self._addr = i2c_addr
        self._i2c = SMBus(bus)
        self._debug = False
        self._buf = bytearray(3 + 255)
        self.fw_version = self._buf[64]

    def _connect(self):
        """!
        @brief Function connect.
        """
        self._write_block(NCI_CORE_RESET_CMD)
        end = self._read_wait(15)
        if (
            end < 6
            or self._buf[0] != 0x40
            or self._buf[1] != 0x00
            or self._buf[3] != _STATUS_OK
            or self._buf[5] != 0x01
        ):
            return False
        self._write_block(NCI_CORE_INIT_CMD)
        end = self._read_wait()
        if (
            end < 20
            or self._buf[0] != 0x40
            or self._buf[1] != 0x01
            or self._buf[3] != _STATUS_OK
        ):
            return False

        nrf_int = self._buf[8]
        self.fw_version = self._buf[17 + nrf_int : 20 + nrf_int]
        # print("Firmware version: 0x{:02x} 0x{:02x} 0x{:02x}".format(
        #    self.fw_version[0], self.fw_version[1], self.fw_version[2]))

        self._write_block(NCI_PROP_ACT_CMD)
        end = self._read_wait()
        if (
            end < 4
            or self._buf[0] != 0x4F
            or self._buf[1] != 0x02
            or self._buf[3] != _STATUS_OK
        ):
            return False

        # print("FW_Build_Number:", self._buf[4:8])
        return True

    def connect(self):
        """!
        @brief Function connect.
        """
        try:
            ok = self._connect()
        finally:
            # print("finally connect")
            pass
        return ok

    def mode_rw(self):
        """!
        @brief Function mode Read/Write.
        """
        self._write_block(NCI_RF_DISCOVER_MAP_RW)
        end = self._read_wait(10)
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x00
            and self._buf[3] == _STATUS_OK
        )

    def start_discovery_rw(self):
        """!
        @brief Function Start Discovery Read/Write.
        """
        self._write_block(NCI_RF_DISCOVER_CMD_RW)
        end = self._read_wait()
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x03
            and self._buf[3] == _STATUS_OK
        )

    def stop_discovery(self):
        """!
        @brief Function stop Discovery.
        """
        self._write_block(NCI_RF_DEACTIVATE_CMD)
        end = self._read_wait()
        return (
            end >= 4
            and self._buf[0] == 0x41
            and self._buf[1] == 0x06
            and self._buf[3] == _STATUS_OK
        )

    def wait_for_card(self):
        """!
        @brief Function wait for Card.
        """
        while True:
            end = 0
            while end == 0:
                end = self._read_wait()
            if self._buf[0] == 0x61 and self._buf[1] == 0x05:
                break

        return Card(self._buf, end)

    def tag_cmd(self, cmd: bytes, conn_id: int = 0):
        """!
        @brief Function tag cmd.
        @param cmd - tag cmd
        @param conn_id - conn_id
        """
        self._buf[0] = conn_id
        self._buf[1] = 0x00
        self._buf[2] = len(cmd)
        end = 3 + len(cmd)
        self._buf[3:end] = cmd
        self._write_block(self._buf, end=end)

        base = time.time() * 100
        timeout = 5
        while (time.time() * 100 - base) < timeout:
            end = self._read_wait()
            if self._buf[0] & 0xE0 == 0x00:
                break
            time.sleep(0.001)

        return self._buf[3:end]

    def _read_wait(self, timeout: int = 5):
        """!
        @brief read the data from the register
        @param timeout - timeout
        @return read data
        """
        base = time.time() * 100
        while (time.time() * 100 - base) < timeout:
            count = self._read_block()
            if 3 < count:
                return count
            # print((time.time() * 100 - base))
            time.sleep(0.01)
        return 0

    def _read_block(self):
        """!
        @brief read the data from the register
        @return read data
        """
        end = 0
        try:
            read_msg = i2c_msg.read(self._addr, 3)
            self._i2c.i2c_rdwr(read_msg)
            self._buf[0:2] = list(read_msg)
            end = 3
            if self._buf[2] > 0:
                read_msg = i2c_msg.read(self._addr, self._buf[2])
                self._i2c.i2c_rdwr(read_msg)
                if len(list(read_msg)) == self._buf[2]:
                    self._buf[3:] = list(read_msg)
                    end = 3 + self._buf[2]
        except IOError as e:
            # print(f"I/O error: {e}")
            pass
        except Exception as e:
            print(f"An unexpected error occurred: {e}")
        if self._debug:
            dump_package(self._buf, end, prefix="< ")
        return end

    def _write_block(self, cmd, end: int = 0):
        """!
        @brief writes data to a register
        @param cmd - written data
        @param end - data len
        """
        self._read_block()
        if not end:
            end = len(cmd)
        if self._debug:
            dump_package(cmd, end, prefix="> ")
        try:
            write_msg = i2c_msg.write(self._addr, cmd[0:end])
            self._i2c.i2c_rdwr(write_msg)
        except IOError as e:
            # print(f"I/O error: {e}")
            self._write_block(cmd)
        except Exception as e:
            print(f"An unexpected error occurred: {e}")
