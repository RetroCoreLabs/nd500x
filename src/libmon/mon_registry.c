/*
 * MON Handler Registry
 *
 * AUTO-GENERATED - DO NOT EDIT
 *
 * This file registers all MON call handlers.
 * Run tools/generate_mon_handlers.py to regenerate.
 */

#include "mon.h"

/* Forward declarations for all handlers */
extern MonResult mon_0B_ExitFromProgram(MonContext* ctx);
extern MonResult mon_100B_StartRTProgram(MonContext* ctx);
extern MonResult mon_101B_DelayStart(MonContext* ctx);
extern MonResult mon_102B_StartupTime(MonContext* ctx);
extern MonResult mon_103B_StartupInterval(MonContext* ctx);
extern MonResult mon_104B_SuspendProgram(MonContext* ctx);
extern MonResult mon_105B_StopRTProgram(MonContext* ctx);
extern MonResult mon_106B_StartOnInterrupt(MonContext* ctx);
extern MonResult mon_107B_NoInterruptStart(MonContext* ctx);
extern MonResult mon_10B_WriteBlock(MonContext* ctx);
extern MonResult mon_110B_SetRTPriority(MonContext* ctx);
extern MonResult mon_111B_SetClock(MonContext* ctx);
extern MonResult mon_112B_AdjustClock(MonContext* ctx);
extern MonResult mon_113B_GetCurrentTime(MonContext* ctx);
extern MonResult mon_114B_GetTimeUsed(MonContext* ctx);
extern MonResult mon_115B_FixScattered(MonContext* ctx);
extern MonResult mon_116B_UnfixSegment(MonContext* ctx);
extern MonResult mon_117B_ReadFromFile(MonContext* ctx);
extern MonResult mon_11B_GetBasicTime(MonContext* ctx);
extern MonResult mon_120B_WriteToFile(MonContext* ctx);
extern MonResult mon_121B_AwaitFileTransfer(MonContext* ctx);
extern MonResult mon_122B_ReserveResource(MonContext* ctx);
extern MonResult mon_123B_ReleaseResource(MonContext* ctx);
extern MonResult mon_124B_ForceReserve(MonContext* ctx);
extern MonResult mon_125B_ForceRelease(MonContext* ctx);
extern MonResult mon_126B_ExactDelayStart(MonContext* ctx);
extern MonResult mon_127B_ExactStartup(MonContext* ctx);
extern MonResult mon_12B_SetCommandBuffer(MonContext* ctx);
extern MonResult mon_130B_ExactInterval(MonContext* ctx);
extern MonResult mon_131B_DataTransfer(MonContext* ctx);
extern MonResult mon_132B_JumpToSegment(MonContext* ctx);
extern MonResult mon_133B_ExitFromSegment(MonContext* ctx);
extern MonResult mon_134B_ExitRTProgram(MonContext* ctx);
extern MonResult mon_135B_WaitForRestart(MonContext* ctx);
extern MonResult mon_136B_EnableRTStart(MonContext* ctx);
extern MonResult mon_137B_DisableRTStart(MonContext* ctx);
extern MonResult mon_13B_ClearInBuffer(MonContext* ctx);
extern MonResult mon_140B_ReservationInfo(MonContext* ctx);
extern MonResult mon_141B_DeviceControl(MonContext* ctx);
extern MonResult mon_142B_ToErrorDevice(MonContext* ctx);
extern MonResult mon_143B_ExecutionInfo(MonContext* ctx);
extern MonResult mon_144B_DeviceFunction(MonContext* ctx);
extern MonResult mon_146B_PrivInstruction(MonContext* ctx);
extern MonResult mon_147B_CAMACFunction(MonContext* ctx);
extern MonResult mon_14B_ClearOutBuffer(MonContext* ctx);
extern MonResult mon_150B_CAMACGLRegister(MonContext* ctx);
extern MonResult mon_151B_GetRTAddress(MonContext* ctx);
extern MonResult mon_152B_GetRTName(MonContext* ctx);
extern MonResult mon_153B_CAMACIOInstruction(MonContext* ctx);
extern MonResult mon_154B_AssignCAMACLAM(MonContext* ctx);
extern MonResult mon_155B_GraphicFunction(MonContext* ctx);
extern MonResult mon_157B_SegmentToPageTable(MonContext* ctx);
extern MonResult mon_160B_FixContiguous(MonContext* ctx);
extern MonResult mon_161B_InString(MonContext* ctx);
extern MonResult mon_162B_OutString(MonContext* ctx);
extern MonResult mon_164B_SaveSegment(MonContext* ctx);
extern MonResult mon_165B_GetInRegisters(MonContext* ctx);
extern MonResult mon_167B_AttachSegment(MonContext* ctx);
extern MonResult mon_16B_GetTerminalType(MonContext* ctx);
extern MonResult mon_170B_UserDef0(MonContext* ctx);
extern MonResult mon_171B_UserDef1(MonContext* ctx);
extern MonResult mon_172B_UserDef2(MonContext* ctx);
extern MonResult mon_173B_UserDef3(MonContext* ctx);
extern MonResult mon_174B_UserDef4(MonContext* ctx);
extern MonResult mon_175B_UserDef5(MonContext* ctx);
extern MonResult mon_176B_UserDef6(MonContext* ctx);
extern MonResult mon_177B_UserDef7(MonContext* ctx);
extern MonResult mon_17B_SetTerminalType(MonContext* ctx);
extern MonResult mon_1B_InByte(MonContext* ctx);
extern MonResult mon_200B_XMSGFunction(MonContext* ctx);
extern MonResult mon_201B_HDLCfunction(MonContext* ctx);
extern MonResult mon_206B_TerminationHandling(MonContext* ctx);
extern MonResult mon_207B_GetErrorInfo(MonContext* ctx);
extern MonResult mon_212B_ReentrantSegment(MonContext* ctx);
extern MonResult mon_213B_GetDirUserIndexes(MonContext* ctx);
extern MonResult mon_214B_GetUserName(MonContext* ctx);
extern MonResult mon_215B_GetObjectEntry(MonContext* ctx);
extern MonResult mon_216B_SetObjectEntry(MonContext* ctx);
extern MonResult mon_217B_GetAllFileIndexes(MonContext* ctx);
extern MonResult mon_21B_InUpTo8Bytes(MonContext* ctx);
extern MonResult mon_220B_DirectOpen(MonContext* ctx);
extern MonResult mon_221B_CreateFile(MonContext* ctx);
extern MonResult mon_222B_GetAddressArea(MonContext* ctx);
extern MonResult mon_227B_SetEscLocalChars(MonContext* ctx);
extern MonResult mon_22B_OutUpTo8Bytes(MonContext* ctx);
extern MonResult mon_230B_GetEscLocalChars(MonContext* ctx);
extern MonResult mon_231B_ExpandFile(MonContext* ctx);
extern MonResult mon_232B_RenameFile(MonContext* ctx);
extern MonResult mon_233B_SetTemporaryFile(MonContext* ctx);
extern MonResult mon_234B_SetPeripheralName(MonContext* ctx);
extern MonResult mon_235B_ScratchOpen(MonContext* ctx);
extern MonResult mon_236B_SetPermanentOpen(MonContext* ctx);
extern MonResult mon_237B_SetFileAccess(MonContext* ctx);
extern MonResult mon_23B_In8Bytes(MonContext* ctx);
extern MonResult mon_240B_AppendSpooling(MonContext* ctx);
extern MonResult mon_241B_NewUser(MonContext* ctx);
extern MonResult mon_242B_OldUser(MonContext* ctx);
extern MonResult mon_243B_GetDirNameIndex(MonContext* ctx);
extern MonResult mon_244B_GetDirEntry(MonContext* ctx);
extern MonResult mon_245B_GetNameEntry(MonContext* ctx);
extern MonResult mon_246B_ReserveDir(MonContext* ctx);
extern MonResult mon_247B_ReleaseDir(MonContext* ctx);
extern MonResult mon_24B_Out8Bytes(MonContext* ctx);
extern MonResult mon_250B_GetDefaultDir(MonContext* ctx);
extern MonResult mon_251B_CopyPage(MonContext* ctx);
extern MonResult mon_252B_BackupClose(MonContext* ctx);
extern MonResult mon_253B_NewFileVersion(MonContext* ctx);
extern MonResult mon_254B_GetErrorDevice(MonContext* ctx);
extern MonResult mon_255B_PIOCFunction(MonContext* ctx);
extern MonResult mon_256B_FullFileName(MonContext* ctx);
extern MonResult mon_257B_OpenFileInfo(MonContext* ctx);
extern MonResult mon_262B_GetSystemInfo(MonContext* ctx);
extern MonResult mon_263B_GetDeviceType(MonContext* ctx);
extern MonResult mon_267B_TimeOut(MonContext* ctx);
extern MonResult mon_26B_GetLastByte(MonContext* ctx);
extern MonResult mon_270B_ReadDiskPage(MonContext* ctx);
extern MonResult mon_271B_WriteDiskPage(MonContext* ctx);
extern MonResult mon_272B_DeletePage(MonContext* ctx);
extern MonResult mon_273B_GetFileName(MonContext* ctx);
extern MonResult mon_274B_GetFileIndexes(MonContext* ctx);
extern MonResult mon_275B_SetTerminalName(MonContext* ctx);
extern MonResult mon_276B_EnableLocal(MonContext* ctx);
extern MonResult mon_277B_DisableLocal(MonContext* ctx);
extern MonResult mon_27B_GetRTDescr(MonContext* ctx);
extern MonResult mon_2B_OutByte(MonContext* ctx);
extern MonResult mon_300B_SetEscapeHandling(MonContext* ctx);
extern MonResult mon_301B_StopEscapeHandling(MonContext* ctx);
extern MonResult mon_302B_OnEscLocalFunction(MonContext* ctx);
extern MonResult mon_303B_OffEscLocalFunction(MonContext* ctx);
extern MonResult mon_306B_GetTerminalMode(MonContext* ctx);
extern MonResult mon_307B_TerminalNoWait(MonContext* ctx);
extern MonResult mon_30B_GetOwnRTAddress(MonContext* ctx);
extern MonResult mon_310B_In8AndFlag(MonContext* ctx);
extern MonResult mon_311B_WriteDirEntry(MonContext* ctx);
extern MonResult mon_312B_CheckMonCall(MonContext* ctx);
extern MonResult mon_313B_InBufferState(MonContext* ctx);
extern MonResult mon_314B_DefaultRemoteSystem(MonContext* ctx);
extern MonResult mon_315B_LAMUFunction(MonContext* ctx);
extern MonResult mon_316B_SetRemoteAccess(MonContext* ctx);
extern MonResult mon_317B_ExecuteCommand(MonContext* ctx);
extern MonResult mon_31B_IOInstruction(MonContext* ctx);
extern MonResult mon_322B_GetSegmentNo(MonContext* ctx);
extern MonResult mon_323B_SegmentOverlay(MonContext* ctx);
extern MonResult mon_324B_OctobusFunction(MonContext* ctx);
extern MonResult mon_325B_BatchModeEcho(MonContext* ctx);
extern MonResult mon_326B_LogInStart(MonContext* ctx);
extern MonResult mon_32B_OutMessage(MonContext* ctx);
extern MonResult mon_330B_TerminalStatus(MonContext* ctx);
extern MonResult mon_332B_TerminalLineInfo(MonContext* ctx);
extern MonResult mon_333B_DMAFunction(MonContext* ctx);
extern MonResult mon_334B_GetErrorMessage(MonContext* ctx);
extern MonResult mon_335B_TransferData(MonContext* ctx);
extern MonResult mon_336B_Terminal(MonContext* ctx);
extern MonResult mon_337B_ChangeSegment(MonContext* ctx);
extern MonResult mon_33B_AltPageTable(MonContext* ctx);
extern MonResult mon_340B_ReadSystemRecord(MonContext* ctx);
extern MonResult mon_341B_SegmentFunction(MonContext* ctx);
extern MonResult mon_34B_NormalPageTable(MonContext* ctx);
extern MonResult mon_35B_OutNumber(MonContext* ctx);
extern MonResult mon_36B_NoWaitSwitch(MonContext* ctx);
extern MonResult mon_37B_ReadADChannel(MonContext* ctx);
extern MonResult mon_3B_SetEcho(MonContext* ctx);
extern MonResult mon_400B_ErrorReturn(MonContext* ctx);
extern MonResult mon_401B_DisAssemble(MonContext* ctx);
extern MonResult mon_402B_GetInputFlags(MonContext* ctx);
extern MonResult mon_403B_SetOutputFlags(MonContext* ctx);
extern MonResult mon_404B_FixIOArea(MonContext* ctx);
extern MonResult mon_405B_SwitchUserBreak(MonContext* ctx);
extern MonResult mon_406B_AccessRTCommon(MonContext* ctx);
extern MonResult mon_40B_CloseSpoolingFile(MonContext* ctx);
extern MonResult mon_410B_FixInMemory(MonContext* ctx);
extern MonResult mon_411B_MemoryUnfix(MonContext* ctx);
extern MonResult mon_412B_FileAsSegment(MonContext* ctx);
extern MonResult mon_413B_FileNotAsSegment(MonContext* ctx);
extern MonResult mon_414B_BCNAFCAMAC(MonContext* ctx);
extern MonResult mon_415B_BCNAF1CAMAC(MonContext* ctx);
extern MonResult mon_416B_SaveND500Segment(MonContext* ctx);
extern MonResult mon_417B_MaxPagesInMemory(MonContext* ctx);
extern MonResult mon_41B_ReadObjectEntry(MonContext* ctx);
extern MonResult mon_420B_GetUserRegisters(MonContext* ctx);
extern MonResult mon_421B_GetActiveSegment(MonContext* ctx);
extern MonResult mon_422B_GetScratchSegment(MonContext* ctx);
extern MonResult mon_423B_CopyCapability(MonContext* ctx);
extern MonResult mon_424B_ClearCapability(MonContext* ctx);
extern MonResult mon_425B_SetProcessName(MonContext* ctx);
extern MonResult mon_426B_GetProcessNo(MonContext* ctx);
extern MonResult mon_427B_GetOwnProcessInfo(MonContext* ctx);
extern MonResult mon_430B_TranslateAddress(MonContext* ctx);
extern MonResult mon_431B_AwaitTransfer(MonContext* ctx);
extern MonResult mon_435B_ForceTrap(MonContext* ctx);
extern MonResult mon_436B_SetND500Param(MonContext* ctx);
extern MonResult mon_437B_GetND500Param(MonContext* ctx);
extern MonResult mon_43B_CloseFile(MonContext* ctx);
extern MonResult mon_440B_Attach500Segment(MonContext* ctx);
extern MonResult mon_44B_GetUserEntry(MonContext* ctx);
extern MonResult mon_4B_SetBreak(MonContext* ctx);
extern MonResult mon_500B_StartProcess(MonContext* ctx);
extern MonResult mon_501B_StopProcess(MonContext* ctx);
extern MonResult mon_502B_SwitchProcess(MonContext* ctx);
extern MonResult mon_503B_InputString(MonContext* ctx);
extern MonResult mon_504B_OutputString(MonContext* ctx);
extern MonResult mon_505B_GetTrapReason(MonContext* ctx);
extern MonResult mon_507B_SetProcessPriority(MonContext* ctx);
extern MonResult mon_50B_OpenFile(MonContext* ctx);
extern MonResult mon_514B_ND500TimeOut(MonContext* ctx);
extern MonResult mon_52B_TerminalMode(MonContext* ctx);
extern MonResult mon_53B_GetSegmentEntry(MonContext* ctx);
extern MonResult mon_54B_DeleteFile(MonContext* ctx);
extern MonResult mon_55B_GetSpoolingEntry(MonContext* ctx);
extern MonResult mon_56B_SetUserParam(MonContext* ctx);
extern MonResult mon_57B_GetUserParam(MonContext* ctx);
extern MonResult mon_5B_ReadScratchFile(MonContext* ctx);
extern MonResult mon_61B_MemoryAllocation(MonContext* ctx);
extern MonResult mon_62B_GetBytesInFile(MonContext* ctx);
extern MonResult mon_63B_In4x2Bytes(MonContext* ctx);
extern MonResult mon_64B_WarningMessage(MonContext* ctx);
extern MonResult mon_65B_ErrorMessage(MonContext* ctx);
extern MonResult mon_66B_InBufferSpace(MonContext* ctx);
extern MonResult mon_67B_OutBufferSpace(MonContext* ctx);
extern MonResult mon_6B_WriteScratchFile(MonContext* ctx);
extern MonResult mon_70B_CallCommand(MonContext* ctx);
extern MonResult mon_71B_DisableEscape(MonContext* ctx);
extern MonResult mon_72B_EnableEscape(MonContext* ctx);
extern MonResult mon_73B_SetMaxBytes(MonContext* ctx);
extern MonResult mon_74B_SetStartByte(MonContext* ctx);
extern MonResult mon_75B_GetStartByte(MonContext* ctx);
extern MonResult mon_76B_SetBlockSize(MonContext* ctx);
extern MonResult mon_77B_SetStartBlock(MonContext* ctx);
extern MonResult mon_7B_ReadBlock(MonContext* ctx);

