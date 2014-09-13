/** @file
  OHCI transfer scheduling routines.

Copyright(c) 2013 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**/

#include "Ohci.h"

/**
  Add an item of interrupt context

  @param  Ohc                   OHC private data
  @param  NewEntry              New entry to add

  @retval EFI_SUCCESS           Item successfully added
**/

EFI_STATUS
OhciAddInterruptContextEntry (
  IN  USB_OHCI_HC_DEV          *Ohc,
  IN  INTERRUPT_CONTEXT_ENTRY  *NewEntry
  )
{
  INTERRUPT_CONTEXT_ENTRY  *Entry;
  EFI_TPL                  OriginalTPL;

  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);
 
  if (Ohc->InterruptContextList == NULL) {
    Ohc->InterruptContextList = NewEntry;
  } else {
    Entry = Ohc->InterruptContextList;
    while (Entry->NextEntry != NULL) {
      Entry = Entry->NextEntry;
    }
    Entry->NextEntry = NewEntry;
  }

  gBS->RestoreTPL (OriginalTPL);

  return EFI_SUCCESS;
}

/**
  Free a interrupt context entry

  @param  Ohc                   OHC private data
  @param  Entry                 Pointer to an interrupt context entry

  @retval EFI_SUCCESS           Entry freed
  @retval EFI_INVALID_PARAMETER Entry is NULL
**/

EFI_STATUS
OhciFreeInterruptContextEntry (
  IN USB_OHCI_HC_DEV          *Ohc,
  IN INTERRUPT_CONTEXT_ENTRY  *Entry
  )
{
  TD_DESCRIPTOR           *Td;

  DEBUG ((EFI_D_INFO, "%a: enter for %p & %p\n", __FUNCTION__, Ohc, Entry));

  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (Entry->UCBufferMapping != NULL) {
    Ohc->PciIo->Unmap (Ohc->PciIo, Entry->UCBufferMapping);
  }
  if (Entry->UCBuffer != NULL) {
    FreePool (Entry->UCBuffer);
  }
  while (Entry->DataTd != NULL) {
    Td = Entry->DataTd;
    Entry->DataTd = (TD_DESCRIPTOR *)(UINTN) (Entry->DataTd->NextTDPointer);
    UsbHcFreeMem (Ohc->MemPool, Td, sizeof (TD_DESCRIPTOR));
  }  
  FreePool (Entry);
  DEBUG ((EFI_D_INFO, "%a: leave\n", __FUNCTION__));
  return EFI_SUCCESS;
}

/**
  Free entries match the device address and endpoint address

  @Param  Ohc                   OHC private date
  @Param  DeviceAddress         Item to free must match this device address
  @Param  EndPointAddress       Item to free must match this end point address
  @Param  DataToggle            DataToggle for output

  @retval  EFI_SUCCESS          Items match the requirement removed
**/

EFI_STATUS
OhciFreeInterruptContext (
  IN  USB_OHCI_HC_DEV     *Ohc,
  IN  UINT8               DeviceAddress,
  IN  UINT8               EndPointAddress,
  OUT UINT8               *DataToggle
  )
{
  INTERRUPT_CONTEXT_ENTRY  *Entry;
  INTERRUPT_CONTEXT_ENTRY  *TempEntry;
  EFI_TPL                  OriginalTPL;
  
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  while (Ohc->InterruptContextList != NULL &&
    Ohc->InterruptContextList->DeviceAddress == DeviceAddress &&
    Ohc->InterruptContextList->EndPointAddress == EndPointAddress) {
    TempEntry = Ohc->InterruptContextList;
    Ohc->InterruptContextList = Ohc->InterruptContextList->NextEntry;
    if (DataToggle != NULL) {
      *DataToggle = (UINT8) (TempEntry->DataTd->Word0.DataToggle & 0x1);
    }
    OhciFreeInterruptContextEntry (Ohc, TempEntry);
  }
  
  Entry = Ohc->InterruptContextList;
  if (Entry == NULL) {
    gBS->RestoreTPL (OriginalTPL);
    return EFI_SUCCESS;
  }
  while (Entry->NextEntry != NULL) {
    if (Entry->NextEntry->DeviceAddress == DeviceAddress && 
      Entry->NextEntry->EndPointAddress == EndPointAddress) {
      TempEntry = Entry->NextEntry;
      Entry->NextEntry = Entry->NextEntry->NextEntry;
      if (DataToggle != NULL) {
        *DataToggle = (UINT8) (TempEntry->DataTd->Word0.DataToggle & 0x1);
      }
      OhciFreeInterruptContextEntry (Ohc, TempEntry);
    } else {
      Entry = Entry->NextEntry;
    }     
  }
  
  gBS->RestoreTPL (OriginalTPL);
  
  return EFI_SUCCESS;
}

