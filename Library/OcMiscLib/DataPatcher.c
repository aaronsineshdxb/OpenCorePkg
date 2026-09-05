/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseOverflowLib.h>
#include <Library/DebugLib.h>
#include <Library/OcMiscLib.h>

STATIC
BOOLEAN
InternalFindPatternBmh (
  IN CONST UINT8   *Pattern,
  IN UINT32        PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN OUT UINT32    *DataOff
  )
{
  UINT32  BadCharShift[256];
  UINT32  CurrentOffset;
  UINT32  LastOffset;
  UINT32  Index;

  ASSERT (DataSize >= PatternSize);
  ASSERT (DataOff != NULL);

  if (PatternSize == 0) {
    return FALSE;
  }

  //
  // Horspool's shift table skips windows that cannot contain the pattern.
  // Keep the last pattern byte out of the table, as its default shift is
  // PatternSize and its match is handled by the comparison below.
  //
  for (Index = 0; Index < ARRAY_SIZE (BadCharShift); ++Index) {
    BadCharShift[Index] = PatternSize;
  }

  for (Index = 0; Index + 1 < PatternSize; ++Index) {
    BadCharShift[Pattern[Index]] = PatternSize - Index - 1;
  }

  CurrentOffset = *DataOff;
  LastOffset    = DataSize - PatternSize;

  while (CurrentOffset <= LastOffset) {
    Index = PatternSize;
    while ((Index > 0) && (Data[CurrentOffset + Index - 1] == Pattern[Index - 1])) {
      --Index;
    }

    if (Index == 0) {
      *DataOff = CurrentOffset;
      return TRUE;
    }

    CurrentOffset += BadCharShift[Data[CurrentOffset + PatternSize - 1]];
  }

  return FALSE;
}

STATIC
BOOLEAN
InternalFindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN OUT UINT32    *DataOff
  )
{
  UINT32  Index;
  UINT32  LastOffset;
  UINT32  CurrentOffset;

  ASSERT (DataSize >= PatternSize);
  ASSERT (DataOff != NULL);

  if (PatternSize == 0) {
    return FALSE;
  }

  // Masked patterns do not have a safe ordinary bad-character shift table.
  // Retain the original linear matcher for that case.
  if (PatternMask == NULL) {
    return InternalFindPatternBmh (Pattern, PatternSize, Data, DataSize, DataOff);
  }

  CurrentOffset = *DataOff;
  LastOffset    = DataSize - PatternSize;

  while (CurrentOffset <= LastOffset) {
    for (Index = 0; Index < PatternSize; ++Index) {
      if ((Data[CurrentOffset + Index] & PatternMask[Index]) != Pattern[Index]) {
        break;
      }
    }

    if (Index == PatternSize) {
      *DataOff = CurrentOffset;
      return TRUE;
    }

    ++CurrentOffset;
  }

  return FALSE;
}

BOOLEAN
FindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN OUT UINT32    *DataOff
  )
{
  if (DataSize < PatternSize) {
    return FALSE;
  }

  return InternalFindPattern (
           Pattern,
           PatternMask,
           PatternSize,
           Data,
           DataSize,
           DataOff
           );
}

UINT32
ApplyPatch (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Replace,
  IN CONST UINT8   *ReplaceMask OPTIONAL,
  IN UINT8         *Data,
  IN UINT32        DataSize,
  IN UINT32        Count,
  IN UINT32        Skip
  )
{
  UINT32   ReplaceCount;
  UINT32   DataOff;
  BOOLEAN  Found;

  if (DataSize < PatternSize) {
    return 0;
  }

  ReplaceCount = 0;
  DataOff      = 0;

  while (TRUE) {
    Found = InternalFindPattern (
              Pattern,
              PatternMask,
              PatternSize,
              Data,
              DataSize,
              &DataOff
              );

    if (!Found) {
      break;
    }

    //
    // DataOff + PatternSize - 1 is guaranteed to be a valid offset here. As
    // DataSize can at most be MAX_UINT32, the maximum valid offset is
    // MAX_UINT32 - 1. In consequence, DataOff + PatternSize cannot wrap around.
    //

    //
    // Skip this finding if requested.
    //
    if (Skip > 0) {
      --Skip;
      DataOff += PatternSize;
      continue;
    }

    //
    // Perform replacement.
    //
    if (ReplaceMask == NULL) {
      CopyMem (&Data[DataOff], Replace, PatternSize);
    } else {
      for (UINTN Index = 0; Index < PatternSize; ++Index) {
        Data[DataOff + Index] = (Data[DataOff + Index] & ~ReplaceMask[Index]) | (Replace[Index] & ReplaceMask[Index]);
      }
    }

    ++ReplaceCount;
    DataOff += PatternSize;

    //
    // Check replace count if requested.
    //
    if (Count > 0) {
      --Count;
      if (Count == 0) {
        break;
      }
    }
  }

  return ReplaceCount;
}