/* Register all handlers */
void mon_register_all_handlers(void) {
    mon_register(
        0,           /* MON number (decimal) */
        "0B",         /* Octal string */
        "LEAVE",    /* Short name */
        "ExitFromProgram",          /* Long name */
        "Terminates the program. Returns to SINTRAN III. Batch jobs continues with the next command.\n\n- Backg",  /* Description */
        mon_0B_ExitFromProgram,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        64,           /* MON number (decimal) */
        "100B",         /* Octal string */
        "RT",    /* Short name */
        "StartRTProgram",          /* Long name */
        "Starts an RT program. The program is moved to the execution queue. It is executed according to its p",  /* Description */
        mon_100B_StartRTProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        65,           /* MON number (decimal) */
        "101B",         /* Octal string */
        "SET",    /* Short name */
        "DelayStart",          /* Long name */
        "Starts an RT program after a specified time. The RT program is put in the time queue. It is moved to",  /* Description */
        mon_101B_DelayStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        66,           /* MON number (decimal) */
        "102B",         /* Octal string */
        "ABSET",    /* Short name */
        "StartupTime",          /* Long name */
        "Starts an RT program at a specified time of the day. The RT program is then put in the time queue. I",  /* Description */
        mon_102B_StartupTime,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        67,           /* MON number (decimal) */
        "103B",         /* Octal string */
        "INTV",    /* Short name */
        "StartupInterval",          /* Long name */
        "Prepares an RT program for periodic execution. The interval between the executions can be specified ",  /* Description */
        mon_103B_StartupInterval,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        68,           /* MON number (decimal) */
        "104B",         /* Octal string */
        "HOLD",    /* Short name */
        "SuspendProgram",          /* Long name */
        "Suspends the execution of your program for a given time. The execution then continues after the time",  /* Description */
        mon_104B_SuspendProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        69,           /* MON number (decimal) */
        "105B",         /* Octal string */
        "ABORT",    /* Short name */
        "StopRTProgram",          /* Long name */
        "Stops an RT program. It is removed from the time or execution queue. All reserved devices and files ",  /* Description */
        mon_105B_StopRTProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        70,           /* MON number (decimal) */
        "106B",         /* Octal string */
        "CONCT",    /* Short name */
        "StartOnInterrupt",          /* Long name */
        "StartOnInterrupt connects an RT program to interrupts from a device. The RT program starts when an i",  /* Description */
        mon_106B_StartOnInterrupt,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        71,           /* MON number (decimal) */
        "107B",         /* Octal string */
        "DSCNT",    /* Short name */
        "NoInterruptStart",          /* Long name */
        "StartOnInterrupt connects an RT program to interrupts from a device. You remove this connection with",  /* Description */
        mon_107B_NoInterruptStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        8,           /* MON number (decimal) */
        "10B",         /* Octal string */
        "WPAGE",    /* Short name */
        "WriteBlock",          /* Long name */
        "Writes randomly to a file. You write one block at a time. The file must be opened for random write a",  /* Description */
        mon_10B_WriteBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        72,           /* MON number (decimal) */
        "110B",         /* Octal string */
        "PRIOR",    /* Short name */
        "SetRTPriority",          /* Long name */
        "Sets the priority of an RT program. RT programs may be given priorities from 0 to 255. SINTRAN III e",  /* Description */
        mon_110B_SetRTPriority,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        73,           /* MON number (decimal) */
        "111B",         /* Octal string */
        "UPDAT",    /* Short name */
        "SetClock",          /* Long name */
        "Gives new values to the computer's clock and calendar. If the computer panel has a clock, it is upda",  /* Description */
        mon_111B_SetClock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        74,           /* MON number (decimal) */
        "112B",         /* Octal string */
        "CLADJ",    /* Short name */
        "AdjustClock",          /* Long name */
        "Sets the computer's clock (i.e. the current system time) forward or back. If the operator panel has ",  /* Description */
        mon_112B_AdjustClock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        75,           /* MON number (decimal) */
        "113B",         /* Octal string */
        "CLOCK",    /* Short name */
        "GetCurrentTime",          /* Long name */
        "Gets the current system time and date.\n\n- The current system time is returned as basic time units, s",  /* Description */
        mon_113B_GetCurrentTime,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        76,           /* MON number (decimal) */
        "114B",         /* Octal string */
        "TUSED",    /* Short name */
        "GetTimeUsed",          /* Long name */
        "Gets the time you have used the CPU since you logged in. In batch jobs, you get the time since you e",  /* Description */
        mon_114B_GetTimeUsed,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        77,           /* MON number (decimal) */
        "115B",         /* Octal string */
        "FIX",    /* Short name */
        "FixScattered",          /* Long name */
        "Place a segment in physical memory. Its pages will no longer be swapped to the disk. The segment mus",  /* Description */
        mon_115B_FixScattered,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        78,           /* MON number (decimal) */
        "116B",         /* Octal string */
        "UNFIX",    /* Short name */
        "UnfixSegment",          /* Long name */
        "Releases a fixed segment and removes it from the Page Index Table (PIT). Its pages may then be swapp",  /* Description */
        mon_116B_UnfixSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        79,           /* MON number (decimal) */
        "117B",         /* Octal string */
        "RFILE",    /* Short name */
        "ReadFromFile",          /* Long name */
        "Reads any number of bytes from a file. The read operation must start at the beginning of a block. Th",  /* Description */
        mon_117B_ReadFromFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        9,           /* MON number (decimal) */
        "11B",         /* Octal string */
        "TIME",    /* Short name */
        "GetBasicTime",          /* Long name */
        "**Time**\n\nGets the current internal time. The internal time is specified in basic time units. There ",  /* Description */
        mon_11B_GetBasicTime,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        80,           /* MON number (decimal) */
        "120B",         /* Octal string */
        "WFILE",    /* Short name */
        "WriteToFile",          /* Long name */
        "Writes any number of bytes to a file. The read operation must start at the beginning of a block. The",  /* Description */
        mon_120B_WriteToFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        81,           /* MON number (decimal) */
        "121B",         /* Octal string */
        "WAITF",    /* Short name */
        "AwaitFileTransfer",          /* Long name */
        "Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevan",  /* Description */
        mon_121B_AwaitFileTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        82,           /* MON number (decimal) */
        "122B",         /* Octal string */
        "RESRV",    /* Short name */
        "ReserveResource",          /* Long name */
        "Reserves a device or file for your program only. You release it with ReleaseResource. Some devices, ",  /* Description */
        mon_122B_ReserveResource,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        83,           /* MON number (decimal) */
        "123B",         /* Octal string */
        "RELES",    /* Short name */
        "ReleaseResource",          /* Long name */
        "Releases a reserved device or file. The resource can then be used by another program. You reserve a ",  /* Description */
        mon_123B_ReleaseResource,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        84,           /* MON number (decimal) */
        "124B",         /* Octal string */
        "PRSRV",    /* Short name */
        "ForceReserve",          /* Long name */
        "Reserves a device for an RT program other than that which is calling. Use ForceRelease if the device",  /* Description */
        mon_124B_ForceReserve,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        85,           /* MON number (decimal) */
        "125B",         /* Octal string */
        "PRLRS",    /* Short name */
        "ForceRelease",          /* Long name */
        "Releases a device reserved by an RT program other than that which is calling. You can then reserve t",  /* Description */
        mon_125B_ForceRelease,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        86,           /* MON number (decimal) */
        "126B",         /* Octal string */
        "DSET",    /* Short name */
        "ExactDelayStart",          /* Long name */
        "Sets an RT program to start after a given period. It is then moved from the time queue to the execut",  /* Description */
        mon_126B_ExactDelayStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        87,           /* MON number (decimal) */
        "127B",         /* Octal string */
        "DABST",    /* Short name */
        "ExactStartup",          /* Long name */
        "Starts an RT program at a specific time. The time is given in basic time units. A basic time unit is",  /* Description */
        mon_127B_ExactStartup,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        10,           /* MON number (decimal) */
        "12B",         /* Octal string */
        "SETCM",    /* Short name */
        "SetCommandBuffer",          /* Long name */
        "Transfers a string to the command buffer. The command buffer contains the last command input from th",  /* Description */
        mon_12B_SetCommandBuffer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        88,           /* MON number (decimal) */
        "130B",         /* Octal string */
        "DINTV",    /* Short name */
        "ExactInterval",          /* Long name */
        "Prepares an RT program for periodic execution. The interval between the executions may be from 1 to ",  /* Description */
        mon_130B_ExactInterval,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        89,           /* MON number (decimal) */
        "131B",         /* Octal string */
        "ABSTR",    /* Short name */
        "DataTransfer",          /* Long name */
        "Transfers data between physical memory and a mass-storage device, e.g. a disk or magnetic tape. You ",  /* Description */
        mon_131B_DataTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        90,           /* MON number (decimal) */
        "132B",         /* Octal string */
        "MCALL",    /* Short name */
        "JumpToSegment",          /* Long name */
        "Calls a routine on another segment in the ND-100. You can divide an ND-100 RT program between variou",  /* Description */
        mon_132B_JumpToSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        91,           /* MON number (decimal) */
        "133B",         /* Octal string */
        "MEXIT",    /* Short name */
        "ExitFromSegment",          /* Long name */
        "Exchanges one or both current segments. Commonly used to return after the monitor call JumpToSegment",  /* Description */
        mon_133B_ExitFromSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        92,           /* MON number (decimal) */
        "134B",         /* Octal string */
        "RTEXT",    /* Short name */
        "ExitRTProgram",          /* Long name */
        "Terminates the calling RT or background program. Releases all reserved resources. The monitor call h",  /* Description */
        mon_134B_ExitRTProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        93,           /* MON number (decimal) */
        "135B",         /* Octal string */
        "RTWT",    /* Short name */
        "WaitForRestart",          /* Long name */
        "Sets the RT program in a waiting state. It is restarted by StartRTProgram or @RT. Execution continue",  /* Description */
        mon_135B_WaitForRestart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        94,           /* MON number (decimal) */
        "136B",         /* Octal string */
        "RTON",    /* Short name */
        "EnableRTStart",          /* Long name */
        "RTON RT programs cannot be started after DisableRTStart has been executed. Use EnableRTStart to do t",  /* Description */
        mon_136B_EnableRTStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        95,           /* MON number (decimal) */
        "137B",         /* Octal string */
        "RTOFF",    /* Short name */
        "DisableRTStart",          /* Long name */
        "Disables start of RT programs. No RT program can be started before EnableRTStart is executed.\n\n- RT ",  /* Description */
        mon_137B_DisableRTStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        11,           /* MON number (decimal) */
        "13B",         /* Octal string */
        "CIBUF",    /* Short name */
        "ClearInBuffer",          /* Long name */
        "Clears a device input buffer. Input from character devices, e.g. terminals, are temporarily stored i",  /* Description */
        mon_13B_ClearInBuffer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        96,           /* MON number (decimal) */
        "140B",         /* Octal string */
        "WHDEV",    /* Short name */
        "ReservationInfo",          /* Long name */
        "Checks that a device is not reserved. If it is reserved, you will receive information about which RT",  /* Description */
        mon_140B_ReservationInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        97,           /* MON number (decimal) */
        "141B",         /* Octal string */
        "IOSET",    /* Short name */
        "DeviceControl",          /* Long name */
        "Sets control information for a character device, e.g. a terminal or a printer. The control informati",  /* Description */
        mon_141B_DeviceControl,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        98,           /* MON number (decimal) */
        "142B",         /* Octal string */
        "ERMON",    /* Short name */
        "ToErrorDevice",          /* Long name */
        "Outputs a user-defined, real-time error. The error message is output on the error device, i.e. norma",  /* Description */
        mon_142B_ToErrorDevice,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        99,           /* MON number (decimal) */
        "143B",         /* Octal string */
        "RSIO",    /* Short name */
        "ExecutionInfo",          /* Long name */
        "Gets information about the execution of the calling program. You are told whether the program execut",  /* Description */
        mon_143B_ExecutionInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        100,           /* MON number (decimal) */
        "144B",         /* Octal string */
        "MAGTP",    /* Short name */
        "DeviceFunction",          /* Long name */
        "Performs various operations on floppy disks, magnetic tapes, Versatec plotters, and SCSI streamers.\n",  /* Description */
        mon_144B_DeviceFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        102,           /* MON number (decimal) */
        "146B",         /* Octal string */
        "IPRIV",    /* Short name */
        "PrivInstruction",          /* Long name */
        "Executes a privileged machine instruction on the ND-100. Privileged instructions may, for example, t",  /* Description */
        mon_146B_PrivInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        103,           /* MON number (decimal) */
        "147B",         /* Octal string */
        "CAMAC",    /* Short name */
        "CAMACFunction",          /* Long name */
        "Operates the CAMAC, i.e. executes the NAF register. CAMAC is a standardized way to connect periphera",  /* Description */
        mon_147B_CAMACFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        12,           /* MON number (decimal) */
        "14B",         /* Octal string */
        "COBUF",    /* Short name */
        "ClearOutBuffer",          /* Long name */
        "Clears a device output buffer. Output to character devices, e.g. terminals, are temporarily stored i",  /* Description */
        mon_14B_ClearOutBuffer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        104,           /* MON number (decimal) */
        "150B",         /* Octal string */
        "GL",    /* Short name */
        "CAMACGLRegister",          /* Long name */
        "Read the CAMAC GL (Graded LAM - \"look at me\") register or the last CAMAC identification number. See ",  /* Description */
        mon_150B_CAMACGLRegister,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        105,           /* MON number (decimal) */
        "151B",         /* Octal string */
        "GRTDA",    /* Short name */
        "GetRTAddress",          /* Long name */
        "Gets the address of an RT description. You specify the name of the RT program. See the SINTRAN III R",  /* Description */
        mon_151B_GetRTAddress,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        106,           /* MON number (decimal) */
        "152B",         /* Octal string */
        "GRTNA",    /* Short name */
        "GetRTName",          /* Long name */
        "Gets the name of an RT program. You specify the RT description address.\n\n- This monitor call is only",  /* Description */
        mon_152B_GetRTName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        107,           /* MON number (decimal) */
        "153B",         /* Octal string */
        "IOXN",    /* Short name */
        "CAMACIOInstruction",          /* Long name */
        "Executes a single IOX instruction for CAMAC. See under CAMACFunction (mon 147) for general informati",  /* Description */
        mon_153B_CAMACIOInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        108,           /* MON number (decimal) */
        "154B",         /* Octal string */
        "ASSIG",    /* Short name */
        "AssignCAMACLAM",          /* Long name */
        "Assigns a graded LAM in the CAMAC identification table to a logical device number in the logical num",  /* Description */
        mon_154B_AssignCAMACLAM,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        109,           /* MON number (decimal) */
        "155B",         /* Octal string */
        "GRAPH",    /* Short name */
        "GraphicFunction",          /* Long name */
        "Executes various functions on a graphic peripheral, such as a NORDCOM terminal, a pen plotter, or a ",  /* Description */
        mon_155B_GraphicFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        111,           /* MON number (decimal) */
        "157B",         /* Octal string */
        "ENTSG",    /* Short name */
        "SegmentToPageTable",          /* Long name */
        "Enters a routine as a direct task or as a device driver, and \"remembers\" which segments have been en",  /* Description */
        mon_157B_SegmentToPageTable,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        112,           /* MON number (decimal) */
        "160B",         /* Octal string */
        "FIXC",    /* Short name */
        "FixContiguous",          /* Long name */
        "Places a segment in physical memory. Its pages will no longer be swapped to the disk. The segment is",  /* Description */
        mon_160B_FixContiguous,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        113,           /* MON number (decimal) */
        "161B",         /* Octal string */
        "INSTR",    /* Short name */
        "InString",          /* Long name */
        "Reads a string of characters from a peripheral device, e.g. a terminal.",  /* Description */
        mon_161B_InString,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        114,           /* MON number (decimal) */
        "162B",         /* Octal string */
        "OUTST",    /* Short name */
        "OutString",          /* Long name */
        "Writes a string of characters to a peripheral file, e.g., a terminal or a printer.\n\n- You cannot use",  /* Description */
        mon_162B_OutString,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        116,           /* MON number (decimal) */
        "164B",         /* Octal string */
        "WSEG",    /* Short name */
        "SaveSegment",          /* Long name */
        "Saves a segment in the ND-100. All pages in physical memory which have been changed, are written bac",  /* Description */
        mon_164B_SaveSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        117,           /* MON number (decimal) */
        "165B",         /* Octal string */
        "DIW",    /* Short name */
        "GetInRegisters",          /* Long name */
        "Reads the device interface registers.",  /* Description */
        mon_165B_GetInRegisters,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        119,           /* MON number (decimal) */
        "167B",         /* Octal string */
        "REENT",    /* Short name */
        "AttachSegment",          /* Long name */
        "Attaches a reentrant segment to your two current segments. The address areas of the segments may ove",  /* Description */
        mon_167B_AttachSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        14,           /* MON number (decimal) */
        "16B",         /* Octal string */
        "MGTTY",    /* Short name */
        "GetTerminalType",          /* Long name */
        "Gets the terminal type. The terminal type tells SINTRAN III how to handle a particular terminal. A w",  /* Description */
        mon_16B_GetTerminalType,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        120,           /* MON number (decimal) */
        "170B",         /* Octal string */
        "US0",    /* Short name */
        "UserDef0",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_170B_UserDef0,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        121,           /* MON number (decimal) */
        "171B",         /* Octal string */
        "US1",    /* Short name */
        "UserDef1",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_171B_UserDef1,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        122,           /* MON number (decimal) */
        "172B",         /* Octal string */
        "US2",    /* Short name */
        "UserDef2",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_172B_UserDef2,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        123,           /* MON number (decimal) */
        "173B",         /* Octal string */
        "US3",    /* Short name */
        "UserDef3",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_173B_UserDef3,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        124,           /* MON number (decimal) */
        "174B",         /* Octal string */
        "US4",    /* Short name */
        "UserDef4",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_174B_UserDef4,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        125,           /* MON number (decimal) */
        "175B",         /* Octal string */
        "US5",    /* Short name */
        "UserDef5",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_175B_UserDef5,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        126,           /* MON number (decimal) */
        "176B",         /* Octal string */
        "US6",    /* Short name */
        "UserDef6",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_176B_UserDef6,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        127,           /* MON number (decimal) */
        "177B",         /* Octal string */
        "US7",    /* Short name */
        "UserDef7",          /* Long name */
        "User-defined monitor call. You can implement up to 8 monitor calls yourself. These are named UserDef",  /* Description */
        mon_177B_UserDef7,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        15,           /* MON number (decimal) */
        "17B",         /* Octal string */
        "MSTTY",    /* Short name */
        "SetTerminalType",          /* Long name */
        "Sets the type of a terminal. The terminal type tells SINTRAN III how to handle a particular terminal",  /* Description */
        mon_17B_SetTerminalType,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        1,           /* MON number (decimal) */
        "1B",         /* Octal string */
        "INBT",    /* Short name */
        "InByte",          /* Long name */
        "Reads one byte from a character device, e.g. a terminal or an opened file. If the device is a word-o",  /* Description */
        mon_1B_InByte,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        128,           /* MON number (decimal) */
        "200B",         /* Octal string */
        "XMSG",    /* Short name */
        "XMSGFunction",          /* Long name */
        "Performs various data communication functions. All types of programs may communicate through this mo",  /* Description */
        mon_200B_XMSGFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        129,           /* MON number (decimal) */
        "201B",         /* Octal string */
        "MHDLC",    /* Short name */
        "HDLCfunction",          /* Long name */
        "Performs various HDLC functions. A HDLC is a high-level data link to another computer. You may send ",  /* Description */
        mon_201B_HDLCfunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        134,           /* MON number (decimal) */
        "206B",         /* Octal string */
        "EDTRM",    /* Short name */
        "TerminationHandling",          /* Long name */
        "Switches termination handling on and off.",  /* Description */
        mon_206B_TerminationHandling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        135,           /* MON number (decimal) */
        "207B",         /* Octal string */
        "RERRP",    /* Short name */
        "GetErrorInfo",          /* Long name */
        "Gets information about the last real-time error. The monitor call returns the error, the RT program ",  /* Description */
        mon_207B_GetErrorInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        138,           /* MON number (decimal) */
        "212B",         /* Octal string */
        "SREEN",    /* Short name */
        "ReentrantSegment",          /* Long name */
        "Connects a reentrant segment to your two current segments. All modified pages of your current segmen",  /* Description */
        mon_212B_ReentrantSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        139,           /* MON number (decimal) */
        "213B",         /* Octal string */
        "MUIDI",    /* Short name */
        "GetDirUserIndexes",          /* Long name */
        "Gets a directory index and a user index. You have to specify a directory name and a user name.\n\n- Us",  /* Description */
        mon_213B_GetDirUserIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        140,           /* MON number (decimal) */
        "214B",         /* Octal string */
        "GUSNA",    /* Short name */
        "GetUserName",          /* Long name */
        "Gets the name of a user. The user may be on a remote computer if the COSMOS network is installed. Th",  /* Description */
        mon_214B_GetUserName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        141,           /* MON number (decimal) */
        "215B",         /* Octal string */
        "DROBJ",    /* Short name */
        "GetObjectEntry",          /* Long name */
        "Gets information about a file. An object entry describes each file. It contains the file name, the a",  /* Description */
        mon_215B_GetObjectEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        142,           /* MON number (decimal) */
        "216B",         /* Octal string */
        "DWOBJ",    /* Short name */
        "SetObjectEntry",          /* Long name */
        "Changes the description of a file. An object entry describes each file. It contains the file name, t",  /* Description */
        mon_216B_SetObjectEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        143,           /* MON number (decimal) */
        "217B",         /* Octal string */
        "GUIOI",    /* Short name */
        "GetAllFileIndexes",          /* Long name */
        "Gets the directory index, the user index, and the object index of a file. These are indexes in the S",  /* Description */
        mon_217B_GetAllFileIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        17,           /* MON number (decimal) */
        "21B",         /* Octal string */
        "M8INB",    /* Short name */
        "InUpTo8Bytes",          /* Long name */
        "See also In8Bytes, InByte, InString, In4x2Bytes, and Out8Bytes.",  /* Description */
        mon_21B_InUpTo8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        144,           /* MON number (decimal) */
        "220B",         /* Octal string */
        "DOPEN",    /* Short name */
        "DirectOpen",          /* Long name */
        "Opens a file. Files must be opened before they can be accessed. For public users this monitor call i",  /* Description */
        mon_220B_DirectOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        145,           /* MON number (decimal) */
        "221B",         /* Octal string */
        "CRALF",    /* Short name */
        "CreateFile",          /* Long name */
        "Creates a file. The file may be indexed, contiguous, or allocated. Most files are indexed. The size ",  /* Description */
        mon_221B_CreateFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        146,           /* MON number (decimal) */
        "222B",         /* Octal string */
        "GBSIZ",    /* Short name */
        "GetAddressArea",          /* Long name */
        "Gets the size of your address area. Your address area may consist of one or two 128 Kbyte areas. Thi",  /* Description */
        mon_222B_GetAddressArea,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        151,           /* MON number (decimal) */
        "227B",         /* Octal string */
        "MSDAE",    /* Short name */
        "SetEscLocalChars",          /* Long name */
        "You can terminate most programs with the ESCAPE key. A LOCAL key has a similar function. It terminat",  /* Description */
        mon_227B_SetEscLocalChars,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        18,           /* MON number (decimal) */
        "22B",         /* Octal string */
        "M8OUT",    /* Short name */
        "OutUpTo8Bytes",          /* Long name */
        "Writes up to 8 characters to a device, e.g. a terminal or an internal device.",  /* Description */
        mon_22B_OutUpTo8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        152,           /* MON number (decimal) */
        "230B",         /* Octal string */
        "MGDAE",    /* Short name */
        "GetEscLocalChars",          /* Long name */
        "Gets ESCAPE and LOCAL characters. You can terminate most programs with the ESCAPE key. A LOCAL key h",  /* Description */
        mon_230B_GetEscLocalChars,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        153,           /* MON number (decimal) */
        "231B",         /* Octal string */
        "EXPFI",    /* Short name */
        "ExpandFile",          /* Long name */
        "Expands the file size. You use this monitor call to increase the size of contiguous and allocated fi",  /* Description */
        mon_231B_ExpandFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        154,           /* MON number (decimal) */
        "232B",         /* Octal string */
        "MRNFI",    /* Short name */
        "RenameFile",          /* Long name */
        "See also @RENAME-FILE.",  /* Description */
        mon_232B_RenameFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        155,           /* MON number (decimal) */
        "233B",         /* Octal string */
        "STEFI",    /* Short name */
        "SetTemporaryFile",          /* Long name */
        "Defines a file to store information temporarily. The file can be read once. When it is closed, its c",  /* Description */
        mon_233B_SetTemporaryFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        156,           /* MON number (decimal) */
        "234B",         /* Octal string */
        "SPEFI",    /* Short name */
        "SetPeripheralName",          /* Long name */
        "Defines a peripheral file, e.g. a printer. You connect a file name to the logical device number of t",  /* Description */
        mon_234B_SetPeripheralName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        157,           /* MON number (decimal) */
        "235B",         /* Octal string */
        "SCROP",    /* Short name */
        "ScratchOpen",          /* Long name */
        "Opens a file as a scratch file. A maximum of 64 pages of the file is kept when you close the file. U",  /* Description */
        mon_235B_ScratchOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        158,           /* MON number (decimal) */
        "236B",         /* Octal string */
        "SPERD",    /* Short name */
        "SetPermanentOpen",          /* Long name */
        "Sets a file permanently open. The file is not closed by CloseFile with -1 as file number. You have t",  /* Description */
        mon_236B_SetPermanentOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        159,           /* MON number (decimal) */
        "237B",         /* Octal string */
        "SFACC",    /* Short name */
        "SetFileAccess",          /* Long name */
        "Sets the access protection for a file. You should specify the access for yourself, friends, and othe",  /* Description */
        mon_237B_SetFileAccess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        19,           /* MON number (decimal) */
        "23B",         /* Octal string */
        "B8INB",    /* Short name */
        "In8Bytes",          /* Long name */
        "Reads 8 bytes from a device. The input is fast, but the monitor call does not apply the defined echo",  /* Description */
        mon_23B_In8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        160,           /* MON number (decimal) */
        "240B",         /* Octal string */
        "APSPE",    /* Short name */
        "AppendSpooling",          /* Long name */
        "Prints a file. The printer has a queue of files waiting to be output. The file is appended to this q",  /* Description */
        mon_240B_AppendSpooling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        161,           /* MON number (decimal) */
        "241B",         /* Octal string */
        "SUSCN",    /* Short name */
        "NewUser",          /* Long name */
        "Switches the user name you are logged in under. The command is similar to logging out and then loggi",  /* Description */
        mon_241B_NewUser,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        162,           /* MON number (decimal) */
        "242B",         /* Octal string */
        "RUSCN",    /* Short name */
        "OldUser",          /* Long name */
        "Switches back to the user name you were logged in under before NewUser. The command is similar to lo",  /* Description */
        mon_242B_OldUser,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        163,           /* MON number (decimal) */
        "243B",         /* Octal string */
        "FDINA",    /* Short name */
        "GetDirNameIndex",          /* Long name */
        "Gets directory index and name index. The name index identifies the device description of the disk. Y",  /* Description */
        mon_243B_GetDirNameIndex,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        164,           /* MON number (decimal) */
        "244B",         /* Octal string */
        "GDIEN",    /* Short name */
        "GetDirEntry",          /* Long name */
        "Gets information about a directory. The directory entry is returned. Appendix C describes the file s",  /* Description */
        mon_244B_GetDirEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        165,           /* MON number (decimal) */
        "245B",         /* Octal string */
        "GNAEN",    /* Short name */
        "GetNameEntry",          /* Long name */
        "Gets information about devices, e.g. disks and floppy disks. The monitor call returns the name entry",  /* Description */
        mon_245B_GetNameEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        166,           /* MON number (decimal) */
        "246B",         /* Octal string */
        "REDIR",    /* Short name */
        "ReserveDir",          /* Long name */
        "Reserves a directory for special use. The directory must be entered. Other users will not be able to",  /* Description */
        mon_246B_ReserveDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        167,           /* MON number (decimal) */
        "247B",         /* Octal string */
        "RLDIR",    /* Short name */
        "ReleaseDir",          /* Long name */
        "Releases a directory. The directory must have been reserved with ReserveDir.",  /* Description */
        mon_247B_ReleaseDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        20,           /* MON number (decimal) */
        "24B",         /* Octal string */
        "B8OUT",    /* Short name */
        "Out8Bytes",          /* Long name */
        "Writes 8 bytes to a character device, e.g. a terminal. All 8 bytes are output. OutUpTo8Bytes stops i",  /* Description */
        mon_24B_Out8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        168,           /* MON number (decimal) */
        "250B",         /* Octal string */
        "FDFDI",    /* Short name */
        "GetDefaultDir",          /* Long name */
        "Gets the user?s default directory. The directory index and the user index are returned.\n\n- Use Execu",  /* Description */
        mon_250B_GetDefaultDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        169,           /* MON number (decimal) */
        "251B",         /* Octal string */
        "COPAG",    /* Short name */
        "CopyPage",          /* Long name */
        "Copies file pages between two opened files. One of the files may be a magnetic tape or floppy disk w",  /* Description */
        mon_251B_CopyPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register(
        170,           /* MON number (decimal) */
        "252B",         /* Octal string */
        "BCLOS",    /* Short name */
        "BackupClose",          /* Long name */
        "Closes a file. The version number and the last date accessed are unchanged. The number of pages in t",  /* Description */
        mon_252B_BackupClose,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        171,           /* MON number (decimal) */
        "253B",         /* Octal string */
        "CRALN",    /* Short name */
        "NewFileVersion",          /* Long name */
        "Creates new versions of a file. You may create new versions for both indexed, contiguous and allocat",  /* Description */
        mon_253B_NewFileVersion,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        172,           /* MON number (decimal) */
        "254B",         /* Octal string */
        "GERDV",    /* Short name */
        "GetErrorDevice",          /* Long name */
        "Gets the logical device number of the error device. The error device may be reserved by an RT progra",  /* Description */
        mon_254B_GetErrorDevice,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        173,           /* MON number (decimal) */
        "255B",         /* Octal string */
        "PIOCM",    /* Short name */
        "PIOCFunction",          /* Long name */
        "PIOC is a programmable input and output processor primarily used in data communication to handle net",  /* Description */
        mon_255B_PIOCFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register(
        174,           /* MON number (decimal) */
        "256B",         /* Octal string */
        "DEABF",    /* Short name */
        "FullFileName",          /* Long name */
        "Returns a complete file name from an abbreviated one. The directory, the user, the file name, the fi",  /* Description */
        mon_256B_FullFileName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        175,           /* MON number (decimal) */
        "257B",         /* Octal string */
        "FOPEN",    /* Short name */
        "OpenFileInfo",          /* Long name */
        "Gets information about an open file. You specify the file name. The monitor call returns the file nu",  /* Description */
        mon_257B_OpenFileInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        178,           /* MON number (decimal) */
        "262B",         /* Octal string */
        "CPUST",    /* Short name */
        "GetSystemInfo",          /* Long name */
        "Gets various system information. The system number, the CPU type, the SINTRAN III version, the instr",  /* Description */
        mon_262B_GetSystemInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        179,           /* MON number (decimal) */
        "263B",         /* Octal string */
        "GDEVT",    /* Short name */
        "GetDeviceType",          /* Long name */
        "Gets the device type, e.g. terminal, floppy disk, mass-storage file, etc. The monitor call also prov",  /* Description */
        mon_263B_GetDeviceType,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        183,           /* MON number (decimal) */
        "267B",         /* Octal string */
        "TMOUT",    /* Short name */
        "TimeOut",          /* Long name */
        "Suspends the execution of your program for a given time. The execution then continues after the moni",  /* Description */
        mon_267B_TimeOut,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        22,           /* MON number (decimal) */
        "26B",         /* Octal string */
        "LASTC",    /* Short name */
        "GetLastByte",          /* Long name */
        "Gets the last character typed on a terminal. The monitor call can be used to terminate long output s",  /* Description */
        mon_26B_GetLastByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        184,           /* MON number (decimal) */
        "270B",         /* Octal string */
        "RDPAG",    /* Short name */
        "ReadDiskPage",          /* Long name */
        "Reads one or more directory pages. Any page can be read.\n\n- The directory must be reserved with Rese",  /* Description */
        mon_270B_ReadDiskPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        185,           /* MON number (decimal) */
        "271B",         /* Octal string */
        "WDPAG",    /* Short name */
        "WriteDiskPage",          /* Long name */
        "Writes to one or more pages in a directory. Any page can be written to.\n\n- The directory must be res",  /* Description */
        mon_271B_WriteDiskPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        186,           /* MON number (decimal) */
        "272B",         /* Octal string */
        "DELPG",    /* Short name */
        "DeletePage",          /* Long name */
        "Deletes pages from a file. Pages between two page numbers are removed.\n\n- The file must be opened.",  /* Description */
        mon_272B_DeletePage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        187,           /* MON number (decimal) */
        "273B",         /* Octal string */
        "MGFIL",    /* Short name */
        "GetFileName",          /* Long name */
        "Gets the name of a file. You specify the directory index, the user index, and the object index. The ",  /* Description */
        mon_273B_GetFileName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        188,           /* MON number (decimal) */
        "274B",         /* Octal string */
        "FOBJN",    /* Short name */
        "GetFileIndexes",          /* Long name */
        "Gets the directory index, the user index, and the object index of a file. These are indexes in the f",  /* Description */
        mon_274B_GetFileIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        189,           /* MON number (decimal) */
        "275B",         /* Octal string */
        "STRFI",    /* Short name */
        "SetTerminalName",          /* Long name */
        "Defines the file name to be used for terminals. This is normally `TERMINAL:`. Background users ident",  /* Description */
        mon_275B_SetTerminalName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        190,           /* MON number (decimal) */
        "276B",         /* Octal string */
        "ELOFU",    /* Short name */
        "EnableLocal",          /* Long name */
        "You may log in on remote computers through the COSMOS data network. A key on the terminal returns yo",  /* Description */
        mon_276B_EnableLocal,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        191,           /* MON number (decimal) */
        "277B",         /* Octal string */
        "DLOFU",    /* Short name */
        "DisableLocal",          /* Long name */
        "You may log in on remote computers through the COSMOS data network. A key on the terminal returns yo",  /* Description */
        mon_277B_DisableLocal,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        23,           /* MON number (decimal) */
        "27B",         /* Octal string */
        "RTDSC",    /* Short name */
        "GetRTDescr",          /* Long name */
        "Reads an RT description. The RT description contains various information about an RT program. You sp",  /* Description */
        mon_27B_GetRTDescr,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        2,           /* MON number (decimal) */
        "2B",         /* Octal string */
        "OUTBT",    /* Short name */
        "OutByte",          /* Long name */
        "Writes one byte to a character device, e.g. a terminal or an opened file. If the device is a word-or",  /* Description */
        mon_2B_OutByte,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        192,           /* MON number (decimal) */
        "300B",         /* Octal string */
        "EUSEL",    /* Short name */
        "SetEscapeHandling",          /* Long name */
        "Enables user-defined escape handling. When the ESCAPE key is pressed, execution continues at the spe",  /* Description */
        mon_300B_SetEscapeHandling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        193,           /* MON number (decimal) */
        "301B",         /* Octal string */
        "DUSEL",    /* Short name */
        "StopEscapeHandling",          /* Long name */
        "Disables user-defined escape handling. The ESCAPE key terminates the program as normal. StartEscapeH",  /* Description */
        mon_301B_StopEscapeHandling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        194,           /* MON number (decimal) */
        "302B",         /* Octal string */
        "ELON",    /* Short name */
        "OnEscLocalFunction",          /* Long name */
        "Enables delayed escape and local functions for your terminal. The ESCAPE key then terminates a progr",  /* Description */
        mon_302B_OnEscLocalFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        195,           /* MON number (decimal) */
        "303B",         /* Octal string */
        "ELOFF",    /* Short name */
        "OffEscLocalFunction",          /* Long name */
        "Delays the escape and local functions for your terminal. Then the ESCAPE key or LOCAL key does not t",  /* Description */
        mon_303B_OffEscLocalFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        198,           /* MON number (decimal) */
        "306B",         /* Octal string */
        "GTMOD",    /* Short name */
        "GetTerminalMode",          /* Long name */
        "Gets the terminal mode. The terminal mode tells how the terminal function, i.e. if all letters are c",  /* Description */
        mon_306B_GetTerminalMode,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        199,           /* MON number (decimal) */
        "307B",         /* Octal string */
        "TNOWAI",    /* Short name */
        "TerminalNoWait",          /* Long name */
        "Switches No Wait on and off. No Wait is useful for input from, and output to, character devices, e.g",  /* Description */
        mon_307B_TerminalNoWait,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        24,           /* MON number (decimal) */
        "30B",         /* Octal string */
        "GETRT",    /* Short name */
        "GetOwnRTAddress",          /* Long name */
        "Gets the address of the calling program's RT description. Background programs get the RT description",  /* Description */
        mon_30B_GetOwnRTAddress,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        200,           /* MON number (decimal) */
        "310B",         /* Octal string */
        "TBIN8",    /* Short name */
        "In8AndFlag",          /* Long name */
        "Reads 8 bytes from a device, e.g., a terminal. The monitor call applies to the defined echo and brea",  /* Description */
        mon_310B_In8AndFlag,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        201,           /* MON number (decimal) */
        "311B",         /* Octal string */
        "WDIEN",    /* Short name */
        "WriteDirEntry",          /* Long name */
        "Changes the information about a directory. The complete contents of the directory entry is set. The ",  /* Description */
        mon_311B_WriteDirEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        202,           /* MON number (decimal) */
        "312B",         /* Octal string */
        "MOINF",    /* Short name */
        "CheckMonCall",          /* Long name */
        "Some monitor calls are optional or only available in later versions of SINTRAN III. This monitor cal",  /* Description */
        mon_312B_CheckMonCall,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        203,           /* MON number (decimal) */
        "313B",         /* Octal string */
        "IBRISZ",    /* Short name */
        "InBufferState",          /* Long name */
        "Gets information about an input buffer. The current number of bytes in it, and the number of bytes u",  /* Description */
        mon_313B_InBufferState,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        204,           /* MON number (decimal) */
        "314B",         /* Octal string */
        "SRUSI",    /* Short name */
        "DefaultRemoteSystem",          /* Long name */
        "Sets default values for COSMOS remote file access. You can specify the default remote system, the re",  /* Description */
        mon_314B_DefaultRemoteSystem,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        205,           /* MON number (decimal) */
        "315B",         /* Octal string */
        "MLAMU",    /* Short name */
        "LAMUFunction",          /* Long name */
        "Performs various functions on the LAMU system. A LAMU is a logically addressed memory unit. The LAMU",  /* Description */
        mon_315B_LAMUFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        206,           /* MON number (decimal) */
        "316B",         /* Octal string */
        "SRLMO",    /* Short name */
        "SetRemoteAccess",          /* Long name */
        "Switches remote file access on and off. The COSMOS network allows you to access files in remote comp",  /* Description */
        mon_316B_SetRemoteAccess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        207,           /* MON number (decimal) */
        "317B",         /* Octal string */
        "UECOM",    /* Short name */
        "ExecuteCommand",          /* Long name */
        "Executes a SINTRAN III command. Specify the command name and the parameters as a text string.\n\n- An ",  /* Description */
        mon_317B_ExecuteCommand,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        25,           /* MON number (decimal) */
        "31B",         /* Octal string */
        "EXIOX",    /* Short name */
        "IOInstruction",          /* Long name */
        "Executes an IOX machine instruction. The IOX instruction handles the device registers. The IOX instr",  /* Description */
        mon_31B_IOInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        210,           /* MON number (decimal) */
        "322B",         /* Octal string */
        "GSGNO",    /* Short name */
        "GetSegmentNo",          /* Long name */
        "Gets the number of a segment in the ND-100. You specify the segment name. Segment names are created ",  /* Description */
        mon_322B_GetSegmentNo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        211,           /* MON number (decimal) */
        "323B",         /* Octal string */
        "SPLRE",    /* Short name */
        "SegmentOverlay",          /* Long name */
        "Used to build multisegment programs in the ND-100. It is mainly for internal use. A new reentrant se",  /* Description */
        mon_323B_SegmentOverlay,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        212,           /* MON number (decimal) */
        "324B",         /* Octal string */
        "OCTIO",    /* Short name */
        "OctobusFunction",          /* Long name */
        "Performs various functions on an old Octobus (earlier than version 3).",  /* Description */
        mon_324B_OctobusFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        213,           /* MON number (decimal) */
        "325B",         /* Octal string */
        "MBECH",    /* Short name */
        "BatchModeEcho",          /* Long name */
        "Controls echo of input and output if the program is executed in a batch or mode job. The purpose is ",  /* Description */
        mon_325B_BatchModeEcho,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        214,           /* MON number (decimal) */
        "326B",         /* Octal string */
        "MLOGI",    /* Short name */
        "LogInStart",          /* Long name */
        "Logs in a user on a terminal and starts a subsystem.",  /* Description */
        mon_326B_LogInStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register(
        26,           /* MON number (decimal) */
        "32B",         /* Octal string */
        "MSG",    /* Short name */
        "OutMessage",          /* Long name */
        "Writes a message to the user's terminal. This is convenient for error messages in background program",  /* Description */
        mon_32B_OutMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        216,           /* MON number (decimal) */
        "330B",         /* Octal string */
        "TERST",    /* Short name */
        "TerminalStatus",          /* Long name */
        "Gets information about a terminal. The user logged in, the time logged in, the CPU time used, the jo",  /* Description */
        mon_330B_TerminalStatus,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        218,           /* MON number (decimal) */
        "332B",         /* Octal string */
        "TREPP",    /* Short name */
        "TerminalLineInfo",          /* Long name */
        "Gets information about a terminal line. You may also enable programs to continue in spite of errors ",  /* Description */
        mon_332B_TerminalLineInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        219,           /* MON number (decimal) */
        "333B",         /* Octal string */
        "UDMA",    /* Short name */
        "DMAFunction",          /* Long name */
        "Various DMA functions for Direct Memory Access operations.\n\nFunction codes:\n- 1: Receive DMA data (i",  /* Description */
        mon_333B_DMAFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        220,           /* MON number (decimal) */
        "334B",         /* Octal string */
        "GETXM",    /* Short name */
        "GetErrorMessage",          /* Long name */
        "Gets a SINTRAN III error message text. Appendix A shows the messages connected to each error number.",  /* Description */
        mon_334B_GetErrorMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        221,           /* MON number (decimal) */
        "335B",         /* Octal string */
        "EXABS",    /* Short name */
        "TransferData",          /* Long name */
        "Transfers data between physical memory and a mass-storage device, e.g. a disk. You may perform vario",  /* Description */
        mon_335B_TransferData,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        222,           /* MON number (decimal) */
        "336B",         /* Octal string */
        "IOMTY",    /* Short name */
        "Terminal",          /* Long name */
        "This I/O multifunction monitor call is used to change the attributes of terminal and terminal access",  /* Description */
        mon_336B_Terminal,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        223,           /* MON number (decimal) */
        "337B",         /* Octal string */
        "SPCHG",    /* Short name */
        "ChangeSegment",          /* Long name */
        "Changes the segment and the page table your program uses. The monitor call is similar to JumpToSegme",  /* Description */
        mon_337B_ChangeSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        27,           /* MON number (decimal) */
        "33B",         /* Octal string */
        "ALTON",    /* Short name */
        "AltPageTable",          /* Long name */
        "Switches page table. Each page table allows you to access 128 Kbyte memory. SINTRAN III has 4 page t",  /* Description */
        mon_33B_AltPageTable,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        224,           /* MON number (decimal) */
        "340B",         /* Octal string */
        "RSREC",    /* Short name */
        "ReadSystemRecord",          /* Long name */
        "Used to read the system record into a buffer.",  /* Description */
        mon_340B_ReadSystemRecord,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        225,           /* MON number (decimal) */
        "341B",         /* Octal string */
        "SGMTY",    /* Short name */
        "SegmentFunction",          /* Long name */
        "This is a multifunction monitor call used to change the active segments of a program, or the page in",  /* Description */
        mon_341B_SegmentFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register(
        28,           /* MON number (decimal) */
        "34B",         /* Octal string */
        "ALTOFF",    /* Short name */
        "NormalPageTable",          /* Long name */
        "Sets the alternative page table equal to the normal page table. All memory addresses are mapped thro",  /* Description */
        mon_34B_NormalPageTable,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        29,           /* MON number (decimal) */
        "35B",         /* Octal string */
        "IOUT",    /* Short name */
        "OutNumber",          /* Long name */
        "Writes a number to the user's terminal. The number can be output as an octal or a decimal value.\n\n- ",  /* Description */
        mon_35B_OutNumber,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        30,           /* MON number (decimal) */
        "36B",         /* Octal string */
        "NOWT",    /* Short name */
        "NoWaitSwitch",          /* Long name */
        "Switches No Wait on and off. No Wait is useful for input from, and output to several devices simulta",  /* Description */
        mon_36B_NoWaitSwitch,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        31,           /* MON number (decimal) */
        "37B",         /* Octal string */
        "AIRDW",    /* Short name */
        "ReadADChannel",          /* Long name */
        "Reads an analog to digital channel.\n\n### PARAMETERS",  /* Description */
        mon_37B_ReadADChannel,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        3,           /* MON number (decimal) */
        "3B",         /* Octal string */
        "ECHOM",    /* Short name */
        "SetEcho",          /* Long name */
        "When you press a key on the terminal, a character is normally displayed. This is called echo. You mo",  /* Description */
        mon_3B_SetEcho,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        256,           /* MON number (decimal) */
        "400B",         /* Octal string */
        "MACROE",    /* Short name */
        "ErrorReturn",          /* Long name */
        "Terminates the program and sets an error code. The error code can be tested by the commands IF-ERROR",  /* Description */
        mon_400B_ErrorReturn,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        257,           /* MON number (decimal) */
        "401B",         /* Octal string */
        "DIASS",    /* Short name */
        "DisAssemble",          /* Long name */
        "Disassembles one machine instruction on the ND-500. Output is the instruction in ASSEMBLY-500 langua",  /* Description */
        mon_401B_DisAssemble,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        258,           /* MON number (decimal) */
        "402B",         /* Octal string */
        "RFLAG",    /* Short name */
        "GetInputFlags",          /* Long name */
        "ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as ",  /* Description */
        mon_402B_GetInputFlags,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        259,           /* MON number (decimal) */
        "403B",         /* Octal string */
        "WFLAG",    /* Short name */
        "SetOutputFlags",          /* Long name */
        "ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as ",  /* Description */
        mon_403B_SetOutputFlags,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        260,           /* MON number (decimal) */
        "404B",         /* Octal string */
        "IOFIX",    /* Short name */
        "FixIOArea",          /* Long name */
        "Fixes an address area in a domain in physical memory. The memory area can be used for later input an",  /* Description */
        mon_404B_FixIOArea,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        261,           /* MON number (decimal) */
        "405B",         /* Octal string */
        "USTRK",    /* Short name */
        "SwitchUserBreak",          /* Long name */
        "Switches user-defined escape handling on and off. The user-defined escape handling transfers control",  /* Description */
        mon_405B_SwitchUserBreak,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        262,           /* MON number (decimal) */
        "406B",         /* Octal string */
        "RWRTC",    /* Short name */
        "AccessRTCommon",          /* Long name */
        "Reads from or writes to RT common from an ND-500 program. RT common is an area in physical memory wh",  /* Description */
        mon_406B_AccessRTCommon,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        32,           /* MON number (decimal) */
        "40B",         /* Octal string */
        "SPCLO",    /* Short name */
        "CloseSpoolingFile",          /* Long name */
        "Appends an opened file to a spooling queue. You specify a text to be printed on the error device whe",  /* Description */
        mon_40B_CloseSpoolingFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        264,           /* MON number (decimal) */
        "410B",         /* Octal string */
        "FIXMEM",    /* Short name */
        "FixInMemory",          /* Long name */
        "Fixes a logical segment (either whole or in part) of a user's domain in physical memory. This action",  /* Description */
        mon_410B_FixInMemory,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        265,           /* MON number (decimal) */
        "411B",         /* Octal string */
        "UNFIXM",    /* Short name */
        "MemoryUnfix",          /* Long name */
        "Releases a fixed segment in your domain from physical memory. A fixed segment has all its pages fixe",  /* Description */
        mon_411B_MemoryUnfix,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        266,           /* MON number (decimal) */
        "412B",         /* Octal string */
        "FSCNT",    /* Short name */
        "FileAsSegment",          /* Long name */
        "Connects a file as a segment to your domain. You can then access the file as a logical segment. This",  /* Description */
        mon_412B_FileAsSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        267,           /* MON number (decimal) */
        "413B",         /* Octal string */
        "FSCDNT",    /* Short name */
        "FileNotAsSegment",          /* Long name */
        "Disconnects a file as a segment in your domain. FileAsSegment allows files to be accessed as segment",  /* Description */
        mon_413B_FileNotAsSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        268,           /* MON number (decimal) */
        "414B",         /* Octal string */
        "BCNAF",    /* Short name */
        "BCNAFCAMAC",          /* Long name */
        "Special CAMAC function on the ND-500. (Same as mon 156 TRACB.)",  /* Description */
        mon_414B_BCNAFCAMAC,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        269,           /* MON number (decimal) */
        "415B",         /* Octal string */
        "BCNAF1",    /* Short name */
        "BCNAF1CAMAC",          /* Long name */
        "Special CAMAC monitor call for the ND-500. (Same as mon 176 - user-defined monitor call.)",  /* Description */
        mon_415B_BCNAF1CAMAC,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        270,           /* MON number (decimal) */
        "416B",         /* Octal string */
        "WSEGN",    /* Short name */
        "SaveND500Segment",          /* Long name */
        "Writes all modified pages of a segment back to the disk.\n\n- Not allowed when fixed in memory.",  /* Description */
        mon_416B_SaveND500Segment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        271,           /* MON number (decimal) */
        "417B",         /* Octal string */
        "MXPISG",    /* Short name */
        "MaxPagesInMemory",          /* Long name */
        "Sets the maximum number of pages a segment may have in physical memory at a time.\n\n- This monitor ca",  /* Description */
        mon_417B_MaxPagesInMemory,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        33,           /* MON number (decimal) */
        "41B",         /* Octal string */
        "ROBJE",    /* Short name */
        "ReadObjectEntry",          /* Long name */
        "Gets information about an opened file. An object entry describes each file. It contains the file nam",  /* Description */
        mon_41B_ReadObjectEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        272,           /* MON number (decimal) */
        "420B",         /* Octal string */
        "GRBLK",    /* Short name */
        "GetUserRegisters",          /* Long name */
        "SwitchUserBreak allows you to save the registers when you terminate an ND-500 program with the ESCAP",  /* Description */
        mon_420B_GetUserRegisters,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        273,           /* MON number (decimal) */
        "421B",         /* Octal string */
        "GASGM",    /* Short name */
        "GetActiveSegment",          /* Long name */
        "Gets the name of the segments in your domain. A 2048 byte buffer is returned. It contains 32 pointer",  /* Description */
        mon_421B_GetActiveSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        274,           /* MON number (decimal) */
        "422B",         /* Octal string */
        "GSWSP",    /* Short name */
        "GetScratchSegment",          /* Long name */
        "Connects an empty data segment to the user's domain and reserves space for it on the swap file. The ",  /* Description */
        mon_422B_GetScratchSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        275,           /* MON number (decimal) */
        "423B",         /* Octal string */
        "CAPCOP",    /* Short name */
        "CopyCapability",          /* Long name */
        "Copies a capability for a segment. The segment itself is also copied. A capability describes each lo",  /* Description */
        mon_423B_CopyCapability,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        276,           /* MON number (decimal) */
        "424B",         /* Octal string */
        "CAPCLE",    /* Short name */
        "ClearCapability",          /* Long name */
        "Clears a capability. A capability describes each logical segment in a domain. The protection of the ",  /* Description */
        mon_424B_ClearCapability,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        277,           /* MON number (decimal) */
        "425B",         /* Octal string */
        "SPRNAM",    /* Short name */
        "SetProcessName",          /* Long name */
        "Defines a new name for your process.\n\n- Process names may be up to 16 characters and contain an addi",  /* Description */
        mon_425B_SetProcessName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        278,           /* MON number (decimal) */
        "426B",         /* Octal string */
        "GPRNAM",    /* Short name */
        "GetProcessNo",          /* Long name */
        "Gets the number of a process in the ND-500. You specify the process name. The process number is assi",  /* Description */
        mon_426B_GetProcessNo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        279,           /* MON number (decimal) */
        "427B",         /* Octal string */
        "GPRNME",    /* Short name */
        "GetOwnProcessInfo",          /* Long name */
        "Gets the name and number of your own process in the ND-500. You get a process each time you enter th",  /* Description */
        mon_427B_GetOwnProcessInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        280,           /* MON number (decimal) */
        "430B",         /* Octal string */
        "ADR100",    /* Short name */
        "TranslateAddress",          /* Long name */
        "Translates an ND-500 logical address to an ND-100 physical address. Use this monitor call to set up ",  /* Description */
        mon_430B_TranslateAddress,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        281,           /* MON number (decimal) */
        "431B",         /* Octal string */
        "MWAITF",    /* Short name */
        "AwaitTransfer",          /* Long name */
        "Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevan",  /* Description */
        mon_431B_AwaitTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        285,           /* MON number (decimal) */
        "435B",         /* Octal string */
        "PRT",    /* Short name */
        "ForceTrap",          /* Long name */
        "Forces a programmed trap to occur in another ND-500 process. The trap handler in this process is sta",  /* Description */
        mon_435B_ForceTrap,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        286,           /* MON number (decimal) */
        "436B",         /* Octal string */
        "5PASET",    /* Short name */
        "SetND500Param",          /* Long name */
        "Sets information about an ND-500 program. Use GetND500Param to read the 5 parameters when a program ",  /* Description */
        mon_436B_SetND500Param,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        287,           /* MON number (decimal) */
        "437B",         /* Octal string */
        "5PAGET",    /* Short name */
        "GetND500Param",          /* Long name */
        "Gets information about why the last ND-500 program terminated. There are five parameters for each ba",  /* Description */
        mon_437B_GetND500Param,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        35,           /* MON number (decimal) */
        "43B",         /* Octal string */
        "CLOSE",    /* Short name */
        "CloseFile",          /* Long name */
        "Closes one or more files. Files must be opened before they are accessed. Afterwards they should be c",  /* Description */
        mon_43B_CloseFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        288,           /* MON number (decimal) */
        "440B",         /* Octal string */
        "AT5SGM",    /* Short name */
        "Attach500Segment",          /* Long name */
        "Maps a logical ND-500 data segment onto shared ND-100/ND-500(0) physical memory (multiport memory).",  /* Description */
        mon_440B_Attach500Segment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        11             /* Param count */
    );
    mon_register(
        36,           /* MON number (decimal) */
        "44B",         /* Octal string */
        "RUSER",    /* Short name */
        "GetUserEntry",          /* Long name */
        "Gets information about a user. The user entry in the directory is returned. It contains the user nam",  /* Description */
        mon_44B_GetUserEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        4,           /* MON number (decimal) */
        "4B",         /* Octal string */
        "BRKM",    /* Short name */
        "SetBreak",          /* Long name */
        "Sets the break characters for a terminal. Normally, a program waits for input. When a break characte",  /* Description */
        mon_4B_SetBreak,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        320,           /* MON number (decimal) */
        "500B",         /* Octal string */
        "STARTP",    /* Short name */
        "StartProcess",          /* Long name */
        "Starts a process in the ND-500. You identify the process with the process number.",  /* Description */
        mon_500B_StartProcess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        321,           /* MON number (decimal) */
        "501B",         /* Octal string */
        "STOPPR",    /* Short name */
        "StopProcess",          /* Long name */
        "Sets the current process in a wait state. StartProcess restarts the process. Execution continues aft",  /* Description */
        mon_501B_StopProcess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        0             /* Param count */
    );
    mon_register(
        322,           /* MON number (decimal) */
        "502B",         /* Octal string */
        "SWITCHP",    /* Short name */
        "SwitchProcess",          /* Long name */
        "Sets the current process in a wait state. Restarts another process. This is similar to executing a S",  /* Description */
        mon_502B_SwitchProcess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        323,           /* MON number (decimal) */
        "503B",         /* Octal string */
        "DVINST",    /* Short name */
        "InputString",          /* Long name */
        "Reads a string from a device, e.g. a terminal or an opened file. This monitor call provide a fast in",  /* Description */
        mon_503B_InputString,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        14             /* Param count */
    );
    mon_register(
        324,           /* MON number (decimal) */
        "504B",         /* Octal string */
        "DVOUTS",    /* Short name */
        "OutputString",          /* Long name */
        "Writes a string to a device, e.g. a terminal or an opened file.\n\n- This is the most efficient way to",  /* Description */
        mon_504B_OutputString,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        325,           /* MON number (decimal) */
        "505B",         /* Octal string */
        "GERRCOD",    /* Short name */
        "GetTrapReason",          /* Long name */
        "Gets the error code from the swapper process. This is only relevant to programmed trap handlers. The",  /* Description */
        mon_505B_GetTrapReason,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        327,           /* MON number (decimal) */
        "507B",         /* Octal string */
        "SPRIO",    /* Short name */
        "SetProcessPriority",          /* Long name */
        "Sets the priority for a process in the ND-500. The priorities vary from 0 to 255. The process with t",  /* Description */
        mon_507B_SetProcessPriority,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        40,           /* MON number (decimal) */
        "50B",         /* Octal string */
        "OPEN",    /* Short name */
        "OpenFile",          /* Long name */
        "Opens a file. You cannot access a file before you open it. Specify what kind of access you want, e.g",  /* Description */
        mon_50B_OpenFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register(
        332,           /* MON number (decimal) */
        "514B",         /* Octal string */
        "5TMOUT",    /* Short name */
        "ND500TimeOut",          /* Long name */
        "Suspends the execution of an ND-500 program for a given time. The execution then continues after the",  /* Description */
        mon_514B_ND500TimeOut,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        42,           /* MON number (decimal) */
        "52B",         /* Octal string */
        "TERMO",    /* Short name */
        "TerminalMode",          /* Long name */
        "Selects various terminal functions. You may stop output on full page. Input may be converted to uppe",  /* Description */
        mon_52B_TerminalMode,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        43,           /* MON number (decimal) */
        "53B",         /* Octal string */
        "RSEGM",    /* Short name */
        "GetSegmentEntry",          /* Long name */
        "Gets information about a segment in the ND-100. The monitor call returns the segment entry. You spec",  /* Description */
        mon_53B_GetSegmentEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        44,           /* MON number (decimal) */
        "54B",         /* Octal string */
        "MDLFI",    /* Short name */
        "DeleteFile",          /* Long name */
        "Deletes a file. The pages of the file are released.\n\n- You must have directory access to the file in",  /* Description */
        mon_54B_DeleteFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        45,           /* MON number (decimal) */
        "55B",         /* Octal string */
        "RSQPE",    /* Short name */
        "GetSpoolingEntry",          /* Long name */
        "Gets the next spooling queue entry, that is, the next file to be printed. The entry is removed from ",  /* Description */
        mon_55B_GetSpoolingEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        46,           /* MON number (decimal) */
        "56B",         /* Octal string */
        "PASET",    /* Short name */
        "SetUserParam",          /* Long name */
        "Sets information about a background program. Use GetUserParam to read the 5 parameters when a progra",  /* Description */
        mon_56B_SetUserParam,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        47,           /* MON number (decimal) */
        "57B",         /* Octal string */
        "PAGEI",    /* Short name */
        "GetUserParam",          /* Long name */
        "Gets information about why the last program terminated. There are 5 parameters for each background u",  /* Description */
        mon_57B_GetUserParam,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        5,           /* MON number (decimal) */
        "5B",         /* Octal string */
        "RDISK",    /* Short name */
        "ReadScratchFile",          /* Long name */
        "Reads randomly from the scratch file. One block is transferred. There is one scratch file connected ",  /* Description */
        mon_5B_ReadScratchFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        49,           /* MON number (decimal) */
        "61B",         /* Octal string */
        "FIXC5",    /* Short name */
        "MemoryAllocation",          /* Long name */
        "Fixes or unfixes ND-100 segments to be used by the ND-500 Monitor. You may also reserve a contiguous",  /* Description */
        mon_61B_MemoryAllocation,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register(
        50,           /* MON number (decimal) */
        "62B",         /* Octal string */
        "RMAX",    /* Short name */
        "GetBytesInFile",          /* Long name */
        "Gets the number of bytes in a file. Only the bytes containing data are counted.",  /* Description */
        mon_62B_GetBytesInFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        51,           /* MON number (decimal) */
        "63B",         /* Octal string */
        "B41NW",    /* Short name */
        "In4x2Bytes",          /* Long name */
        "Reads 8 bytes from a word-oriented or character-oriented device, e.g. internal devices.\n\n- Do not us",  /* Description */
        mon_63B_In4x2Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register(
        52,           /* MON number (decimal) */
        "64B",         /* Octal string */
        "ERMSG",    /* Short name */
        "WarningMessage",          /* Long name */
        "Outputs a file system error message. Appendix A shows the messages connected to each error code. The",  /* Description */
        mon_64B_WarningMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        53,           /* MON number (decimal) */
        "65B",         /* Octal string */
        "QERMS",    /* Short name */
        "ErrorMessage",          /* Long name */
        "Displays a file system error message. Appendix A shows the messages connected to each error number. ",  /* Description */
        mon_65B_ErrorMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        54,           /* MON number (decimal) */
        "66B",         /* Octal string */
        "ISIZE",    /* Short name */
        "InBufferSpace",          /* Long name */
        "Gets the current number of bytes in the input buffer. Terminals and other character devices place in",  /* Description */
        mon_66B_InBufferSpace,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        55,           /* MON number (decimal) */
        "67B",         /* Octal string */
        "OSIZE",    /* Short name */
        "OutBufferSpace",          /* Long name */
        "Gets the number of free bytes in the output buffer (number of bytes which can be written before the ",  /* Description */
        mon_67B_OutBufferSpace,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        6,           /* MON number (decimal) */
        "6B",         /* Octal string */
        "WDISK",    /* Short name */
        "WriteScratchFile",          /* Long name */
        "Writes randomly to the scratch file. One block is transferred. There is one scratch file connected t",  /* Description */
        mon_6B_WriteScratchFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        56,           /* MON number (decimal) */
        "70B",         /* Octal string */
        "COMMND",    /* Short name */
        "CallCommand",          /* Long name */
        "Executes a SINTRAN III command from a program. The program terminates if an error occurs in the comm",  /* Description */
        mon_70B_CallCommand,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        57,           /* MON number (decimal) */
        "71B",         /* Octal string */
        "DESCF",    /* Short name */
        "DisableEscape",          /* Long name */
        "The ESCAPE key on the terminal normally terminates a program. This is called user break. This monito",  /* Description */
        mon_71B_DisableEscape,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        58,           /* MON number (decimal) */
        "72B",         /* Octal string */
        "EESCF",    /* Short name */
        "EnableEscape",          /* Long name */
        "Enables the ESCAPE key on the terminal. The ESCAPE key normally terminates a program. This is called",  /* Description */
        mon_72B_EnableEscape,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register(
        59,           /* MON number (decimal) */
        "73B",         /* Octal string */
        "SMAX",    /* Short name */
        "SetMaxBytes",          /* Long name */
        "Sets the value of the maximum byte pointer in an opened file (i.e. the number of bytes minus 1). The",  /* Description */
        mon_73B_SetMaxBytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        60,           /* MON number (decimal) */
        "74B",         /* Octal string */
        "SETBT",    /* Short name */
        "SetStartByte",          /* Long name */
        "Sets the next byte to be read or written in an opened mass-storage file.\n\n- The bytes in a file are ",  /* Description */
        mon_74B_SetStartByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        61,           /* MON number (decimal) */
        "75B",         /* Octal string */
        "REABT",    /* Short name */
        "GetStartByte",          /* Long name */
        "Gets the number of the next byte to access in a file. The bytes in a file are numbered from 0.\n\n- Th",  /* Description */
        mon_75B_GetStartByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        62,           /* MON number (decimal) */
        "76B",         /* Octal string */
        "SETBS",    /* Short name */
        "SetBlockSize",          /* Long name */
        "Sets the block size of an opened file. Monitor calls which read randomly from, or write randomly to ",  /* Description */
        mon_76B_SetBlockSize,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        63,           /* MON number (decimal) */
        "77B",         /* Octal string */
        "SETBL",    /* Short name */
        "SetStartBlock",          /* Long name */
        "Sets the next block to be read or written in an opened file. You may access the first bytes in the b",  /* Description */
        mon_77B_SetStartBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register(
        7,           /* MON number (decimal) */
        "7B",         /* Octal string */
        "RPAGE",    /* Short name */
        "ReadBlock",          /* Long name */
        "Reads randomly from a file. You read one block at a time. The file must be opened for random read ac",  /* Description */
        mon_7B_ReadBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
}