/**
  Convert Error code from OHCI format to EFI format

  @Param  ErrorCode             ErrorCode in OHCI format

  @retval                       ErrorCode in EFI format
**/

UINT32
ConvertErrorCode (
  IN  UINT32              ErrorCode
  )
{
  UINT32                  TransferResult;

  switch (ErrorCode) {
    case TD_NO_ERROR:
      TransferResult = EFI_USB_NOERROR;
      break;
    case TD_TOBE_PROCESSED:
    case TD_TOBE_PROCESSED_2:
      TransferResult = EFI_USB_ERR_NOTEXECUTE;
      break;
    case TD_DEVICE_STALL:
      TransferResult = EFI_USB_ERR_STALL;
      break;
    case TD_BUFFER_OVERRUN:
    case TD_BUFFER_UNDERRUN:
      TransferResult = EFI_USB_ERR_BUFFER;
      break;
    case TD_CRC_ERROR:
      TransferResult = EFI_USB_ERR_CRC;
      break;
    case TD_NO_RESPONSE:
      TransferResult = EFI_USB_ERR_TIMEOUT;
      break;
    case TD_BITSTUFFING_ERROR:
      TransferResult = EFI_USB_ERR_BITSTUFF;
      break;
    default:
      TransferResult = EFI_USB_ERR_SYSTEM;
  }

  return TransferResult;
}

/**
  Check TDs Results

  @Param  Ohc                   OHC private data
  @Param  Td                    TD_DESCRIPTOR
  @Param  Result                Result to return
 
  @retval TRUE                  means OK
  @retval FLASE                 means Error or Short packet
**/

BOOLEAN
OhciCheckTDsResults (
  IN  USB_OHCI_HC_DEV     *Ohc,
  IN  TD_DESCRIPTOR       *Td,
  OUT UINT32              *Result
  )
{
  UINT32                  TdCompletionCode;

  *Result   = EFI_USB_NOERROR;
  
  while (Td != NULL) {
    TdCompletionCode = Td->Word0.ConditionCode;

    *Result |= ConvertErrorCode (TdCompletionCode);

    // if any error encountered, stop processing the left TDs.

    if (*Result != EFI_USB_NOERROR) {
      return FALSE;
    }

    Td = (TD_DESCRIPTOR *)(UINTN) (Td->NextTDPointer);
  }
  return TRUE;
}

/**
  Check the task status on an ED

  @Param  Ed                    Pointer to the ED task that TD hooked on
  @Param  HeadTd                TD header for current transaction

  @retval                       Task Status Code
**/

UINT32
CheckEDStatus (
  IN  ED_DESCRIPTOR       *Ed,
  IN  TD_DESCRIPTOR       *HeadTd,
  OUT OHCI_ED_RESULT      *EdResult
  )
{
  while (HeadTd != NULL) {
    if (HeadTd->NextTDPointer == 0) {
      return TD_NO_ERROR;
    }
    if (HeadTd->Word0.ConditionCode != 0) {	
      return HeadTd->Word0.ConditionCode;
    }
    EdResult->NextToggle = ((UINT8) (HeadTd->Word0.DataToggle) & BIT0) ^ BIT0;
    HeadTd = (TD_DESCRIPTOR *)(UINTN) (HeadTd->NextTDPointer);
  }
  if (OhciGetEDField (Ed, ED_TDHEAD_PTR) != OhciGetEDField (Ed, ED_TDTAIL_PTR)) {
    return TD_TOBE_PROCESSED;
  }
  return TD_NO_ERROR;
}

