/** @file

  The definition for OHCI register operation routines.

Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _EFI_OHCI_REG_H_
#define _EFI_OHCI_REG_H_

//
// OHCI register offset
//

#define OHCI_FRAME_NUM        1024

//
// Register offset and PCI related staff
//
#define USB_BAR_INDEX         4

#define USBCMD_OFFSET         0
#define USBSTS_OFFSET         2
#define USBINTR_OFFSET        4
#define USBPORTSC_OFFSET      0x10
#define USB_FRAME_NO_OFFSET   6
#define USB_FRAME_BASE_OFFSET 8
#define USB_EMULATION_OFFSET  0xC0

//
// Packet IDs
//
#define SETUP_PACKET_ID       0x2D
#define INPUT_PACKET_ID       0x69
#define OUTPUT_PACKET_ID      0xE1
#define ERROR_PACKET_ID       0x55

//
// USB port status and control bit definition.
//
#define USBPORTSC_CCS         BIT0  // Current Connect Status
#define USBPORTSC_CSC         BIT1  // Connect Status Change
#define USBPORTSC_PED         BIT2  // Port Enable / Disable
#define USBPORTSC_PEDC        BIT3  // Port Enable / Disable Change
#define USBPORTSC_LSL         BIT4  // Line Status Low BIT
#define USBPORTSC_LSH         BIT5  // Line Status High BIT
#define USBPORTSC_RD          BIT6  // Resume Detect
#define USBPORTSC_LSDA        BIT8  // Low Speed Device Attached
#define USBPORTSC_PR          BIT9  // Port Reset
#define USBPORTSC_SUSP        BIT12 // Suspend

//
// OHCI Spec said it must implement 2 ports each host at least,
// and if more, check whether the bit7 of PORTSC is always 1.
// So here assume the max of port number each host is 16.
//
#define USB_MAX_ROOTHUB_PORT  0x0F

//
// Command register bit definitions
//
#define USBCMD_RS             BIT0  // Run/Stop
#define USBCMD_HCRESET        BIT1  // Host reset
#define USBCMD_GRESET         BIT2  // Global reset
#define USBCMD_EGSM           BIT3  // Global Suspend Mode
#define USBCMD_FGR            BIT4  // Force Global Resume
#define USBCMD_SWDBG          BIT5  // SW Debug mode
#define USBCMD_CF             BIT6  // Config Flag (sw only)
#define USBCMD_MAXP           BIT7  // Max Packet (0 = 32, 1 = 64)

//
// USB Status register bit definitions
//
#define USBSTS_USBINT         BIT0  // Interrupt due to IOC
#define USBSTS_ERROR          BIT1  // Interrupt due to error
#define USBSTS_RD             BIT2  // Resume Detect
#define USBSTS_HSE            BIT3  // Host System Error
#define USBSTS_HCPE           BIT4  // Host Controller Process Error
#define USBSTS_HCH            BIT5  // HC Halted

#define USBTD_ACTIVE          BIT7  // TD is still active
#define USBTD_STALLED         BIT6  // TD is stalled
#define USBTD_BUFFERR         BIT5  // Buffer underflow or overflow
#define USBTD_BABBLE          BIT4  // Babble condition
#define USBTD_NAK             BIT3  // NAK is received
#define USBTD_CRC             BIT2  // CRC/Time out error
#define USBTD_BITSTUFF        BIT1  // Bit stuff error


/**
  Read a OHCI register.

  @param  PciIo        The EFI_PCI_IO_PROTOCOL to use.
  @param  Offset       Register offset to USB_BAR_INDEX.

  @return Content of register.

**/
UINT16
OhciReadReg (
  IN EFI_PCI_IO_PROTOCOL     *PciIo,
  IN UINT32                  Offset
  );



/**
  Write data to OHCI register.

  @param  PciIo        The EFI_PCI_IO_PROTOCOL to use.
  @param  Offset       Register offset to USB_BAR_INDEX.
  @param  Data         Data to write.

  @return None.

**/
VOID
OhciWriteReg (
  IN EFI_PCI_IO_PROTOCOL     *PciIo,
  IN UINT32                  Offset,
  IN UINT16                  Data
  );



/**
  Set a bit of the OHCI Register.

  @param  PciIo        The EFI_PCI_IO_PROTOCOL to use.
  @param  Offset       Register offset to USB_BAR_INDEX.
  @param  Bit          The bit to set.

  @return None.

**/
VOID
OhciSetRegBit (
  IN EFI_PCI_IO_PROTOCOL     *PciIo,
  IN UINT32                  Offset,
  IN UINT16                  Bit
  );



/**
  Clear a bit of the OHCI Register.

  @param  PciIo        The PCI_IO protocol to access the PCI.
  @param  Offset       Register offset to USB_BAR_INDEX.
  @param  Bit          The bit to clear.

  @return None.

**/
VOID
OhciClearRegBit (
  IN EFI_PCI_IO_PROTOCOL     *PciIo,
  IN UINT32                  Offset,
  IN UINT16                  Bit
  );


/**
  Clear all the interrutp status bits, these bits
  are Write-Clean.

  @param  Ohc          The OHCI device.

  @return None.

**/
VOID
OhciAckAllInterrupt (
  IN  USB_HC_DEV          *Ohc
  );


/**
  Stop the host controller.

  @param  Ohc          The OHCI device.
  @param  Timeout      Max time allowed.

  @retval EFI_SUCCESS  The host controller is stopped.
  @retval EFI_TIMEOUT  Failed to stop the host controller.

**/
EFI_STATUS
OhciStopHc (
  IN USB_HC_DEV         *Ohc,
  IN UINTN              Timeout
  );



/**
  Check whether the host controller operates well.

  @param  PciIo        The PCI_IO protocol to use.

  @retval TRUE         Host controller is working.
  @retval FALSE        Host controller is halted or system error.

**/
BOOLEAN
OhciIsHcWorking (
  IN EFI_PCI_IO_PROTOCOL     *PciIo
  );


/**
  Set the OHCI frame list base address. It can't use
  OhciWriteReg which access memory in UINT16.

  @param  PciIo        The EFI_PCI_IO_PROTOCOL to use.
  @param  Addr         Address to set.

  @return None.

**/
VOID
OhciSetFrameListBaseAddr (
  IN EFI_PCI_IO_PROTOCOL     *PciIo,
  IN VOID                    *Addr
  );


/**
  Disable USB Emulation.

  @param  PciIo        The EFI_PCI_IO_PROTOCOL protocol to use.

  @return None.

**/
VOID
OhciTurnOffUsbEmulation (
  IN EFI_PCI_IO_PROTOCOL     *PciIo
  );
#endif