/**
  Check the task status

  @Param  Ohc                   OHC private data
  @Param  ListType              Pipe type
  @Param  Ed                    Pointer to the ED task hooked on
  @Param  HeadTd                Head of TD corresponding to the task
  @Param  ErrorCode             return the ErrorCode

  @retval  EFI_SUCCESS          Task done
  @retval  EFI_NOT_READY        Task on processing
  @retval  EFI_DEVICE_ERROR     Some error occured
**/

EFI_STATUS
CheckIfDone (
  IN  USB_OHCI_HC_DEV       *Ohc,
  IN  DESCRIPTOR_LIST_TYPE  ListType,
  IN  ED_DESCRIPTOR         *Ed,
  IN  TD_DESCRIPTOR         *HeadTd,
  OUT OHCI_ED_RESULT        *EdResult
  )
{
  EdResult->ErrorCode = TD_TOBE_PROCESSED;
  
  switch (ListType) {
    case CONTROL_LIST:
      if (OhciGetHcCommandStatus (Ohc, CONTROL_LIST_FILLED) != 0) {
        return EFI_NOT_READY;
      }
      break;
    case BULK_LIST:
      if (OhciGetHcCommandStatus (Ohc, BULK_LIST_FILLED) != 0) {
        return EFI_NOT_READY;
      }
      break;
    default: 
      break;
  }

  EdResult->ErrorCode = CheckEDStatus (Ed, HeadTd, EdResult);

  if (EdResult->ErrorCode == TD_NO_ERROR) {
    return EFI_SUCCESS;
  } else if (EdResult->ErrorCode == TD_TOBE_PROCESSED) {
    return EFI_NOT_READY;
  } else {
    return EFI_DEVICE_ERROR;
  }
}

/**
  Convert TD condition code to Efi Status

  @Param  ConditionCode         Condition code to convert

  @retval  EFI_SUCCESS          No error occured
  @retval  EFI_NOT_READY        TD still on processing
  @retval  EFI_DEVICE_ERROR     Error occured in processing TD
**/

EFI_STATUS
OhciTDConditionCodeToStatus (
  IN  UINT32              ConditionCode
  )
{
  if (ConditionCode == TD_NO_ERROR) {
    return EFI_SUCCESS;
  }
  
  if (ConditionCode == TD_TOBE_PROCESSED) {
    return EFI_NOT_READY;
  }
  
  return EFI_DEVICE_ERROR;
}

/**
  Invoke callbacks hooked on done TDs

  @Param  Entry                 Interrupt transfer transaction information data structure
  @Param  Result                ???
**/

VOID
OhciInvokeInterruptCallBack (
  IN  INTERRUPT_CONTEXT_ENTRY  *Entry,
  IN  UINT32                   Result
)
{
  // Generally speaking, Keyboard driver should not 
  // check the Keyboard buffer if an error happens, it will be robust 
  // if we NULLed the buffer once error happens

  if (Result) {	
    Entry->CallBackFunction (
             NULL,
             0, 
             Entry->Context,
             Result
             );
  } else {
    Entry->CallBackFunction (
             (UINT8 *)(UINTN) (Entry->DataTd->DataBuffer), 
             Entry->DataTd->ActualSendLength, 
             Entry->Context,
             Result
             );
  }
}

/**
  Timer to submit periodic interrupt transfer, and invoke callbacks hooked on done TDs

  @param  Event                 Event handle
  @param  Context               Device private data
**/

VOID
EFIAPI
OhciHouseKeeper (
  IN  EFI_EVENT           Event,
  IN  VOID                *Context
  )
{

  USB_OHCI_HC_DEV          *Ohc; 
  INTERRUPT_CONTEXT_ENTRY  *Entry;
  INTERRUPT_CONTEXT_ENTRY  *PreEntry;
  ED_DESCRIPTOR            *Ed;
  TD_DESCRIPTOR            *DataTd;
  TD_DESCRIPTOR            *HeadTd;

  UINT8                    Toggle;
  EFI_TPL                  OriginalTPL;
  UINT32                   Result;
  
  Ohc = (USB_OHCI_HC_DEV *) Context;
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);
  
  Entry = Ohc->InterruptContextList;
  PreEntry = NULL;
  
  while (Entry != NULL) {
    OhciCheckTDsResults (Ohc, Entry->DataTd, &Result );
    if (((Result & EFI_USB_ERR_STALL) == EFI_USB_ERR_STALL) ||
      ((Result & EFI_USB_ERR_NOTEXECUTE) == EFI_USB_ERR_NOTEXECUTE)) {
      // Why return? We should continue to next entry in this case.
#if 0
      return;
#endif
      PreEntry = Entry;
      Entry = Entry->NextEntry;
      continue;
    } 

    if (Entry->CallBackFunction != NULL) {   
      OhciInvokeInterruptCallBack (Entry, Result);
    }
    if (Entry->IsPeriodic) {

      Ed = Entry->Ed;
      HeadTd = Entry->DataTd;
      DataTd = HeadTd;
      Toggle = 0;
      if (Result == EFI_USB_NOERROR) {

        // Update toggle if there is no error, and re-submit the interrupt Ed & Tds

        if (Ed != NULL && DataTd != NULL) {
          Ed->Word0.Skip = 1;
        }

        // From hcir1_0a.pdf 4.2.2 
        // ToggleCarry: This bit is the data toggle carry bit,
        // Whenever a TD is retired, this bit is written to 
        // contain the last data toggle value (LSb of data Toggel
        // file) from the retired TD.
        // This field is not used for Isochronous Endpoints

        if (Ed == NULL) {
          return;
        }
        Toggle = (UINT8) OhciGetEDField (Ed, ED_DTTOGGLE);
        while (DataTd != NULL) {
          if (DataTd->NextTDPointer == 0) {  
            DataTd->Word0.DataToggle = 0;
            break;
          } else {    
            OhciSetTDField (DataTd, TD_DT_TOGGLE, Toggle);
          }
          DataTd = (TD_DESCRIPTOR *)(UINTN)(DataTd->NextTDPointer);
          Toggle ^= 1;
        }

        // HC will only update DataToggle, ErrorCount, ConditionCode
        // CurrentBufferPointer & NextTD, so we only need to update
        // them once we want to active them again

        DataTd = HeadTd;
        while (DataTd != NULL) {
          if (DataTd->NextTDPointer == 0) {  
            OhciSetTDField (DataTd, TD_ERROR_CNT | TD_COND_CODE | TD_CURR_BUFFER_PTR | TD_NEXT_PTR, 0);	  
            break;
          }
          OhciSetTDField (DataTd, TD_ERROR_CNT, 0);
          OhciSetTDField (DataTd, TD_COND_CODE, TD_TOBE_PROCESSED);
          DataTd->NextTD = DataTd->NextTDPointer;
          DataTd->CurrBufferPointer = DataTd->DataBuffer;
          DataTd = (TD_DESCRIPTOR *)(UINTN)(DataTd->NextTDPointer);
        }

        // Active current Ed, Td 
        //
        // HC will only update Halted, ToggleCarry & TDQueueHeadPointer,
        // So we only need to update them once we want to active them again.

        if (Ed != NULL && DataTd != NULL) {
          Ed->Word2.TdHeadPointer = (UINT32)((UINTN) HeadTd >> 4);
          OhciSetEDField (Ed, ED_HALTED | ED_DTTOGGLE, 0);
          Ed->Word0.Skip = 0;
        }
      }
    } else {            
      if (PreEntry == NULL) {
        Ohc->InterruptContextList = Entry->NextEntry;
      } else {
        PreEntry = Entry;
        PreEntry->NextEntry = Entry->NextEntry;
      }
      OhciFreeInterruptContextEntry (Ohc, PreEntry);
      gBS->RestoreTPL (OriginalTPL);
      return ;
    }
    PreEntry = Entry;
    Entry = Entry->NextEntry;
  }
  gBS->RestoreTPL (OriginalTPL);
  return;
}
