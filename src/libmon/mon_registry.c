/*
 * MON Handler Registry
 *
 * Initially auto-generated, now MANUALLY MAINTAINED.
 * Update status field when implementing MON calls:
 *   MON_STATUS_NOT_IMPLEMENTED - stub only
 *   MON_STATUS_IN_PROGRESS     - partially implemented
 *   MON_STATUS_VALIDATED       - fully tested and working
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
extern MonResult mon_321B_UEAdministrator(MonContext* ctx);
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
    mon_register_ex(
        64,           /* MON number (decimal) */
        "100B",         /* Octal string */
        "RT",    /* Short name */
        "StartRTProgram",          /* Long name */
        "Starts an RT program. The program is moved to the execution queue. It is executed according to its p",  /* Description */
        "[I] RTProgram (INTEGER): Address of RT description. Use 0 for your own RT description address.",  /* Parameter details */
        mon_100B_StartRTProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        65,           /* MON number (decimal) */
        "101B",         /* Octal string */
        "SET",    /* Short name */
        "DelayStart",          /* Long name */
        "Starts an RT program after a specified time. The RT program is put in the time queue. It is moved to",  /* Description */
        "[I] RTProgram (INTEGER2): Address of the RT description. Use 0 for the calling program.\\n[I] TimeUnits (INTEGER2): The number of time units to stay in the time queue.\\n[I] UnitType (INTEGER2): Time unit type. 1=basic units (1/50s), 2=seconds, 3=minutes, 4=hours.",  /* Parameter details */
        mon_101B_DelayStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        66,           /* MON number (decimal) */
        "102B",         /* Octal string */
        "ABSET",    /* Short name */
        "StartupTime",          /* Long name */
        "Starts an RT program at a specified time of the day. The RT program is then put in the time queue. I",  /* Description */
        "[I] RTProgram (INTEGER): Address of the RT description. Use 0 for the calling program.\\n[I] Seconds (INTEGER): Seconds.\\n[I] Minutes (INTEGER): Minutes.\\n[I] Hours (INTEGER): Hours.",  /* Parameter details */
        mon_102B_StartupTime,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        67,           /* MON number (decimal) */
        "103B",         /* Octal string */
        "INTV",    /* Short name */
        "StartupInterval",          /* Long name */
        "Prepares an RT program for periodic execution. The interval between the executions can be specified ",  /* Description */
        "[I] RTProgram (INTEGER): Address of RT description. 0 means calling program. GetRtAddress gives RT description addresses.\\n[I] Time (INTEGER): Number of time units between executions of the program.\\n[I] Units (INTEGER): Type of time units:\n1 = basic time units (1/50th second)\n2 = seconds\n3 = minutes\n4 = hours",  /* Parameter details */
        mon_103B_StartupInterval,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        68,           /* MON number (decimal) */
        "104B",         /* Octal string */
        "HOLD",    /* Short name */
        "SuspendProgram",          /* Long name */
        "Suspends the execution of your program for a given time. The execution then continues after the time",  /* Description */
        "[I] TimeUnits (INTEGER): Number of time units to suspend the program. Should not be zero.\\n[I] UnitType (INTEGER): Type of time units:\n1 = basic time units (1/50th second)\n2 = seconds\n3 = minutes\n4 = hours",  /* Parameter details */
        mon_104B_SuspendProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        69,           /* MON number (decimal) */
        "105B",         /* Octal string */
        "ABORT",    /* Short name */
        "StopRTProgram",          /* Long name */
        "Stops an RT program. It is removed from the time or execution queue. All reserved devices and files ",  /* Description */
        "[I] RTProgram (INTEGER): Address of RT description. 0 means calling program.",  /* Parameter details */
        mon_105B_StopRTProgram,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        70,           /* MON number (decimal) */
        "106B",         /* Octal string */
        "CONCT",    /* Short name */
        "StartOnInterrupt",          /* Long name */
        "StartOnInterrupt connects an RT program to interrupts from a device. The RT program starts when an i",  /* Description */
        "[I] RTProgram (INTEGER2): Address of RT description. Use 0 for your own RT description address.\\n[I] DeviceNumber (INTEGER2): Logical device number. See appendix B.",  /* Parameter details */
        mon_106B_StartOnInterrupt,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        71,           /* MON number (decimal) */
        "107B",         /* Octal string */
        "DSCNT",    /* Short name */
        "NoInterruptStart",          /* Long name */
        "StartOnInterrupt connects an RT program to interrupts from a device. You remove this connection with",  /* Description */
        "[I] RTProgram (INTEGER): Address of RT description.",  /* Parameter details */
        mon_107B_NoInterruptStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        8,           /* MON number (decimal) */
        "10B",         /* Octal string */
        "WPAGE",    /* Short name */
        "WriteBlock",          /* Long name */
        "Writes randomly to a file. You write one block at a time. The file must be opened for random write a",  /* Description */
        "[I] FileNumber (INTEGER2): File number. See OpenFile.\\n[I] BlockNumber (INTEGER2): Block number.\\n[I] Buffer (ARRAY): Transferred block (data to be written).\\n[O] ErrCode (INTEGER2): Standard Error Code. See appendix A.",  /* Parameter details */
        mon_10B_WriteBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        72,           /* MON number (decimal) */
        "110B",         /* Octal string */
        "PRIOR",    /* Short name */
        "SetRTPriority",          /* Long name */
        "Sets the priority of an RT program. RT programs may be given priorities from 0 to 255. SINTRAN III e",  /* Description */
        "[I] RTProgram (INTEGER): Address of the RT description. You may use 0 for your own program.\\n[I] PriorityLevel (INTEGER): Priority (0-255).\\n[O] OldPriority (INTEGER): The previous priority returned in W1.",  /* Parameter details */
        mon_110B_SetRTPriority,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        73,           /* MON number (decimal) */
        "111B",         /* Octal string */
        "UPDAT",    /* Short name */
        "SetClock",          /* Long name */
        "Gives new values to the computer's clock and calendar. If the computer panel has a clock, it is upda",  /* Description */
        "[I] Minute (INTEGER): Minutes (0-59).\\n[I] Hour (INTEGER): Hours (0-23).\\n[I] Day (INTEGER): Day of month (1-31).\\n[I] Month (INTEGER): Month (1-12).\\n[I] Year (INTEGER): Year.",  /* Parameter details */
        mon_111B_SetClock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        74,           /* MON number (decimal) */
        "112B",         /* Octal string */
        "CLADJ",    /* Short name */
        "AdjustClock",          /* Long name */
        "Sets the computer's clock (i.e. the current system time) forward or back. If the operator panel has ",  /* Description */
        "[I] TimeUnits (INTEGER2): Number of time units to adjust the clock by. Negative values make the clock halt for that time.\\n[I] UnitType (INTEGER2): Time unit type. 1=basic units (1/50s), 2=seconds, 3=minutes, 4=hours.",  /* Parameter details */
        mon_112B_AdjustClock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        75,           /* MON number (decimal) */
        "113B",         /* Octal string */
        "CLOCK",    /* Short name */
        "GetCurrentTime",          /* Long name */
        "Gets the current system time and date.\n\n- The current system time is returned as basic time units, s",  /* Description */
        "[O] TimeBuffer (ARRAY): Buffer to receive 7 integers containing:\n[0] = basic time units (1/50th second)\n[1] = seconds\n[2] = minutes\n[3] = hours\n[4] = day\n[5] = month\n[6] = year",  /* Parameter details */
        mon_113B_GetCurrentTime,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        76,           /* MON number (decimal) */
        "114B",         /* Octal string */
        "TUSED",    /* Short name */
        "GetTimeUsed",          /* Long name */
        "Gets the time you have used the CPU since you logged in. In batch jobs, you get the time since you entered the job. CPU time is in basic time units (1/50th second). Can also be used from RT-programs.",  /* Description */
        "[O] TimeUsed (LONGINT): CPU time used in basic time units (1/50s). Output returned in W1 register.",  /* Parameter details */
        mon_114B_GetTimeUsed,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        77,           /* MON number (decimal) */
        "115B",         /* Octal string */
        "FIX",    /* Short name */
        "FixScattered",          /* Long name */
        "Place a segment in physical memory. Its pages will no longer be swapped to the disk. The segment mus",  /* Description */
        "[I] SegmentNumber (INTEGER2): Segment number to be fixed in memory. Must be a non-demand segment.",  /* Parameter details */
        mon_115B_FixScattered,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        78,           /* MON number (decimal) */
        "116B",         /* Octal string */
        "UNFIX",    /* Short name */
        "UnfixSegment",          /* Long name */
        "Releases a fixed segment and removes it from the Page Index Table (PIT). Its pages may then be swapp",  /* Description */
        "[I] SegmentNumber (INTEGER): Segment number to be released.",  /* Parameter details */
        mon_116B_UnfixSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        79,           /* MON number (decimal) */
        "117B",         /* Octal string */
        "RFILE",    /* Short name */
        "ReadFromFile",          /* Long name */
        "Reads any number of bytes from a file. The read operation must start at the beginning of a block. Th",  /* Description */
        "[I] FileNo (INTEGER2): File number. See OpenFile.\\n[I] WaitFlag (INTEGER2): Wait flag. 0=suspend until complete, non-zero=continue (use AwaitFileTransfer to check).\\n[O] Buff (BYTES): Buffer to receive transferred data (output). Must start on even byte address.\\n[I] BlockNo (INTEGER2): Block number to start reading from. Use -1 to read the next block.\\n[I] NoOfBytes (LONGINT): Number of bytes to read.",  /* Parameter details */
        mon_117B_ReadFromFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        9,           /* MON number (decimal) */
        "11B",         /* Octal string */
        "TIME",    /* Short name */
        "GetBasicTime",          /* Long name */
        "**Time**\n\nGets the current internal time. The internal time is specified in basic time units. There ",  /* Description */
        "[O] BasicTime (LONGINT): Internal time in basic time units (output). 50 units per second.",  /* Parameter details */
        mon_11B_GetBasicTime,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        80,           /* MON number (decimal) */
        "120B",         /* Octal string */
        "WFILE",    /* Short name */
        "WriteToFile",          /* Long name */
        "Writes any number of bytes to a file. The read operation must start at the beginning of a block. The",  /* Description */
        "[I] FileNo (INTEGER2): File number. See OpenFile.\\n[I] ReturnFlag (INTEGER2): Wait flag. 0=suspend until complete, non-zero=continue (use AwaitFileTransfer to check).\\n[I] Buff (BYTES): Buffer containing data to be transferred.\\n[I] BlockNo (INTEGER2): Block number to start writing from. Use -1 to write to the next block.\\n[I] NoOfBytes (LONGINT): Number of bytes to be written.",  /* Parameter details */
        mon_120B_WriteToFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        81,           /* MON number (decimal) */
        "121B",         /* Octal string */
        "WAITF",    /* Short name */
        "AwaitFileTransfer",          /* Long name */
        "Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevan",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[I] ReturnFlag (INTEGER): Wait flag. 0=wait until transfer complete. Other values=return immediately with status.\\n[O] Status (INTEGER): State of transfer:\n0 = transfer finished\n-1 = transfer not finished\n> 0 = error code (see appendix A)",  /* Parameter details */
        mon_121B_AwaitFileTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        82,           /* MON number (decimal) */
        "122B",         /* Octal string */
        "RESRV",    /* Short name */
        "ReserveResource",          /* Long name */
        "Reserves a device or file for your program only. You release it with ReleaseResource. Some devices, ",  /* Description */
        "[I] DeviceNo (INTEGER2): Logical device number. See appendix B.\\n[I] IOFlag (INTEGER2): Input/output flag. 0=input part, 1=output part.\\n[I] WaitFlag (INTEGER2): Wait flag. 0=wait if reserved, 1=return status value.\\n[O] Status (INTEGER2): Return status (output). Only used if WaitFlag=1. Negative=already reserved.",  /* Parameter details */
        mon_122B_ReserveResource,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        83,           /* MON number (decimal) */
        "123B",         /* Octal string */
        "RELES",    /* Short name */
        "ReleaseResource",          /* Long name */
        "Releases a reserved device or file. The resource can then be used by another program. You reserve a ",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[I] IOFlag (INTEGER): Input or output flag. Use 0 for the input part and 1 for the output part.",  /* Parameter details */
        mon_123B_ReleaseResource,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        84,           /* MON number (decimal) */
        "124B",         /* Octal string */
        "PRSRV",    /* Short name */
        "ForceReserve",          /* Long name */
        "Reserves a device for an RT program other than that which is calling. Use ForceRelease if the device",  /* Description */
        "[I] DeviceNo (INTEGER2): Logical device number. See appendix B.\\n[I] IOFlag (INTEGER2): Input/output flag. 0=input part, 1=output part.\\n[I] RTProgram (INTEGER2): RT description address of the RT program to reserve the device. Use 0 for your own program.",  /* Parameter details */
        mon_124B_ForceReserve,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        85,           /* MON number (decimal) */
        "125B",         /* Octal string */
        "PRLRS",    /* Short name */
        "ForceRelease",          /* Long name */
        "Releases a device reserved by an RT program other than that which is calling. You can then reserve t",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[I] IOFlag (INTEGER): Input or output flag. Use 0 for the input part and 1 for the output part.",  /* Parameter details */
        mon_125B_ForceRelease,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        86,           /* MON number (decimal) */
        "126B",         /* Octal string */
        "DSET",    /* Short name */
        "ExactDelayStart",          /* Long name */
        "Sets an RT program to start after a given period. It is then moved from the time queue to the execut",  /* Description */
        "[I] RTProgram (INTEGER): Address of the RT description. Use 0 for the calling program.\\n[I] BasicTimeUnits (INTEGER): The number of basic time units (1/50th second) to stay in the time queue.",  /* Parameter details */
        mon_126B_ExactDelayStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        87,           /* MON number (decimal) */
        "127B",         /* Octal string */
        "DABST",    /* Short name */
        "ExactStartup",          /* Long name */
        "Starts an RT program at a specific time. The time is given in basic time units. A basic time unit is",  /* Description */
        "[I] RTProgram (INTEGER2): Address of an RT description. 0 means calling program. GetRtAddress gives RT description addresses.\\n[I] BasicTimeUnits (LONGINT): Startup time in basic time units (1/50th of a second).",  /* Parameter details */
        mon_127B_ExactStartup,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        10,           /* MON number (decimal) */
        "12B",         /* Octal string */
        "SETCM",    /* Short name */
        "SetCommandBuffer",          /* Long name */
        "Transfers a string to the command buffer. The command buffer contains the last command input from th",  /* Description */
        "[I] Command (STRING): String to transfer to the command buffer (up to 32 characters).",  /* Parameter details */
        mon_12B_SetCommandBuffer,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        88,           /* MON number (decimal) */
        "130B",         /* Octal string */
        "DINTV",    /* Short name */
        "ExactInterval",          /* Long name */
        "Prepares an RT program for periodic execution. The interval between the executions may be from 1 to ",  /* Description */
        "[I] RTProgram (INTEGER): Address of an RT description. 0 means calling program. GetRtAddress gives RT description addresses.\\n[I] BasicTimeUnits (INTEGER): Period between executions in basic time units (1/50th second).",  /* Parameter details */
        mon_130B_ExactInterval,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        89,           /* MON number (decimal) */
        "131B",         /* Octal string */
        "ABSTR",    /* Short name */
        "DataTransfer",          /* Long name */
        "Transfers data between physical memory and a mass-storage device, e.g. a disk or magnetic tape. You ",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. See appendix B.\\n[I] Func (INTEGER): Function code. See the tables on the following pages.\\n[I] MemoryAddr (INTEGER): Physical memory address (32-bit).\\n[I] BlockAddr (INTEGER): Block address on the disk. See the tables on the following pages.\\n[I] NoOfBlocks (INTEGER): Number of blocks to transfer.\\n[O] Stat (INTEGER): Error code returned in W1. Negative if error. Contains a hardware status.",  /* Parameter details */
        mon_131B_DataTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        90,           /* MON number (decimal) */
        "132B",         /* Octal string */
        "MCALL",    /* Short name */
        "JumpToSegment",          /* Long name */
        "Calls a routine on another segment in the ND-100. You can divide an ND-100 RT program between variou",  /* Description */
        "[I] SubroutineAddr (INTEGER): Address of routine.\\n[I] NewSegment (INTEGER): New segments. The most significant byte identifies the first segment. The least significant byte identifies the second segment. Use 377B as segment number if you do not want to change it.",  /* Parameter details */
        mon_132B_JumpToSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        91,           /* MON number (decimal) */
        "133B",         /* Octal string */
        "MEXIT",    /* Short name */
        "ExitFromSegment",          /* Long name */
        "Exchanges one or both current segments. Commonly used to return after the monitor call JumpToSegment",  /* Description */
        "[I] SegmentNumber (INTEGER): Segment number to return to (8-bit, values 0-255).",  /* Parameter details */
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
    mon_register_ex(
        94,           /* MON number (decimal) */
        "136B",         /* Octal string */
        "RTON",    /* Short name */
        "EnableRTStart",          /* Long name */
        "RTON RT programs cannot be started after DisableRTStart has been executed. Use EnableRTStart to do t",  /* Description */
        "[I] RTProgram (INTEGER): Address of the RT description. 0 means calling program.",  /* Parameter details */
        mon_136B_EnableRTStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        95,           /* MON number (decimal) */
        "137B",         /* Octal string */
        "RTOFF",    /* Short name */
        "DisableRTStart",          /* Long name */
        "Disables start of RT programs. No RT program can be started before EnableRTStart is executed.\n\n- RT ",  /* Description */
        "[I] RTProgram (INTEGER2): Address of the RT description. 0 means calling program.",  /* Parameter details */
        mon_137B_DisableRTStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        11,           /* MON number (decimal) */
        "13B",         /* Octal string */
        "CIBUF",    /* Short name */
        "ClearInBuffer",          /* Long name */
        "Clears a device input buffer. Input from character devices, e.g. terminals, are temporarily stored i",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.",  /* Parameter details */
        mon_13B_ClearInBuffer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        96,           /* MON number (decimal) */
        "140B",         /* Octal string */
        "WHDEV",    /* Short name */
        "ReservationInfo",          /* Long name */
        "Checks that a device is not reserved. If it is reserved, you will receive information about which RT",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number. See appendix B.\\n[I] IOFlag (INTEGER2): Input/output flag. 0=input part, 1=output part.\\n[O] ReturnValue (INTEGER2): RT description address of reserving RT program (output). 0=not reserved, -1=error.",  /* Parameter details */
        mon_140B_ReservationInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        97,           /* MON number (decimal) */
        "141B",         /* Octal string */
        "IOSET",    /* Short name */
        "DeviceControl",          /* Long name */
        "Sets control information for a character device, e.g. a terminal or a printer. The control informati",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. See appendix B. You cannot use 1 for your own terminal. Use ExecutionInfo to get its logical device number instead.\\n[I] IOFlag (INTEGER): Input or output part of the device. Use 0 for input and 1 for output.\\n[I] RTProgram (INTEGER): Address of RT description of reserving program. Use 0 for the calling program.\\n[I] CtrlFlag (INTEGER): Control flag. -2=empty TAD output buffer, -1=reset device (ASCII mode), 0=ASCII mode, 1=binary mode.\\n[O] ReturnStatus (INTEGER): Return status in W1. 0 means no errors. An illegal RT description address returns -1.",  /* Parameter details */
        mon_141B_DeviceControl,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        98,           /* MON number (decimal) */
        "142B",         /* Octal string */
        "ERMON",    /* Short name */
        "ToErrorDevice",          /* Long name */
        "Outputs a user-defined, real-time error. The error message is output on the error device, i.e. norma",  /* Description */
        "[I] ErrorNumber (INTEGER): Error number (50-69). This number is output following ERROR.\\n[I] SubErrorNumber (INTEGER): Suberror number.",  /* Parameter details */
        mon_142B_ToErrorDevice,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        99,           /* MON number (decimal) */
        "143B",         /* Octal string */
        "RSIO",    /* Short name */
        "ExecutionInfo",          /* Long name */
        "Gets information about the execution of the calling program. You are told whether the program execut",  /* Description */
        "[O] ExecutionMode (INTEGER): Execution mode:\n0 = interactive program\n1 = batch job\n2 = mode job\n3 = RT program\\n[O] InputDev (INTEGER): Logical device number for command input. Terminal number for interactive, file number for batch/mode.\\n[O] OutputDev (INTEGER): Logical device number for command output. Terminal number for interactive, file number for batch/mode.\\n[O] UserIndex (INTEGER): Directory and user index of program's owner. Bits 8-15=directory index, bits 0-7=user index.",  /* Parameter details */
        mon_143B_ExecutionInfo,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        100,           /* MON number (decimal) */
        "144B",         /* Octal string */
        "MAGTP",    /* Short name */
        "DeviceFunction",          /* Long name */
        "Performs various operations on floppy disks, magnetic tapes, Versatec plotters, and SCSI streamers.\n",  /* Description */
        "[I] FunctionCode (INTEGER2): Function code. See the following pages.\\n[IO] Buffer (INTEGER2[1024]): Buffer used for data transfer to and from the device.\\n[I] DeviceNo (INTEGER2): Logical device number (or open-file number). See appendix B.\\n[I] DeviceParam1 (INTEGER2): First device dependent parameter. See the following pages.\\n[I] DeviceParam2 (INTEGER2): Second device dependent parameter. See the following pages.",  /* Parameter details */
        mon_144B_DeviceFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        102,           /* MON number (decimal) */
        "146B",         /* Octal string */
        "IPRIV",    /* Short name */
        "PrivInstruction",          /* Long name */
        "Executes a privileged machine instruction on the ND-100. Privileged instructions may, for example, t",  /* Description */
        "[I] Instruction (INTEGER): Privileged machine instruction to execute on the ND-100.",  /* Parameter details */
        mon_146B_PrivInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        103,           /* MON number (decimal) */
        "147B",         /* Octal string */
        "CAMAC",    /* Short name */
        "CAMACFunction",          /* Long name */
        "Operates the CAMAC, i.e. executes the NAF register. CAMAC is a standardized way to connect periphera",  /* Description */
        "[IO] DataWord (INTEGER2): Input of data if write, output if read.\\n[O] RetStatus (INTEGER2): Return status (0 if OK, 24 if not OK).\\n[I] CrateNo (INTEGER2): Crate number.\\n[I] StationNo (INTEGER2): Station number.\\n[I] Subaddress (INTEGER2): Subaddress.\\n[I] Func (INTEGER2): Function:\n- 0 All registers - Read\n- 1 All registers - Write\n- 3 Selected Bit Clear\n- 5 Selected Bit Set\n- 7 Execute Dataway Cycle",  /* Parameter details */
        mon_147B_CAMACFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        12,           /* MON number (decimal) */
        "14B",         /* Octal string */
        "COBUF",    /* Short name */
        "ClearOutBuffer",          /* Long name */
        "Clears a device output buffer. Output to character devices, e.g. terminals, are temporarily stored i",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B. Use 1 for your own terminal.",  /* Parameter details */
        mon_14B_ClearOutBuffer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        104,           /* MON number (decimal) */
        "150B",         /* Octal string */
        "GL",    /* Short name */
        "CAMACGLRegister",          /* Long name */
        "Read the CAMAC GL (Graded LAM - \"look at me\") register or the last CAMAC identification number. See ",  /* Description */
        "[I] Flag (INTEGER2): -1 means read last identification number. Other values means read GL register.\\n[I] CrateNo (INTEGER2): Crate number.",  /* Parameter details */
        mon_150B_CAMACGLRegister,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        105,           /* MON number (decimal) */
        "151B",         /* Octal string */
        "GRTDA",    /* Short name */
        "GetRTAddress",          /* Long name */
        "Gets the address of an RT description. You specify the name of the RT program. See the SINTRAN III R",  /* Description */
        "[I] RTProgramName (STRING): RT-program name (up to 7 chars, uppercase). End with apostrophe if < 7 chars.\\n[O] RTProgram (INTEGER): RT program address. Returns -1 if RT-program doesn't exist.",  /* Parameter details */
        mon_151B_GetRTAddress,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        106,           /* MON number (decimal) */
        "152B",         /* Octal string */
        "GRTNA",    /* Short name */
        "GetRTName",          /* Long name */
        "Gets the name of an RT program. You specify the RT description address.\n\n- This monitor call is only",  /* Description */
        "[I] RTProgram (INTEGER): The address of the RT description. Use 0 for the calling program.\\n[O] RTProgramName (STRING): Name of the RT program (7 bytes). Returned with terminal apostrophe if less than 7 characters.",  /* Parameter details */
        mon_152B_GetRTName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        107,           /* MON number (decimal) */
        "153B",         /* Octal string */
        "IOXN",    /* Short name */
        "CAMACIOInstruction",          /* Long name */
        "Executes a single IOX instruction for CAMAC. See under CAMACFunction (mon 147) for general informati",  /* Description */
        "[IO] DataWord (INTEGER): Input of data if write. Output if read.\\n[I] IOXCode (INTEGER): Physical device number in the range 2000B-4000B.",  /* Parameter details */
        mon_153B_CAMACIOInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        108,           /* MON number (decimal) */
        "154B",         /* Octal string */
        "ASSIG",    /* Short name */
        "AssignCAMACLAM",          /* Long name */
        "Assigns a graded LAM in the CAMAC identification table to a logical device number in the logical num",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[I] GradedLAMNumber (INTEGER): Graded LAM number. Use 0 for high priority on interrupt level 13.\\n[I] CrateNumber (INTEGER): CAMAC crate number in the range 0:15.",  /* Parameter details */
        mon_154B_AssignCAMACLAM,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        109,           /* MON number (decimal) */
        "155B",         /* Octal string */
        "GRAPH",    /* Short name */
        "GraphicFunction",          /* Long name */
        "Executes various functions on a graphic peripheral, such as a NORDCOM terminal, a pen plotter, or a ",  /* Description */
        "[I] Ycoor (INTEGER): The Y-coordinate of new line relative to current reference point.\\n[I] Xcoor (INTEGER): The X-coordinate of new line relative to current reference point.\\n[I] Code (INTEGER): Integer code.\\n[I] DeviceNo (INTEGER): Logical device number.\\n[I] Func (INTEGER): Function code. 0 means PLOT. 1 means PLOTS, i.e. establish reference point and/or clear a NORDCOM screen. 2 means NEWP, i.e. select pen or screen.\\n[O] ReturnValue (INTEGER): Return value. Output parameter for the PLOT function. Not used on the ND-500.",  /* Parameter details */
        mon_155B_GraphicFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        111,           /* MON number (decimal) */
        "157B",         /* Octal string */
        "ENTSG",    /* Short name */
        "SegmentToPageTable",          /* Long name */
        "Enters a routine as a direct task or as a device driver, and \"remembers\" which segments have been en",  /* Description */
        "[I] SegmentNo (INTEGER2): Segment number where the routine resides.\\n[I] PageTable (INTEGER2): Page table to use for the segment. In practice, this should be 3. For SINTRAN III VSX, version K, the range is 0-17.\\n[I] InterruptLevel (INTEGER2): The interrupt level where the direct task should run. You must specify one of the free levels 6, 7, 10B or 11B. Do not use level 2B on SINTRAN III VSX, version K.\\n[I] StartAddress (INTEGER2): Start address of the routine (entry point).",  /* Parameter details */
        mon_157B_SegmentToPageTable,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        112,           /* MON number (decimal) */
        "160B",         /* Octal string */
        "FIXC",    /* Short name */
        "FixContiguous",          /* Long name */
        "Places a segment in physical memory. Its pages will no longer be swapped to the disk. The segment is",  /* Description */
        "[I] SegmentNo (INTEGER2): Segment number to be fixed. Set bit 15 to 1 if you want a return status.\\n[I] PageNumber (INTEGER2): First physical page number to be used.",  /* Parameter details */
        mon_160B_FixContiguous,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        113,           /* MON number (decimal) */
        "161B",         /* Octal string */
        "INSTR",    /* Short name */
        "InString",          /* Long name */
        "Reads a string of characters from a peripheral device, e.g. a terminal.",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number of a peripheral device. See appendix B.\\n[O] TextRead (STRING): Buffer to receive the string of characters read.\\n[I] NoOfBytes (INTEGER): Maximum number of characters to read.\\n[I] Terminator (INTEGER): Terminating character. Input stops when this character is read.\\n[O] ReturnStatus (INTEGER): 16-bit status word. -1=parameter error. Bits 15:14 indicate:\n0=max chars read, 1=terminator read, 2=not terminated (RT only), 3=device error.\nBits 13:0 contain count of chars read (if bits 15:14 are 0, 1, or 2).",  /* Parameter details */
        mon_161B_InString,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        114,           /* MON number (decimal) */
        "162B",         /* Octal string */
        "OUTST",    /* Short name */
        "OutString",          /* Long name */
        "Writes a string of characters to a peripheral file, e.g., a terminal or a printer.\n\n- You cannot use",  /* Description */
        "[I] DeviceNo (INTEGER2): Logical device number. See appendix B. You cannot use 1 for your own terminal. Use ExecutionInfo to get its logical device number instead. File numbers are illegal.\\n[I] TextWrite (STRING): Character string to be output (max 2048 bytes).",  /* Parameter details */
        mon_162B_OutString,  /* Handler */
        MON_STATUS_IN_PROGRESS,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        116,           /* MON number (decimal) */
        "164B",         /* Octal string */
        "WSEG",    /* Short name */
        "SaveSegment",          /* Long name */
        "Saves a segment in the ND-100. All pages in physical memory which have been changed, are written bac",  /* Description */
        "[I] SegmentNumber (INTEGER2): Segment number.",  /* Parameter details */
        mon_164B_SaveSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        117,           /* MON number (decimal) */
        "165B",         /* Octal string */
        "DIW",    /* Short name */
        "GetInRegisters",          /* Long name */
        "Reads the device interface registers.",  /* Description */
        "[I] NoOfReg (INTEGER): Number of registers.\\n[I] Buffer (ARRAY): Buffer with logical unit.\\n[I] DataBuffer (ARRAY): Data buffer.\\n[O] ErrorIndicator (INTEGER): Error indicator.",  /* Parameter details */
        mon_165B_GetInRegisters,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        119,           /* MON number (decimal) */
        "167B",         /* Octal string */
        "REENT",    /* Short name */
        "AttachSegment",          /* Long name */
        "Attaches a reentrant segment to your two current segments. The address areas of the segments may ove",  /* Description */
        "[I] SegmentNumber (INTEGER): Segment number of the reentrant segment. See @LIST-REENTRANT.",  /* Parameter details */
        mon_167B_AttachSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        14,           /* MON number (decimal) */
        "16B",         /* Octal string */
        "MGTTY",    /* Short name */
        "GetTerminalType",          /* Long name */
        "Gets the terminal type. The terminal type tells SINTRAN III how to handle a particular terminal. A w",  /* Description */
        "[I] DeviceNumber (INTEGER2): The logical device number of the terminal. Use 1 for your own terminal in background programs. You may specify TADs.\\n[O] TerminalType (INTEGER2): The terminal type (output). See appendix H for terminal types.",  /* Parameter details */
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
    mon_register_ex(
        15,           /* MON number (decimal) */
        "17B",         /* Octal string */
        "MSTTY",    /* Short name */
        "SetTerminalType",          /* Long name */
        "Sets the type of a terminal. The terminal type tells SINTRAN III how to handle a particular terminal",  /* Description */
        "[I] DeviceNumber (INTEGER2): The logical device number of the terminal. Use 1 for your own terminal in background programs.\\n[I] TerminalType (INTEGER2): The terminal type. See appendix H for terminal types.",  /* Parameter details */
        mon_17B_SetTerminalType,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        1,           /* MON number (decimal) */
        "1B",         /* Octal string */
        "INBT",    /* Short name */
        "InByte",          /* Long name */
        "Reads one byte from a character device, e.g. a terminal or an opened file. If the device is a word-o",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. Use 1 for your own terminal.\\n[O] ReturnValue (INTEGER): The read byte.",  /* Parameter details */
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
    mon_register_ex(
        129,           /* MON number (decimal) */
        "201B",         /* Octal string */
        "MHDLC",    /* Short name */
        "HDLCfunction",          /* Long name */
        "Performs various HDLC functions. A HDLC is a high-level data link to another computer. You may send ",  /* Description */
        "[I] Func (INTEGER): Function code: 0=send DCB to driver, 1=receive DCB from driver\\n[I] DevNo (INTEGER): Logical device number. Different LDNs for input and output part.\\n[IO] Buffer (ARRAY): Address of driver control block buffer.\\n[IO] USize (INTEGER): Size of the used part of the driver control block in bytes.\\n[I] MSize (INTEGER): For SEND: max DCB size in bytes. For RECEIVE: wait flag (1=wait, 0=continue).\\n[O] Status (INTEGER): HDLC error code returned in W1: 1=LDN not reserved, 2=illegal LDN, 3=no DCB in queue, 4=no buffer, 5=illegal DCB size, 6=illegal LDN for this call, 7=max size < used size, 10=illegal function, 11=fatal error.",  /* Parameter details */
        mon_201B_HDLCfunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        134,           /* MON number (decimal) */
        "206B",         /* Octal string */
        "EDTRM",    /* Short name */
        "TerminationHandling",          /* Long name */
        "Switches termination handling on and off.",  /* Description */
        "[I] EnDisFlag (INTEGER): On or off flag. Use 1 for on and 0 for off.\\n[I] Flag (INTEGER): Circumstances for termination handling. Use 1 for termination handling on user break. Use 2 for fatal errors. Specify 0 for both. Always use 0 for RT programs.",  /* Parameter details */
        mon_206B_TerminationHandling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        135,           /* MON number (decimal) */
        "207B",         /* Octal string */
        "RERRP",    /* Short name */
        "GetErrorInfo",          /* Long name */
        "Gets information about the last real-time error. The monitor call returns the error, the RT program ",  /* Description */
        "[O] Buffer (ARRAY): A 12 byte buffer receiving error information: bytes 0-1=error number (2 ASCII chars), bytes 2-3=program address, bytes 4-7=additional error info, bytes 8-9=RT description address, bytes 10-11=flag (0=aborted).\\n[O] ReturnStatus (INTEGER): Returned status in W1. 0 means OK, 153B means illegal output buffer.",  /* Parameter details */
        mon_207B_GetErrorInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        138,           /* MON number (decimal) */
        "212B",         /* Octal string */
        "SREEN",    /* Short name */
        "ReentrantSegment",          /* Long name */
        "Connects a reentrant segment to your two current segments. All modified pages of your current segmen",  /* Description */
        "[I] SegmentNumber (INTEGER): Segment number to attach.",  /* Parameter details */
        mon_212B_ReentrantSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        139,           /* MON number (decimal) */
        "213B",         /* Octal string */
        "MUIDI",    /* Short name */
        "GetDirUserIndexes",          /* Long name */
        "Gets a directory index and a user index. You have to specify a directory name and a user name.\n\n- Us",  /* Description */
        "[I] UserName (STRING): Directory and user name string (up to 16 chars), e.g. 'A-HANSEN'.\\n[O] DirIndex (INTEGER): Directory index.\\n[O] UserIndex (INTEGER): User index.",  /* Parameter details */
        mon_213B_GetDirUserIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        140,           /* MON number (decimal) */
        "214B",         /* Octal string */
        "GUSNA",    /* Short name */
        "GetUserName",          /* Long name */
        "Gets the name of a user. The user may be on a remote computer if the COSMOS network is installed. Th",  /* Description */
        "[O] UserName (STRING): Buffer to receive user name (16 chars).\\n[I] DirectoryIndex (INTEGER): Directory index.\\n[I] UserIndex (INTEGER): User index.\\n[O] RemoteFlag (INTEGER): 0 if local user, 1 if remote user.\\n[O] RemoteSystem (STRING): Remote system identification string (optional, 64 chars).",  /* Parameter details */
        mon_214B_GetUserName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        141,           /* MON number (decimal) */
        "215B",         /* Octal string */
        "DROBJ",    /* Short name */
        "GetObjectEntry",          /* Long name */
        "Gets information about a file. An object entry describes each file. It contains the file name, the a",  /* Description */
        "[O] Buffer (INTEGER2[32]): The 64 byte object entry.\\n[I] DirIndex (INTEGER2): The directory index. See GetAllFileIndexes.\\n[I] UserIndex (INTEGER2): The user index. See GetAllFileIndexes.\\n[I] ObjectIndex (INTEGER2): The object index. See GetAllFileIndexes.",  /* Parameter details */
        mon_215B_GetObjectEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        142,           /* MON number (decimal) */
        "216B",         /* Octal string */
        "DWOBJ",    /* Short name */
        "SetObjectEntry",          /* Long name */
        "Changes the description of a file. An object entry describes each file. It contains the file name, t",  /* Description */
        "[I] Buffer (INTEGER2[32]): The 64 byte object entry. See appendix C.\\n[I] DirIndex (INTEGER2): The directory index. See GetAllFileIndexes.\\n[I] UserIndex (INTEGER2): The user index. See GetAllFileIndexes.\\n[I] ObjectIndex (INTEGER2): The object index. See GetAllFileIndexes.\\n[I] SystemId (STRING): Remote system identification if bit 7 in DirIndex is set.",  /* Parameter details */
        mon_216B_SetObjectEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        5             /* Param count */
    );
    mon_register_ex(
        143,           /* MON number (decimal) */
        "217B",         /* Octal string */
        "GUIOI",    /* Short name */
        "GetAllFileIndexes",          /* Long name */
        "Gets the directory index, the user index, and the object index of a file. These are indexes in the S",  /* Description */
        "[I] FileNo (INTEGER): File number. See OpenFile.\\n[O] DirIndex (INTEGER): Directory index.\\n[O] UserIndex (INTEGER): User index.\\n[O] ObjectIndex (INTEGER): Object index.\\n[O] RemoteFlag (INTEGER): 0 if file is local, 1 if on remote computer.\\n[O] RemoteSystem (STRING): Remote system identification string (optional, 64 chars).",  /* Parameter details */
        mon_217B_GetAllFileIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        17,           /* MON number (decimal) */
        "21B",         /* Octal string */
        "M8INB",    /* Short name */
        "InUpTo8Bytes",          /* Long name */
        "See also In8Bytes, InByte, InString, In4x2Bytes, and Out8Bytes.",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number. See appendix B.\\n[O] NoOfBytes (INTEGER2): Number of bytes read (output). For word-oriented devices, returns word count.\\n[O] InData (BYTES[8]): The 8 bytes of input data (output).",  /* Parameter details */
        mon_21B_InUpTo8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        144,           /* MON number (decimal) */
        "220B",         /* Octal string */
        "DOPEN",    /* Short name */
        "DirectOpen",          /* Long name */
        "Opens a file. Files must be opened before they can be accessed. For public users this monitor call i",  /* Description */
        "[O] FileNumber (INTEGER2): File number returned on success.\\n[I] AccessCode (INTEGER2): Access code: 0=Sequential write, 1=Sequential read, 2=Random read/write, 3=Random read only, 4=Sequential read/write, 5=Sequential write append, 6=Random read/write common contiguous, 7=Random read common contiguous, 8=Random read/write contiguous direct transfer, 9=Random read/write append.\\n[I] FileName (STRING): File name.\\n[I] FileType (STRING): File type. Do not include the colon. Default is SYMB.",  /* Parameter details */
        mon_220B_DirectOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        145,           /* MON number (decimal) */
        "221B",         /* Octal string */
        "CRALF",    /* Short name */
        "CreateFile",          /* Long name */
        "Creates a file. The file may be indexed, contiguous, or allocated. Most files are indexed. The size ",  /* Description */
        "[I] FileName (STRING): File name. Default file type is :DATA.\\n[I] StartAddress (INTEGER2): Start address in the directory. Use 0 if you want to create a contiguous or indexed file.\\n[I] NoOfPages (INTEGER2): Length of the file in pages. Use 0 if you want to create an indexed file.",  /* Parameter details */
        mon_221B_CreateFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        146,           /* MON number (decimal) */
        "222B",         /* Octal string */
        "GBSIZ",    /* Short name */
        "GetAddressArea",          /* Long name */
        "Gets the size of your address area. Your address area may consist of one or two 128 Kbyte areas. Thi",  /* Description */
        "[O] SegmentSize (INTEGER): The size of the address area. 100B means one 128 Kbyte address area. 200B means two 128 Kbyte address areas, one for instructions and one for data.",  /* Parameter details */
        mon_222B_GetAddressArea,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        151,           /* MON number (decimal) */
        "227B",         /* Octal string */
        "MSDAE",    /* Short name */
        "SetEscLocalChars",          /* Long name */
        "You can terminate most programs with the ESCAPE key. A LOCAL key has a similar function. It terminat",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. See appendix B. Only used by RT programs. Your own terminal is always used in background program.\\n[I] DisconnectChar (INTEGER): The local character.\\n[I] EscapeChar (INTEGER): The escape character.",  /* Parameter details */
        mon_227B_SetEscLocalChars,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        18,           /* MON number (decimal) */
        "22B",         /* Octal string */
        "M8OUT",    /* Short name */
        "OutUpTo8Bytes",          /* Long name */
        "Writes up to 8 characters to a device, e.g. a terminal or an internal device.",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. Use 1 for your own terminal. File numbers are illegal.\\n[I] OutData (STRING): The 8 characters to be written (writing stops at first 0 byte).",  /* Parameter details */
        mon_22B_OutUpTo8Bytes,  /* Handler */
        MON_STATUS_IN_PROGRESS,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        152,           /* MON number (decimal) */
        "230B",         /* Octal string */
        "MGDAE",    /* Short name */
        "GetEscLocalChars",          /* Long name */
        "Gets ESCAPE and LOCAL characters. You can terminate most programs with the ESCAPE key. A LOCAL key h",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. Only used by RT programs. Your own terminal is always used in background program. See appendix B.\\n[O] DisconnectChar (INTEGER): The local character.\\n[O] EscapeChar (INTEGER): The escape character.",  /* Parameter details */
        mon_230B_GetEscLocalChars,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        153,           /* MON number (decimal) */
        "231B",         /* Octal string */
        "EXPFI",    /* Short name */
        "ExpandFile",          /* Long name */
        "Expands the file size. You use this monitor call to increase the size of contiguous and allocated fi",  /* Description */
        "[I] FileName (STRING): File name. It may be abbreviated, but this slows down execution.\\n[I] NoOfPages (INTEGER2): Number of additional pages.",  /* Parameter details */
        mon_231B_ExpandFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        154,           /* MON number (decimal) */
        "232B",         /* Octal string */
        "MRNFI",    /* Short name */
        "RenameFile",          /* Long name */
        "See also @RENAME-FILE.",  /* Description */
        "[I] OldFileName (STRING): Old file name.\\n[I] NewFileName (STRING): New file name with file type, e.g. ADDRESS-LIST:TEXT. Do not use the directory name, the user name, or the version number. You may change the file type only. For example, specify :SYMB only. Include the colon.",  /* Parameter details */
        mon_232B_RenameFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        155,           /* MON number (decimal) */
        "233B",         /* Octal string */
        "STEFI",    /* Short name */
        "SetTemporaryFile",          /* Long name */
        "Defines a file to store information temporarily. The file can be read once. When it is closed, its c",  /* Description */
        "[I] FileName (STRING): File name.",  /* Parameter details */
        mon_233B_SetTemporaryFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        156,           /* MON number (decimal) */
        "234B",         /* Octal string */
        "SPEFI",    /* Short name */
        "SetPeripheralName",          /* Long name */
        "Defines a peripheral file, e.g. a printer. You connect a file name to the logical device number of t",  /* Description */
        "[I] FileName (STRING): File name for the peripheral. See appendix G.\\n[I] DeviceNumber (INTEGER2): Logical device number. See appendix B.",  /* Parameter details */
        mon_234B_SetPeripheralName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        157,           /* MON number (decimal) */
        "235B",         /* Octal string */
        "SCROP",    /* Short name */
        "ScratchOpen",          /* Long name */
        "Opens a file as a scratch file. A maximum of 64 pages of the file is kept when you close the file. U",  /* Description */
        "[O] FileNo (INTEGER): File number returned for use with other monitor calls.\\n[I] AccessCode (INTEGER): Access code: 0=seq write, 1=seq read, 2=random R/W, 3=random read, 4=seq R/W, 5=seq write append, 6-9=contiguous file modes.\\n[I] FileName (STRING): Name of the file to be opened.\\n[I] FileType (STRING): Default file type without colon.",  /* Parameter details */
        mon_235B_ScratchOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        158,           /* MON number (decimal) */
        "236B",         /* Octal string */
        "SPERD",    /* Short name */
        "SetPermanentOpen",          /* Long name */
        "Sets a file permanently open. The file is not closed by CloseFile with -1 as file number. You have t",  /* Description */
        "[I] FileNumber (INTEGER): File number returned from earlier open.",  /* Parameter details */
        mon_236B_SetPermanentOpen,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        159,           /* MON number (decimal) */
        "237B",         /* Octal string */
        "SFACC",    /* Short name */
        "SetFileAccess",          /* Long name */
        "Sets the access protection for a file. You should specify the access for yourself, friends, and othe",  /* Description */
        "[I] FileName (STRING): File name. It is most efficient to use unabbreviated file names, e.g. EXAMPLE:TEXT. The default file type is :SYMB.\\n[I] PublicAccess (STRING): Public access. Use N or a combination of R, W, A, C, and D. Public access is typically set to R or N.\\n[I] FriendAccess (STRING): Friend access. Use N or a combination of R, W, A, C, and D. Friend access is typically set to RWA or R.\\n[I] OwnAccess (STRING): Own access. Use N or a combination of R, W, A, C, and D. Own access is typically RWACD, though lesser combinations can be used (e.g. RWAC to protect your own files from accidental deletion by yourself).",  /* Parameter details */
        mon_237B_SetFileAccess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        19,           /* MON number (decimal) */
        "23B",         /* Octal string */
        "B8INB",    /* Short name */
        "In8Bytes",          /* Long name */
        "Reads 8 bytes from a device. The input is fast, but the monitor call does not apply the defined echo",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[O] NoOfBytes (INTEGER): Number of bytes read (or words if word-oriented device).\\n[O] DataRead (STRING): Buffer to receive the 8 bytes read.",  /* Parameter details */
        mon_23B_In8Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        160,           /* MON number (decimal) */
        "240B",         /* Octal string */
        "APSPE",    /* Short name */
        "AppendSpooling",          /* Long name */
        "Prints a file. The printer has a queue of files waiting to be output. The file is appended to this q",  /* Description */
        "[I] FileName (STRING): File name to print (64 chars).\\n[I] PrinterName (STRING): Spooling device name, e.g. 'LINE-PRINTER' (64 chars).\\n[I] NoOfCopies (INTEGER): Number of copies (bits 0-14). Set bit 15 if UserText is present.\\n[I] UserText (STRING): Optional message to operator (128 chars). Only used if bit 15 of NoOfCopies is set.",  /* Parameter details */
        mon_240B_AppendSpooling,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        161,           /* MON number (decimal) */
        "241B",         /* Octal string */
        "SUSCN",    /* Short name */
        "NewUser",          /* Long name */
        "Switches the user name you are logged in under. The command is similar to logging out and then loggi",  /* Description */
        "[I] UserName (STRING): New user name.\\n[I] UserPassword (INTEGER2): Password. Use the contents of the password location in the user entry.\\n[I] ProjectPassword (STRING): Project password.\\n[O] UserType (INTEGER2): Return status. Public users return 0. User SYSTEM returns 1. User RT returns 2.",  /* Parameter details */
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
    mon_register_ex(
        163,           /* MON number (decimal) */
        "243B",         /* Octal string */
        "FDINA",    /* Short name */
        "GetDirNameIndex",          /* Long name */
        "Gets directory index and name index. The name index identifies the device description of the disk. Y",  /* Description */
        "[I] DirName (STRING): Directory name string (1-16 characters).\\n[O] DirIndex (INTEGER)\\n[O] NameIndex (INTEGER)",  /* Parameter details */
        mon_243B_GetDirNameIndex,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        164,           /* MON number (decimal) */
        "244B",         /* Octal string */
        "GDIEN",    /* Short name */
        "GetDirEntry",          /* Long name */
        "Gets information about a directory. The directory entry is returned. Appendix C describes the file s",  /* Description */
        "[I] DirectoryIndex (INTEGER2): The directory index. See GetDirUserIndexes. Set bit 7 if SysId is supplied.\\n[O] DirEntry (INTEGER2[21]): The 42-byte directory entry. See appendix C.\\n[O] Flag (INTEGER2): Flag indicating whether or not the disk has spare-track allocation.",  /* Parameter details */
        mon_244B_GetDirEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        165,           /* MON number (decimal) */
        "245B",         /* Octal string */
        "GNAEN",    /* Short name */
        "GetNameEntry",          /* Long name */
        "Gets information about devices, e.g. disks and floppy disks. The monitor call returns the name entry",  /* Description */
        "[I] NameIndex (INTEGER): The name index of the device.\\n[O] NameTableEntry (ARRAY): A 28 byte buffer receiving name entry: bytes 0-15=device name, 16-19=storage capacity (pages), 20-21=sector size, 22-23=flags (bit15=cartridge, bit13=single user, bit11=tape, bit10=EEC, bit8=floppy, bit7=Phoenix, bit6=Winchester, bit5=SCSI streamer, bit4=SCSI disk, bit3=WORM, bits0-2=max subunits), 24-25=transfer routine address, 26-27=semaphore LDN.",  /* Parameter details */
        mon_245B_GetNameEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        166,           /* MON number (decimal) */
        "246B",         /* Octal string */
        "REDIR",    /* Short name */
        "ReserveDir",          /* Long name */
        "Reserves a directory for special use. The directory must be entered. Other users will not be able to",  /* Description */
        "[I] DirectoryIndex (INTEGER): Directory index. Use @LIST-DIRECTORIES to find the directory index.",  /* Parameter details */
        mon_246B_ReserveDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        167,           /* MON number (decimal) */
        "247B",         /* Octal string */
        "RLDIR",    /* Short name */
        "ReleaseDir",          /* Long name */
        "Releases a directory. The directory must have been reserved with ReserveDir.",  /* Description */
        "[I] DirectoryIndex (INTEGER): Directory index. Use @LIST-DIRECTORIES to find the directory index.",  /* Parameter details */
        mon_247B_ReleaseDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        20,           /* MON number (decimal) */
        "24B",         /* Octal string */
        "B8OUT",    /* Short name */
        "Out8Bytes",          /* Long name */
        "Writes 8 bytes to a character device, e.g. a terminal. All 8 bytes are output. OutUpTo8Bytes stops i",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[I] OutData (STRING): Buffer containing 8 bytes to write.",  /* Parameter details */
        mon_24B_Out8Bytes,  /* Handler */
        MON_STATUS_IN_PROGRESS,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        168,           /* MON number (decimal) */
        "250B",         /* Octal string */
        "FDFDI",    /* Short name */
        "GetDefaultDir",          /* Long name */
        "Gets the user?s default directory. The directory index and the user index are returned.\n\n- Use Execu",  /* Description */
        "[I] UserName (STRING): User name string (16 chars). May identify remote user.\\n[O] DirectoryIndex (INTEGER): Directory index.\\n[O] UserIndex (INTEGER): User index in the directory.",  /* Parameter details */
        mon_250B_GetDefaultDir,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        169,           /* MON number (decimal) */
        "251B",         /* Octal string */
        "COPAG",    /* Short name */
        "CopyPage",          /* Long name */
        "Copies file pages between two opened files. One of the files may be a magnetic tape or floppy disk w",  /* Description */
        "[I] SourceFile (INTEGER): File number of source file. In the T register. Use logical device number for magnetic tape and floppy disks.\\n[I] DestFile (INTEGER): File number of destination file. In the A register. Use logical device number for magnetic tapes and floppy disks.\\n[I] FirstPage (INTEGER): Address of 32-bit word with page address of first page to copy. The page address is the same for the source and destination files.\\n[I] DestBuffer (INTEGER): Address of buffer to receive short magnetic tape record. If source file is magnetic tape. Use -1 if you do not want the short record returned.\\n[O] FirstPageMiss (INTEGER): Page number of the missing page (double skip return). In the A&D register. Unless the source is magnetic tape, a page is missing.\\n[O] LastPageMiss (INTEGER): Last page number missing in a hole (double skip return). A set of contiguous pages can be missing. The T and X register contains the last page number missing in such a hole. This is only returned if the source is a directory.\\n[O] NoOfWord (INTEGER): Number of 16-bit integers returned (double skip return). If a short magnetic tape record is found. Only for magnetic tape as source and the D register different from -1.",  /* Parameter details */
        mon_251B_CopyPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register_ex(
        170,           /* MON number (decimal) */
        "252B",         /* Octal string */
        "BCLOS",    /* Short name */
        "BackupClose",          /* Long name */
        "Closes a file. The version number and the last date accessed are unchanged. The number of pages in t",  /* Description */
        "[I] FileNumber (INTEGER): File number of the opened file. See OpenFile.\\n[I] Flag (INTEGER): Modified flag. If 0, the file is not marked as modified.",  /* Parameter details */
        mon_252B_BackupClose,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        171,           /* MON number (decimal) */
        "253B",         /* Octal string */
        "CRALN",    /* Short name */
        "NewFileVersion",          /* Long name */
        "Creates new versions of a file. You may create new versions for both indexed, contiguous and allocat",  /* Description */
        "[I] FileName (STRING): The file name including the version number (up to 64 chars). The version number defines the total number of versions.\\n[I] FirstPage (INTEGER): Start address of the first new version. Use 0 for contiguous and indexed files.\\n[I] NoOfPages (INTEGER): File size in pages. Use 0 for indexed files.",  /* Parameter details */
        mon_253B_NewFileVersion,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        172,           /* MON number (decimal) */
        "254B",         /* Octal string */
        "GERDV",    /* Short name */
        "GetErrorDevice",          /* Long name */
        "Gets the logical device number of the error device. The error device may be reserved by an RT progra",  /* Description */
        "[O] ErrorDevice (INTEGER): The logical device number of the error device.\\n[O] RTProgram (INTEGER): RT description address of reserving RT program. 0 means unreserved.",  /* Parameter details */
        mon_254B_GetErrorDevice,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        173,           /* MON number (decimal) */
        "255B",         /* Octal string */
        "PIOCM",    /* Short name */
        "PIOCFunction",          /* Long name */
        "PIOC is a programmable input and output processor primarily used in data communication to handle net",  /* Description */
        "[I] DeviceNo (INTEGER): Logical Device number. Specifies which PIOC module to access.\\n[I] SlotNo (INTEGER): Slot number (1-8). Refers to the PIOC kick channel. Not applicable for functions 4, 5, 6, or 7.\\n[I] FuncNo (INTEGER): Function number (0-7). Further details are on the following page.\\n[I] Message (INTEGER): The message itself (2 bytes of information). Used by functions 2 and 3.\\n[I] SegNo (INTEGER): Segment number to be loaded. Applicable for function 4.\\n[I] PageNo (INTEGER): Page number where the loading should start. Applicable for function 4.\\n[O] Status (INTEGER): Status code (octal). Further details are on the following page.",  /* Parameter details */
        mon_255B_PIOCFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register_ex(
        174,           /* MON number (decimal) */
        "256B",         /* Octal string */
        "DEABF",    /* Short name */
        "FullFileName",          /* Long name */
        "Returns a complete file name from an abbreviated one. The directory, the user, the file name, the fi",  /* Description */
        "[I] AbbrevFileName (STRING): Abbreviated file name string (64 chars). May include a file type.\\n[O] FileName (STRING): Buffer to receive complete file name, terminated by apostrophe (64 chars).\\n[I] FileType (STRING): Default file type string (4 chars). Used on ND-100 only, ignored by ND-500.",  /* Parameter details */
        mon_256B_FullFileName,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        175,           /* MON number (decimal) */
        "257B",         /* Octal string */
        "FOPEN",    /* Short name */
        "OpenFileInfo",          /* Long name */
        "Gets information about an open file. You specify the file name. The monitor call returns the file nu",  /* Description */
        "[I] FileName (STRING): File name.\\n[I] FileType (STRING): File type.\\n[O] FileNo (INTEGER2): File number. If error return, this parameter contains the Logical Device Number (LDN) of peripheral device (only for peripheral files).\\n[O] AccessCode (INTEGER2): Access code. 0 means read. 1 means write. 2 means read and write.\\n[O] DevNo (INTEGER2): Logical device number of peripheral device. This is only relevant for peripheral files.\\n[O] ErrCode (INTEGER2): Standard Error Code. See appendix A.",  /* Parameter details */
        mon_257B_OpenFileInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        178,           /* MON number (decimal) */
        "262B",         /* Octal string */
        "CPUST",    /* Short name */
        "GetSystemInfo",          /* Long name */
        "Gets various system information. The system number, the CPU type, the SINTRAN III version, the instr",  /* Description */
        "[I] Number (INTEGER): A number. Should always be 0.\\n[O] Buffer (ARRAY): A 24 byte (12 word) buffer receiving system information (CPU type, SINTRAN version, instruction set, etc.).",  /* Parameter details */
        mon_262B_GetSystemInfo,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        179,           /* MON number (decimal) */
        "263B",         /* Octal string */
        "GDEVT",    /* Short name */
        "GetDeviceType",          /* Long name */
        "Gets the device type, e.g. terminal, floppy disk, mass-storage file, etc. The monitor call also prov",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number (1= own terminal). See appendix B.\\n[I] IOFlag (INTEGER): Input or output part. Use 0 for input and 1 for output.\\n[O] DevType (INTEGER): Device type. The numbers below are returned: 0: Unspecified. 1: Terminal. 2: Terminal access device (TAD). 3: Communication channel. 4: Internal block device. 5: Floppy disk drive. 6: Magnetic tape station. 7: Mass-storage file.\\n[O] DevAttr (INTEGER4): Device information (returned in the combined A and D registers. The bits have the following meaning: Bit 0: InByte or OutByte allowed. Bit 1: StartOnInterrupt allowed. Bit 2: DeviceControl allowed. Bit 3: Block calls allowed. Bit 4: ClearDevice available. Bit 5: Reservation not needed. Bit 6: COSMOS remote open file. Bit 10g: NOTS (NET/One Terminal Server) terminal. Bit 11g: MTAD device.",  /* Parameter details */
        mon_263B_GetDeviceType,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        183,           /* MON number (decimal) */
        "267B",         /* Octal string */
        "TMOUT",    /* Short name */
        "TimeOut",          /* Long name */
        "Suspends the execution of your program for a given time. The execution then continues after the moni",  /* Description */
        "[I] NoTimeUnits (INTEGER): Number of time units to suspend the program.\\n[I] UnitType (INTEGER): The type of time units: 1=basic time units (1/50th sec), 2=seconds, 3=minutes, 4=hours.\\n[O] RestartReason (INTEGER): Restart cause: 0=time elapsed, 1=break restarted, -1=scheduled for repeated execution.",  /* Parameter details */
        mon_267B_TimeOut,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        22,           /* MON number (decimal) */
        "26B",         /* Octal string */
        "LASTC",    /* Short name */
        "GetLastByte",          /* Long name */
        "Gets the last character typed on a terminal. The monitor call can be used to terminate long output s",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number of a terminal.\\n[O] LastCharTyped (INTEGER): The last character typed on the terminal. Returns -1 on error.",  /* Parameter details */
        mon_26B_GetLastByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        184,           /* MON number (decimal) */
        "270B",         /* Octal string */
        "RDPAG",    /* Short name */
        "ReadDiskPage",          /* Long name */
        "Reads one or more directory pages. Any page can be read.\n\n- The directory must be reserved with Rese",  /* Description */
        "[I] DirIndex (INTEGER): Directory index. See GetDirUserIndexes.\\n[O] Buffer (ARRAY): Buffer to receive pages (must start on even byte address).\\n[I] PageAddr (INTEGER): Address of the pages on the disk (double word).\\n[I] NoOfPages (INTEGER): Number of pages to transfer. Each page is 2048 bytes.",  /* Parameter details */
        mon_270B_ReadDiskPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        185,           /* MON number (decimal) */
        "271B",         /* Octal string */
        "WDPAG",    /* Short name */
        "WriteDiskPage",          /* Long name */
        "Writes to one or more pages in a directory. Any page can be written to.\n\n- The directory must be res",  /* Description */
        "[I] DirIndex (INTEGER): Directory index. See GetDirUserIndexes.\\n[I] Buffer (ARRAY): Buffer with pages to transfer (must start on even byte address).\\n[I] PageAddr (INTEGER): Address of the destination pages on the disk (double word).\\n[I] NoOfPages (INTEGER): Number of pages to transfer. Each page is 2048 bytes.",  /* Parameter details */
        mon_271B_WriteDiskPage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        186,           /* MON number (decimal) */
        "272B",         /* Octal string */
        "DELPG",    /* Short name */
        "DeletePage",          /* Long name */
        "Deletes pages from a file. Pages between two page numbers are removed.\n\n- The file must be opened.",  /* Description */
        "[I] FileNo (INTEGER2): File number.\\n[I] FirstPage (INTEGER2): First page to be deleted.\\n[I] LastPage (INTEGER2): Last page to be deleted. The value -1 means delete to end of the file.\\n[O] NoOfPages (INTEGER2): Number of pages deleted.",  /* Parameter details */
        mon_272B_DeletePage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        187,           /* MON number (decimal) */
        "273B",         /* Octal string */
        "MGFIL",    /* Short name */
        "GetFileName",          /* Long name */
        "Gets the name of a file. You specify the directory index, the user index, and the object index. The ",  /* Description */
        "[I] DirIndex (INTEGER): Directory index.\\n[I] UserIndex (INTEGER): User index.\\n[I] ObjectIndex (INTEGER): Object index.\\n[O] FileName (STRING): File name (64 chars).\\n[I] RemoteFlag (INTEGER): 0 for local file, 1 for remote file.\\n[IO] RemoteSystem (STRING): Remote system identification if remote flag is 1 (64 chars). Not returned by ND-500.",  /* Parameter details */
        mon_273B_GetFileName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        188,           /* MON number (decimal) */
        "274B",         /* Octal string */
        "FOBJN",    /* Short name */
        "GetFileIndexes",          /* Long name */
        "Gets the directory index, the user index, and the object index of a file. These are indexes in the f",  /* Description */
        "[I] FileName (STRING): File name. Abbreviated file names are less efficient.\\n[I] FileType (STRING): File type. Not used on the ND-100. Do not include the colon.\\n[O] DirIndex (INTEGER2): Directory index.\\n[O] UserIndex (INTEGER2): User index.\\n[O] ObjectIndex (INTEGER2): Object index.\\n[O] NextObjectIndex (INTEGER2): Object index of the next file version. Equal to object index if no more versions exist.",  /* Parameter details */
        mon_274B_GetFileIndexes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        189,           /* MON number (decimal) */
        "275B",         /* Octal string */
        "STRFI",    /* Short name */
        "SetTerminalName",          /* Long name */
        "Defines the file name to be used for terminals. This is normally `TERMINAL:`. Background users ident",  /* Description */
        "[I] TerminalName (STRING): File name for terminals (64 chars), normally 'TERMINAL'.",  /* Parameter details */
        mon_275B_SetTerminalName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        190,           /* MON number (decimal) */
        "276B",         /* Octal string */
        "ELOFU",    /* Short name */
        "EnableLocal",          /* Long name */
        "You may log in on remote computers through the COSMOS data network. A key on the terminal returns yo",  /* Description */
        "[I] ProgramAddress (INTEGER): Program address of local handling.",  /* Parameter details */
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
    mon_register_ex(
        23,           /* MON number (decimal) */
        "27B",         /* Octal string */
        "RTDSC",    /* Short name */
        "GetRTDescr",          /* Long name */
        "Reads an RT description. The RT description contains various information about an RT program. You sp",  /* Description */
        "[I] RTProgram (INTEGER): RT description address. Use 0 for the calling RT program.\\n[O] RTDescriptor (ARRAY): A 52 byte buffer receiving RT description.\\n[O] NoOfConnDev (INTEGER): Number of devices connected to the RT program through StartOnInterrupt. Returns -1 on invalid RT description address.",  /* Parameter details */
        mon_27B_GetRTDescr,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        2,           /* MON number (decimal) */
        "2B",         /* Octal string */
        "OUTBT",    /* Short name */
        "OutByte",          /* Long name */
        "Writes one byte to a character device, e.g. a terminal or an opened file. If the device is a word-or",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B. Use 1 for your own terminal.\\n[I] OutputValue (INTEGER): The byte to write.",  /* Parameter details */
        mon_2B_OutByte,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        192,           /* MON number (decimal) */
        "300B",         /* Octal string */
        "EUSEL",    /* Short name */
        "SetEscapeHandling",          /* Long name */
        "Enables user-defined escape handling. When the ESCAPE key is pressed, execution continues at the spe",  /* Description */
        "[I] EscapeHandler (INTEGER): Contents of first location of escape-handler routine. See PLANC example.",  /* Parameter details */
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
    mon_register_ex(
        198,           /* MON number (decimal) */
        "306B",         /* Octal string */
        "GTMOD",    /* Short name */
        "GetTerminalMode",          /* Long name */
        "Gets the terminal mode. The terminal mode tells how the terminal function, i.e. if all letters are c",  /* Description */
        "[I] DeviceNumber (INTEGER2): The logical device number of the terminal. See appendix B.\\n[O] TerminalMode (INTEGER2): The terminal mode. Bit 0: Capital letters. Bit 1: Delay after return. Bit 2: Stop on full page. Bit 3: (modes 8-15).",  /* Parameter details */
        mon_306B_GetTerminalMode,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        199,           /* MON number (decimal) */
        "307B",         /* Octal string */
        "TNOWAI",    /* Short name */
        "TerminalNoWait",          /* Long name */
        "Switches No Wait on and off. No Wait is useful for input from, and output to, character devices, e.g",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number of a character device. See appendix B.\\n[I] IOFlag (INTEGER2): Input or output flag. Use 0 for input and 1 for output.\\n[I] NoWaitFlag (INTEGER2): No Wait flag. Use 0 to switch No Wait off, and any other number to switch it on.",  /* Parameter details */
        mon_307B_TerminalNoWait,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        24,           /* MON number (decimal) */
        "30B",         /* Octal string */
        "GETRT",    /* Short name */
        "GetOwnRTAddress",          /* Long name */
        "Gets the address of the calling program's RT description. Background programs get the RT description",  /* Description */
        "[O] RTDescrAddress (INTEGER): The RT description address returned in W1.",  /* Parameter details */
        mon_30B_GetOwnRTAddress,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status - implemented for RT mode support */
        1             /* Param count */
    );
    mon_register_ex(
        200,           /* MON number (decimal) */
        "310B",         /* Octal string */
        "TBIN8",    /* Short name */
        "In8AndFlag",          /* Long name */
        "Reads 8 bytes from a device, e.g., a terminal. The monitor call applies to the defined echo and brea",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[O] NoOfBytes (INTEGER): Number of bytes read. Negative (bit 15 set) if break character was read.\\n[O] Buffer (STRING): Buffer to receive the 8 bytes read.",  /* Parameter details */
        mon_310B_In8AndFlag,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        201,           /* MON number (decimal) */
        "311B",         /* Octal string */
        "WDIEN",    /* Short name */
        "WriteDirEntry",          /* Long name */
        "Changes the information about a directory. The complete contents of the directory entry is set. The ",  /* Description */
        "[I] DirIndex (INTEGER): The directory index. See GetDirUserIndexes.\\n[I] DirEntry (ARRAY): The 48 byte directory entry. See Appendix C.",  /* Parameter details */
        mon_311B_WriteDirEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        202,           /* MON number (decimal) */
        "312B",         /* Octal string */
        "MOINF",    /* Short name */
        "CheckMonCall",          /* Long name */
        "Some monitor calls are optional or only available in later versions of SINTRAN III. This monitor cal",  /* Description */
        "[I] MonCallNumber (INTEGER): Monitor-call number.\\n[O] MonCallEntry (INTEGER): Address of the monitor call entry. 0 means not implemented.",  /* Parameter details */
        mon_312B_CheckMonCall,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        203,           /* MON number (decimal) */
        "313B",         /* Octal string */
        "IBRISZ",    /* Short name */
        "InBufferState",          /* Long name */
        "Gets information about an input buffer. The current number of bytes in it, and the number of bytes u",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number. See appendix B.\\n[O] NoInBuffer (INTEGER): Number of bytes currently in the buffer.\\n[O] NoUntilBreak (INTEGER): Number of bytes until break character (0 if no break in buffer).",  /* Parameter details */
        mon_313B_InBufferState,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        204,           /* MON number (decimal) */
        "314B",         /* Octal string */
        "SRUSI",    /* Short name */
        "DefaultRemoteSystem",          /* Long name */
        "Sets default values for COSMOS remote file access. You can specify the default remote system, the re",  /* Description */
        "[I] SystemName (STRING): Remote system name.\\n[I] UserName (STRING): User owning the files in the remote system.\\n[I] Password (STRING): The user's password.\\n[I] ProjPassword (STRING): The user's project password.",  /* Parameter details */
        mon_314B_DefaultRemoteSystem,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        205,           /* MON number (decimal) */
        "315B",         /* Octal string */
        "MLAMU",    /* Short name */
        "LAMUFunction",          /* Long name */
        "Performs various functions on the LAMU system. A LAMU is a logically addressed memory unit. The LAMU",  /* Description */
        "[I] Func (INTEGER): Function code: 1=create LAMU, 2=delete, 3=connect, 4=disconnect, 7=protect, 8=get info, 9=create system-LAMU, 10=create temp system LAMU.\\n[IO] Para2 (INTEGER): Function-dependent parameter 2.\\n[IO] Para3 (INTEGER): Function-dependent parameter 3.\\n[IO] Para4 (INTEGER): Function-dependent parameter 4.",  /* Parameter details */
        mon_315B_LAMUFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        206,           /* MON number (decimal) */
        "316B",         /* Octal string */
        "SRLMO",    /* Short name */
        "SetRemoteAccess",          /* Long name */
        "Switches remote file access on and off. The COSMOS network allows you to access files in remote comp",  /* Description */
        "[I] Mode (INTEGER2): Remote mode flag. Use 0 to switch remote mode off. Switch it on with 1.",  /* Parameter details */
        mon_316B_SetRemoteAccess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        207,           /* MON number (decimal) */
        "317B",         /* Octal string */
        "UECOM",    /* Short name */
        "ExecuteCommand",          /* Long name */
        "Executes a SINTRAN III command. Specify the command name and the parameters as a text string.\n\n- An ",  /* Description */
        "[I] Command (STRING): SINTRAN III command string to execute (up to 35 chars).",  /* Parameter details */
        mon_317B_ExecuteCommand,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        209,           /* MON number (decimal) */
        "321B",         /* Octal string */
        "UEADM",    /* Short name */
        "UEAdministrator",          /* Long name */
        "DEPRECATED: This MON call is no longer supported.",  /* Description */
        "",  /* Parameter details */
        mon_321B_UEAdministrator,  /* Handler */
        MON_STATUS_DEPRECATED,    /* Handler runs (error 52), MOINF returns 0 */
        0             /* Param count */
    );
    mon_register_ex(
        25,           /* MON number (decimal) */
        "31B",         /* Octal string */
        "EXIOX",    /* Short name */
        "IOInstruction",          /* Long name */
        "Executes an IOX machine instruction. The IOX instruction handles the device registers. The IOX instr",  /* Description */
        "[I] RegContents (INTEGER2): Register contents before execution.\\n[I] DevRegAddr (INTEGER2): Device register address.\\n[O] ContentsAfter (INTEGER2): Register contents after execution (output, returned in W1).",  /* Parameter details */
        mon_31B_IOInstruction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        210,           /* MON number (decimal) */
        "322B",         /* Octal string */
        "GSGNO",    /* Short name */
        "GetSegmentNo",          /* Long name */
        "Gets the number of a segment in the ND-100. You specify the segment name. Segment names are created ",  /* Description */
        "[I] SegmentName (STRING): Segment name (6 characters).",  /* Parameter details */
        mon_322B_GetSegmentNo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        211,           /* MON number (decimal) */
        "323B",         /* Octal string */
        "SPLRE",    /* Short name */
        "SegmentOverlay",          /* Long name */
        "Used to build multisegment programs in the ND-100. It is mainly for internal use. A new reentrant se",  /* Description */
        "[I] SegmentNo (INTEGER): Segment number. The segment must be reentrant.\\n[I] Page1A1 (INTEGER): Page number of first page in address-area 1. Normally in the program bank.\\n[I] NoPageA1 (INTEGER): Number of pages in address-area 1.\\n[I] Page1A2 (INTEGER): Page number of first page in address-area 2. Normally in the data bank.\\n[I] NoPageA2 (INTEGER): Number of pages in address-area 2.\\n[I] ClearFlag (INTEGER): Clear flag. If not 0, earlier overlay areas are cleared. Use 0 first time.",  /* Parameter details */
        mon_323B_SegmentOverlay,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        212,           /* MON number (decimal) */
        "324B",         /* Octal string */
        "OCTIO",    /* Short name */
        "OctobusFunction",          /* Long name */
        "Performs various functions on an old Octobus (earlier than version 3).",  /* Description */
        "[I] FunctionCode (INTEGER2): Function number: 0=kick, 1=wait for kick, 5=read Octobus status, 6=Who am I.\\n[I] DeviceNo (INTEGER2): Logical device number.\\n[IO] Parameter (INTEGER2): Function dependent parameter. Function 0 returns destination station. Function 5/6 return status value.",  /* Parameter details */
        mon_324B_OctobusFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        213,           /* MON number (decimal) */
        "325B",         /* Octal string */
        "MBECH",    /* Short name */
        "BatchModeEcho",          /* Long name */
        "Controls echo of input and output if the program is executed in a batch or mode job. The purpose is ",  /* Description */
        "[I/O] ControlBitmask (INTEGER): Bit mask to set the echo. Bit 0 set to 1 means no echo in batch and mode executions. Bit 1 set to 1 means output on terminal from mode executions. Bit 2 set to 1 means input from terminal in mode executions. If the parameter is -1, the bit mask from the previous batch or mode job is returned in the A register (ASSEMBLY-500) or via the parameter (high-level languages).",  /* Parameter details */
        mon_325B_BatchModeEcho,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        214,           /* MON number (decimal) */
        "326B",         /* Octal string */
        "MLOGI",    /* Short name */
        "LogInStart",          /* Long name */
        "Logs in a user on a terminal and starts a subsystem.",  /* Description */
        "[I] TermNo (INTEGER): Logical device number of a terminal.\\n[I] UserName (STRING)\\n[I] Password (STRING)\\n[I] ProjPassword (STRING): Project password.\\n[I] Subsystem (STRING)\\n[I] UserParam (ARRAY): The 5 user parameters. See SetUserParam. Array of 5 elements.\\n[O] Status (INTEGER): Return status. Unsuccessful log in returns -1.",  /* Parameter details */
        mon_326B_LogInStart,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        7             /* Param count */
    );
    mon_register_ex(
        26,           /* MON number (decimal) */
        "32B",         /* Octal string */
        "MSG",    /* Short name */
        "OutMessage",          /* Long name */
        "Writes a message to the user's terminal. This is convenient for error messages in background program",  /* Description */
        "[I] Message (STRING): String message to write to user's terminal (max 512 characters).",  /* Parameter details */
        mon_32B_OutMessage,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        216,           /* MON number (decimal) */
        "330B",         /* Octal string */
        "TERST",    /* Short name */
        "TerminalStatus",          /* Long name */
        "Gets information about a terminal. The user logged in, the time logged in, the CPU time used, the jo",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number of a terminal. Use 1 for your own terminal.\\n[O] Buffer (ARRAY): 44 byte buffer receiving terminal info: bytes 0-15=user name, 16-17=mode (1=command, 2=program), 18-19=state (-1=not logged in, 0=idle batch, 1=active), 20-21=CPU time (min), 22-23=time logged in (min), 24-43=last command.",  /* Parameter details */
        mon_330B_TerminalStatus,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        218,           /* MON number (decimal) */
        "332B",         /* Octal string */
        "TREPP",    /* Short name */
        "TerminalLineInfo",          /* Long name */
        "Gets information about a terminal line. You may also enable programs to continue in spite of errors ",  /* Description */
        "[I] FunctionCode (INTEGER): Function code: 0=disable continue after errors, 1=enable continue, 2=get line info.\\n[I] DeviceNo (INTEGER): Logical device number of a terminal. Use 1 for your own terminal.\\n[O] ReturnInfo (INTEGER): Terminal line information (returned for function code 2).",  /* Parameter details */
        mon_332B_TerminalLineInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        219,           /* MON number (decimal) */
        "333B",         /* Octal string */
        "UDMA",    /* Short name */
        "DMAFunction",          /* Long name */
        "Various DMA functions for Direct Memory Access operations.\n\nFunction codes:\n- 1: Receive DMA data (i",  /* Description */
        "[I] DeviceNo (INTEGER2): Logical device number of a DMA channel.\\n[I] FuncCode (INTEGER2): Function code. See function codes in description.\\n[IO] DataAddress (ARRAY): Memory address of data to send or receive for function code 0:3. Function codes 54:57 use programmed input/output devices and this parameter contains the logical device number of the device. Ignored for other function codes.\\n[I] InPara (INTEGER4): Input parameter. Function dependent.\\n[O] OutPara (INTEGER4): Output parameter. Function dependent. Some functions return a status value.\\n[O] ErrCode (INTEGER2): Standard Error Code. See appendix A.",  /* Parameter details */
        mon_333B_DMAFunction,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        220,           /* MON number (decimal) */
        "334B",         /* Octal string */
        "GETXM",    /* Short name */
        "GetErrorMessage",          /* Long name */
        "Gets a SINTRAN III error message text. Appendix A shows the messages connected to each error number.",  /* Description */
        "[I] ErrorNo (INTEGER): Error number (octal) of message to retrieve. Do not use 0.\\n[O] Buffer (STRING): Buffer to receive error message text (128 chars).",  /* Parameter details */
        mon_334B_GetErrorMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        221,           /* MON number (decimal) */
        "335B",         /* Octal string */
        "EXABS",    /* Short name */
        "TransferData",          /* Long name */
        "Transfers data between physical memory and a mass-storage device, e.g. a disk. You may perform vario",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number.\\n[I] Func (INTEGER): Function code.\\n[I] MemAddr (INTEGER): Physical memory address.\\n[I] BlockAddr (INTEGER): Block address on device.\\n[IO] NoOfBlocks (INTEGER): Number of blocks to transfer.\\n[O] ReturnStatus (INTEGER): Return status.",  /* Parameter details */
        mon_335B_TransferData,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        222,           /* MON number (decimal) */
        "336B",         /* Octal string */
        "IOMTY",    /* Short name */
        "Terminal",          /* Long name */
        "This I/O multifunction monitor call is used to change the attributes of terminal and terminal access",  /* Description */
        "[I] FunctionCode (INTEGER2): Function code (more details on page 496).\\n[I] ArrayLength (INTEGER2): Length of function parameter array (must be greater than or equal number of input/output parameters specified for function).\\n[IO] ParameterArray (INTEGER2[]): Function parameter array. (More details are given on page 496.)",  /* Parameter details */
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
    mon_register_ex(
        27,           /* MON number (decimal) */
        "33B",         /* Octal string */
        "ALTON",    /* Short name */
        "AltPageTable",          /* Long name */
        "Switches page table. Each page table allows you to access 128 Kbyte memory. SINTRAN III has 4 page t",  /* Description */
        "[I] PageTableNumber (INTEGER): Number of the page table to use (0-15 for VSX).",  /* Parameter details */
        mon_33B_AltPageTable,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        224,           /* MON number (decimal) */
        "340B",         /* Octal string */
        "RSREC",    /* Short name */
        "ReadSystemRecord",          /* Long name */
        "Used to read the system record into a buffer.",  /* Description */
        "[I] RecordType (INTEGER2): Record type: 1=RT-description, 2=segment entry.\\n[I] AddressOrNumber (INTEGER2): RT-description address or segment number.\\n[O] Buffer (INTEGER2[]): Buffer to receive system record. Minimum 38 words for RecordType=1, 8 words for RecordType=2.\\n[I] Format (INTEGER2): Format (ND-500 only): 0=return information in 16-bit integer format.",  /* Parameter details */
        mon_340B_ReadSystemRecord,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        225,           /* MON number (decimal) */
        "341B",         /* Octal string */
        "SGMTY",    /* Short name */
        "SegmentFunction",          /* Long name */
        "This is a multifunction monitor call used to change the active segments of a program, or the page in",  /* Description */
        "[I] FunctionCode (INTEGER2): Function code: 0=JumpToSegment without PIT change, 1=ExitFromSegment without PIT change, 2=JumpToSegment with PIT change, 3=ExitFromSegment with PIT change, 4=REMSG (remove segment from ENTSEG).\\n[I] StartAddress (INTEGER2): Start/return address.\\n[I] NewSegment1 (INTEGER2): New segment 1 (16-bit value 0-65535).\\n[I] NewSegment2 (INTEGER2): New segment 2 (16-bit value 0-65535).\\n[I] NewPageIndexTables (INTEGER2): New page index tables.",  /* Parameter details */
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
    mon_register_ex(
        29,           /* MON number (decimal) */
        "35B",         /* Octal string */
        "IOUT",    /* Short name */
        "OutNumber",          /* Long name */
        "Writes a number to the user's terminal. The number can be output as an octal or a decimal value.\n\n- ",  /* Description */
        "[I] Format (INTEGER2): Output format. 8=octal, 10=decimal, 16=hexadecimal, 2=bitpattern.\\n[I] Number (INTEGER2): The number to be written (-32768 to 32767).",  /* Parameter details */
        mon_35B_OutNumber,  /* Handler */
        MON_STATUS_IN_PROGRESS,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        30,           /* MON number (decimal) */
        "36B",         /* Octal string */
        "NOWT",    /* Short name */
        "NoWaitSwitch",          /* Long name */
        "Switches No Wait on and off. No Wait is useful for input from, and output to several devices simulta",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number of a character device. See appendix B.\\n[I] IOFlag (INTEGER): Input or output flag. 0=input, 1=output.\\n[I] WaitFlag (INTEGER): No Wait flag. 0=switch No Wait off, non-zero=switch it on.",  /* Parameter details */
        mon_36B_NoWaitSwitch,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        31,           /* MON number (decimal) */
        "37B",         /* Octal string */
        "AIRDW",    /* Short name */
        "ReadADChannel",          /* Long name */
        "Reads an analog to digital channel.\n\n### PARAMETERS",  /* Description */
        "[I] NoOfChannels (INTEGER): Number of channels to read.\\n[I] Channel (INTEGER): Starting channel number.\\n[O] Buffer (ARRAY): Array for data returned.\\n[O] ReturnValue (INTEGER): Actual number of channels read.",  /* Parameter details */
        mon_37B_ReadADChannel,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        3,           /* MON number (decimal) */
        "3B",         /* Octal string */
        "ECHOM",    /* Short name */
        "SetEcho",          /* Long name */
        "When you press a key on the terminal, a character is normally displayed. This is called echo. You mo",  /* Description */
        "[I] DeviceNumber (INTEGER2): The terminal's logical device number. See appendix B. Only needed for RT programs. Background programs ignore this parameter. The user's terminal is assumed.\\n[I] EchoStrategy (INTEGER2): Echo strategy: <0=No echo, 0=Echo all, 1=Echo except control chars, 2=MAC echo, 3-6=System defined, 7=User-defined table.\\n[I] Table (INTEGER2[8]): User-defined echo table (128 bits for ASCII chars). Only used when EchoStrategy=7. Bit 0 means echo that character.",  /* Parameter details */
        mon_3B_SetEcho,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
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
    mon_register_ex(
        257,           /* MON number (decimal) */
        "401B",         /* Octal string */
        "DIASS",    /* Short name */
        "DisAssemble",          /* Long name */
        "Disassembles one machine instruction on the ND-500. Output is the instruction in ASSEMBLY-500 langua",  /* Description */
        "[I] ProgPointer (INTEGER2): Program address.\\n[O] ReturnString (STRING): The assembly instruction as text.\\n[I] MaxNoOfChar (INTEGER2): Maximum number of characters in the assembly instruction.",  /* Parameter details */
        mon_401B_DisAssemble,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        258,           /* MON number (decimal) */
        "402B",         /* Octal string */
        "RFLAG",    /* Short name */
        "GetInputFlags",          /* Long name */
        "ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as ",  /* Description */
        "[O] Value (INTEGER4): Flag values as a 32-bit integer.",  /* Parameter details */
        mon_402B_GetInputFlags,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        259,           /* MON number (decimal) */
        "403B",         /* Octal string */
        "WFLAG",    /* Short name */
        "SetOutputFlags",          /* Long name */
        "ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as ",  /* Description */
        "[I] Value (INTEGER): Flag values as a 32-bit integer.",  /* Parameter details */
        mon_403B_SetOutputFlags,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        260,           /* MON number (decimal) */
        "404B",         /* Octal string */
        "IOFIX",    /* Short name */
        "FixIOArea",          /* Long name */
        "Fixes an address area in a domain in physical memory. The memory area can be used for later input an",  /* Description */
        "[I] FirstAddress (INTEGER): Start address in the domain.\\n[I] SizeOfArea (INTEGER): Number of bytes to fix.",  /* Parameter details */
        mon_404B_FixIOArea,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        261,           /* MON number (decimal) */
        "405B",         /* Octal string */
        "USTRK",    /* Short name */
        "SwitchUserBreak",          /* Long name */
        "Switches user-defined escape handling on and off. The user-defined escape handling transfers control",  /* Description */
        "[I] OnOffFlag (INTEGER2): On/off flag. Use 1 for on and 0 for off.\\n[I] Address (INTEGER2): Program address to start at when you press the ESCAPE key.",  /* Parameter details */
        mon_405B_SwitchUserBreak,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        262,           /* MON number (decimal) */
        "406B",         /* Octal string */
        "RWRTC",    /* Short name */
        "AccessRTCommon",          /* Long name */
        "Reads from or writes to RT common from an ND-500 program. RT common is an area in physical memory wh",  /* Description */
        "[I] Func (INTEGER): Function: 0=read, 1=write.\\n[I] RTCommon (INTEGER): RT common address (ND-100 16-bit word address).\\n[I] NoOfBytes (INTEGER): Number of bytes to read or write.\\n[IO] Buffer (ARRAY): Buffer of data to be read or written.",  /* Parameter details */
        mon_406B_AccessRTCommon,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        32,           /* MON number (decimal) */
        "40B",         /* Octal string */
        "SPCLO",    /* Short name */
        "CloseSpoolingFile",          /* Long name */
        "Appends an opened file to a spooling queue. You specify a text to be printed on the error device whe",  /* Description */
        "[I] FileNo (INTEGER2): File number given when the file was opened.\\n[I] UserText (STRING): Text to be output on the error device when printed.\\n[I] NoOfCopies (INTEGER2): Number of print copies.\\n[I] PrintFlag (INTEGER2): Print flag. 0=output text only if required by @DEFINE-SPOOLING-CONDITIONS, non-zero=print unconditionally.",  /* Parameter details */
        mon_40B_CloseSpoolingFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        264,           /* MON number (decimal) */
        "410B",         /* Octal string */
        "FIXMEM",    /* Short name */
        "FixInMemory",          /* Long name */
        "Fixes a logical segment (either whole or in part) of a user's domain in physical memory. This action",  /* Description */
        "[I] FixType (INTEGER): 0=scattered (non-contiguous), 1=contiguous (returns address), 2=at given memory address.\\n[I] FirstAddr (INTEGER): Starting address within user's domain (32-bit, includes segment number).\\n[I] Length (INTEGER): Length of segment to fix in bytes. -1 = fix remaining part.\\n[IO] ND100Addr (INTEGER): Physical memory address in ND-100 (for FixType 1 or 2). Start of physical page.",  /* Parameter details */
        mon_410B_FixInMemory,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        265,           /* MON number (decimal) */
        "411B",         /* Octal string */
        "UNFIXM",    /* Short name */
        "MemoryUnfix",          /* Long name */
        "Releases a fixed segment in your domain from physical memory. A fixed segment has all its pages fixe",  /* Description */
        "[I] Address (INTEGER): Address containing segment number to unfix.",  /* Parameter details */
        mon_411B_MemoryUnfix,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        266,           /* MON number (decimal) */
        "412B",         /* Octal string */
        "FSCNT",    /* Short name */
        "FileAsSegment",          /* Long name */
        "Connects a file as a segment to your domain. You can then access the file as a logical segment. This",  /* Description */
        "[I] FileNo (INTEGER2): File number. See OpenFile.\\n[I] LogSegmentNo (INTEGER2): Logical segment number in the domain. The segment number must be free. Use 0 to select the first free segment.\\n[I] AccessType (INTEGER2): Access type: 0=file contains initial data, 1=uninitialized empty file, 2=primarily sequential access, 3=combination of 1 and 2.\\n[O] SegmentNo (INTEGER2): Logical segment number selected (returned if LogSegmentNo was 0).",  /* Parameter details */
        mon_412B_FileAsSegment,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        267,           /* MON number (decimal) */
        "413B",         /* Octal string */
        "FSCDNT",    /* Short name */
        "FileNotAsSegment",          /* Long name */
        "Disconnects a file as a segment in your domain. FileAsSegment allows files to be accessed as segment",  /* Description */
        "[I] FileNumber (INTEGER2): File number. See OpenFile.\\n[I] LogSegmentNumber (INTEGER2): Segment number (optional parameter).",  /* Parameter details */
        mon_413B_FileNotAsSegment,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        268,           /* MON number (decimal) */
        "414B",         /* Octal string */
        "BCNAF",    /* Short name */
        "BCNAFCAMAC",          /* Long name */
        "Special CAMAC function on the ND-500. (Same as mon 156 TRACB.)",  /* Description */
        "[I] Function (INTEGER4)\\n[I] Address (INTEGER4)\\n[I] Data (INTEGER4)\\n[O] Status (INTEGER4): Returned via Status variable in high-level languages, or in W1 register for ASSEMBLY-500 (K flag indicates error).",  /* Parameter details */
        mon_414B_BCNAFCAMAC,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        269,           /* MON number (decimal) */
        "415B",         /* Octal string */
        "BCNAF1",    /* Short name */
        "BCNAF1CAMAC",          /* Long name */
        "Special CAMAC monitor call for the ND-500. (Same as mon 176 - user-defined monitor call.)",  /* Description */
        "[I] Function (INTEGER4)\\n[I] Address (INTEGER4)\\n[I] Data (INTEGER4)\\n[O] Status (INTEGER4): Returned via Status variable in high-level languages, or in W1 register for ASSEMBLY-500 (K flag indicates error).",  /* Parameter details */
        mon_415B_BCNAF1CAMAC,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        270,           /* MON number (decimal) */
        "416B",         /* Octal string */
        "WSEGN",    /* Short name */
        "SaveND500Segment",          /* Long name */
        "Writes all modified pages of a segment back to the disk.\n\n- Not allowed when fixed in memory.",  /* Description */
        "[I] LogSegmentNo (INTEGER2): Logical segment number in the domain. If 0, the segment number is retrieved from the parameter address.\\n[I] FirstPage (INTEGER2): First logical page in the segment.\\n[I] LastPage (INTEGER2): Last logical page in the segment.",  /* Parameter details */
        mon_416B_SaveND500Segment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        271,           /* MON number (decimal) */
        "417B",         /* Octal string */
        "MXPISG",    /* Short name */
        "MaxPagesInMemory",          /* Long name */
        "Sets the maximum number of pages a segment may have in physical memory at a time.\n\n- This monitor ca",  /* Description */
        "[I] SegmentNo (INTEGER): Logical segment number in your domain. Use 0 to derive from parameter address.\\n[I] SegType (INTEGER): Segment type: 0=data segments, 1=program segments.\\n[I] NoOfPages (INTEGER): Number of pages.",  /* Parameter details */
        mon_417B_MaxPagesInMemory,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        33,           /* MON number (decimal) */
        "41B",         /* Octal string */
        "ROBJE",    /* Short name */
        "ReadObjectEntry",          /* Long name */
        "Gets information about an opened file. An object entry describes each file. It contains the file nam",  /* Description */
        "[I] FileNumber (WORD): The file number. See OpenFile.\\n[O] Buff (BYTES[64]): The 64 byte object entry buffer. See appendix C.\\n[O] W1: Standard Error Code on error.",  /* Parameter details */
        mon_41B_ReadObjectEntry,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        272,           /* MON number (decimal) */
        "420B",         /* Octal string */
        "GRBLK",    /* Short name */
        "GetUserRegisters",          /* Long name */
        "SwitchUserBreak allows you to save the registers when you terminate an ND-500 program with the ESCAP",  /* Description */
        "[I/O] Buffer (ARRAY): 154 bytes containing the registers in their number sequence.",  /* Parameter details */
        mon_420B_GetUserRegisters,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        273,           /* MON number (decimal) */
        "421B",         /* Octal string */
        "GASGM",    /* Short name */
        "GetActiveSegment",          /* Long name */
        "Gets the name of the segments in your domain. A 2048 byte buffer is returned. It contains 32 pointer",  /* Description */
        "[O] Buffer (INTEGER2[1024]): A 2048 byte buffer (1 page) containing 32 pointers to segment names.",  /* Parameter details */
        mon_421B_GetActiveSegment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        274,           /* MON number (decimal) */
        "422B",         /* Octal string */
        "GSWSP",    /* Short name */
        "GetScratchSegment",          /* Long name */
        "Connects an empty data segment to the user's domain and reserves space for it on the swap file. The ",  /* Description */
        "[I] SizeInBytes (INTEGER): Segment size in bytes.\\n[I] LogSegmentNo (INTEGER): Logical segment number to use. Use 0 for system to select first available free segment.\\n[O] RetLogSegmentNo (INTEGER): Returns the logical segment number actually selected.",  /* Parameter details */
        mon_422B_GetScratchSegment,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        275,           /* MON number (decimal) */
        "423B",         /* Octal string */
        "CAPCOP",    /* Short name */
        "CopyCapability",          /* Long name */
        "Copies a capability for a segment. The segment itself is also copied. A capability describes each lo",  /* Description */
        "[I] SourceSegNo (INTEGER): Source segment number.\\n[I] SourceType (INTEGER): Source segment type.\\n[I] DestSegNo (INTEGER): Destination segment number.\\n[I] DestType (INTEGER): Destination segment type.\\n[I] AccCode (INTEGER): Access code.\\n[O] RetSegNo (INTEGER): Returned segment number.",  /* Parameter details */
        mon_423B_CopyCapability,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        276,           /* MON number (decimal) */
        "424B",         /* Octal string */
        "CAPCLE",    /* Short name */
        "ClearCapability",          /* Long name */
        "Clears a capability. A capability describes each logical segment in a domain. The protection of the ",  /* Description */
        "[I] LogicalSegmentNo (INTEGER2): Logical segment number in your domain.\\n[I] SegmentType (INTEGER2): Segment type: 0=data segments, 1=program segments.",  /* Parameter details */
        mon_424B_ClearCapability,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        277,           /* MON number (decimal) */
        "425B",         /* Octal string */
        "SPRNAM",    /* Short name */
        "SetProcessName",          /* Long name */
        "Defines a new name for your process.\n\n- Process names may be up to 16 characters and contain an addi",  /* Description */
        "[I] ProcessName (STRING): New process name (up to 34 chars, max 16 chars + user name prefix).",  /* Parameter details */
        mon_425B_SetProcessName,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        278,           /* MON number (decimal) */
        "426B",         /* Octal string */
        "GPRNAM",    /* Short name */
        "GetProcessNo",          /* Long name */
        "Gets the number of a process in the ND-500. You specify the process name. The process number is assi",  /* Description */
        "[I] ProcessName (STRING): Process name. May include a user name, e.g. (P-HANSEN)WP-PROCESS.\\n[O] ProcessNumber (INTEGER): Process number in bit 31:16 and magic number in bit 15:0.",  /* Parameter details */
        mon_426B_GetProcessNo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        279,           /* MON number (decimal) */
        "427B",         /* Octal string */
        "GPRNME",    /* Short name */
        "GetOwnProcessInfo",          /* Long name */
        "Gets the name and number of your own process in the ND-500. You get a process each time you enter th",  /* Description */
        "[O] ProcessName (STRING): Process name, maximum 34 bytes.\\n[O] ProcessNumber (INTEGER): Process number in bit 31:16 and magic number in bit 15:0.",  /* Parameter details */
        mon_427B_GetOwnProcessInfo,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        280,           /* MON number (decimal) */
        "430B",         /* Octal string */
        "ADR100",    /* Short name */
        "TranslateAddress",          /* Long name */
        "Translates an ND-500 logical address to an ND-100 physical address. Use this monitor call to set up ",  /* Description */
        "[I] ND500Array (INTEGER4): ND-500 array logical address.\\n[O] ND100PhysWordAddr (INTEGER4): Physical word address in the ND-100.",  /* Parameter details */
        mon_430B_TranslateAddress,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        281,           /* MON number (decimal) */
        "431B",         /* Octal string */
        "MWAITF",    /* Short name */
        "AwaitTransfer",          /* Long name */
        "Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevan",  /* Description */
        "[I] FileNumber (INTEGER2): File number. See OpenFile.\\n[I] WaitFlag (INTEGER2): Wait flag. If 0, the program waits until the data transfer is completed. Other values return a value showing the state of the transfer. ND-500 programs do not wait.\\n[O] NoOfBytes (INTEGER2): Number of bytes transferred.",  /* Parameter details */
        mon_431B_AwaitTransfer,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        285,           /* MON number (decimal) */
        "435B",         /* Octal string */
        "PRT",    /* Short name */
        "ForceTrap",          /* Long name */
        "Forces a programmed trap to occur in another ND-500 process. The trap handler in this process is sta",  /* Description */
        "[I] ProcessNumber (INTEGER): Process number to be trapped.\\n[I] ReasonCode (INTEGER): Reason code. Bit 31:16=process number, bit 15:0=magic number.",  /* Parameter details */
        mon_435B_ForceTrap,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        286,           /* MON number (decimal) */
        "436B",         /* Octal string */
        "5PASET",    /* Short name */
        "SetND500Param",          /* Long name */
        "Sets information about an ND-500 program. Use GetND500Param to read the 5 parameters when a program ",  /* Description */
        "[I] Buffer (INTEGER2[5]): 5-word buffer: [0]=user/dir index (bits 24:16=user, 15:0=directory), [1]=terminal device number, [2]=error number or -1 if ESCAPE, [3][4]=user parameters.",  /* Parameter details */
        mon_436B_SetND500Param,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        287,           /* MON number (decimal) */
        "437B",         /* Octal string */
        "5PAGET",    /* Short name */
        "GetND500Param",          /* Long name */
        "Gets information about why the last ND-500 program terminated. There are five parameters for each ba",  /* Description */
        "[O] Buffer (ARRAY): 5-word buffer receiving: [0]=user/dir index, [1]=terminal device, [2]=error code, [3-4]=user defined.",  /* Parameter details */
        mon_437B_GetND500Param,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        35,           /* MON number (decimal) */
        "43B",         /* Octal string */
        "CLOSE",    /* Short name */
        "CloseFile",          /* Long name */
        "Closes one or more files. Files must be opened before they are accessed. Afterwards they should be c",  /* Description */
        "[I] FileNumber (INTEGER): File number returned when the file was opened.\n-1 = close all files not permanently open\n-2 = close all files including scratch and permanently open files",  /* Parameter details */
        mon_43B_CloseFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        288,           /* MON number (decimal) */
        "440B",         /* Octal string */
        "AT5SGM",    /* Short name */
        "Attach500Segment",          /* Long name */
        "Maps a logical ND-500 data segment onto shared ND-100/ND-500(0) physical memory (multiport memory).",  /* Description */
        "[I] Function (INTEGER): Function code. 0 = Detach (forget) a previously attached segment. 1 = Attach a segment. If a physical segment does not exist, create it and map the segment on to the physical ND-100 address area. 2 = Map an existing ND-500 logical data segment onto a physical ND-100 address area.\\n[I] SegNo0 (INTEGER): Segment number (function code 0). Segment number in the range 0-31??.\\n[I] SegNo1 (INTEGER): ND-500 logical data segment address (function code 1). If you specify 0 (zero), the first free segment will be used.\\n[I] Length1 (INTEGER): Length of segment in pages (function code 1).\\n[I] N1Addr1 (INTEGER): ND-100 physical page address (function code 1).\\n[I] SegName (STRING): Segment name (function code 1). A maximum of 35?? characters in the name, including an optional user name, and a terminating apostrophe, which must be present. The parameter is a string descriptor.\\n[I] Access (INTEGER): Access (function code 1). 0 = read only access. 1 = read/write access.\\n[O] RetSegNo (INTEGER): Logical segment number (function code 1).\\n[I] SegNo2 (INTEGER): Start address of ND-500 logical data segment (function code 2).\\n[I] Length2 (INTEGER): Length of the (entire) segment in pages (function code 2).\\n[I] N1Addr2 (INTEGER): Physical ND-100 page address (function code 2).",  /* Parameter details */
        mon_440B_Attach500Segment,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        11             /* Param count */
    );
    mon_register_ex(
        36,           /* MON number (decimal) */
        "44B",         /* Octal string */
        "RUSER",    /* Short name */
        "GetUserEntry",          /* Long name */
        "Gets information about a user. The user entry in the directory is returned. It contains the user nam",  /* Description */
        "[I] UserName (STRING): User name string. May include directory name, e.g. PACK-ONE:P-HANSEN.\\n[O] Buff (ARRAY): Buffer to receive the 64-byte user entry. See appendix C.",  /* Parameter details */
        mon_44B_GetUserEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        4,           /* MON number (decimal) */
        "4B",         /* Octal string */
        "BRKM",    /* Short name */
        "SetBreak",          /* Long name */
        "Sets the break characters for a terminal. Normally, a program waits for input. When a break characte",  /* Description */
        "[I] DeviceNo (INTEGER): Logical device number. Only used by RT programs; background programs use own terminal.\\n[I] BreakStrategy (INTEGER): Break strategy: <0=none, 0=all, 1=control chars, 2=MAC, 3-6=system, 7=user table, 8=last user, 9=max chars only.\\n[I] Table (ARRAY): User-defined 128-bit break table (8 words). Only used with strategy 7.\\n[I] NoOfChar (INTEGER): Maximum number of characters before break. Dummy for strategies 0, 1, 2.",  /* Parameter details */
        mon_4B_SetBreak,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        320,           /* MON number (decimal) */
        "500B",         /* Octal string */
        "STARTP",    /* Short name */
        "StartProcess",          /* Long name */
        "Starts a process in the ND-500. You identify the process with the process number.",  /* Description */
        "[I] ProcessNumber (INTEGER): Process number to start.",  /* Parameter details */
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
    mon_register_ex(
        322,           /* MON number (decimal) */
        "502B",         /* Octal string */
        "SWITCHP",    /* Short name */
        "SwitchProcess",          /* Long name */
        "Sets the current process in a wait state. Restarts another process. This is similar to executing a S",  /* Description */
        "[I] ProcessNumber (INTEGER): Process number to start.",  /* Parameter details */
        mon_502B_SwitchProcess,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        323,           /* MON number (decimal) */
        "503B",         /* Octal string */
        "DVINST",    /* Short name */
        "InputString",          /* Long name */
        "Reads a string from a device, e.g. a terminal or an opened file. This monitor call provide a fast in",  /* Description */
        "[I] DevNo (INTEGER): Logical device number. See appendix B. Use 1 for your own terminal.\\n[I] MaxNo (INTEGER): Maximum number of bytes to read before break.\\n[O] NoOfBytesRet (INTEGER): Number of bytes read.\\n[O] Buff (STRING): Buffer to receive input.\\n[I] BreakStrat (INTEGER): Break setting. See SetBreak. Use 8 for user-defined break table.\\n[I] EchoStrat (INTEGER): Echo setting. See SetEcho. Use 8 for user-defined echo table.\\n[I] BreakT1 (INTEGER): Break table bits 0:31. Bits set to 1 cause break.\\n[I] BreakT2 (INTEGER): Break table bits 32:63.\\n[I] BreakT3 (INTEGER): Break table bits 64:95.\\n[I] BreakT4 (INTEGER): Break table bits 96:127.\\n[I] EchoT1 (INTEGER): Echo table bits 0:31. Bits set to 0 cause echo.\\n[I] EchoT2 (INTEGER): Echo table bits 32:63.\\n[I] EchoT3 (INTEGER): Echo table bits 64:95.\\n[I] EchoT4 (INTEGER): Echo table bits 96:127.",  /* Parameter details */
        mon_503B_InputString,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        14             /* Param count */
    );
    mon_register_ex(
        324,           /* MON number (decimal) */
        "504B",         /* Octal string */
        "DVOUTS",    /* Short name */
        "OutputString",          /* Long name */
        "Writes a string to a device, e.g. a terminal or an opened file.\n\n- This is the most efficient way to",  /* Description */
        "[I] DeviceNo (INTEGER2): Logical device number, e.g. a file number. See appendix B. You may use 1 for your own terminal. Use the SINTRAN III open file number if output to a file.\\n[I] NoOfBytes (INTEGER2): Number of bytes to write (max 2048).\\n[I] Buffer (STRING): String to be output.",  /* Parameter details */
        mon_504B_OutputString,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        325,           /* MON number (decimal) */
        "505B",         /* Octal string */
        "GERRCOD",    /* Short name */
        "GetTrapReason",          /* Long name */
        "Gets the error code from the swapper process. This is only relevant to programmed trap handlers. The",  /* Description */
        "[O] ErrorCode (INTEGER): Error code from swapper process.",  /* Parameter details */
        mon_505B_GetTrapReason,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        327,           /* MON number (decimal) */
        "507B",         /* Octal string */
        "SPRIO",    /* Short name */
        "SetProcessPriority",          /* Long name */
        "Sets the priority for a process in the ND-500. The priorities vary from 0 to 255. The process with t",  /* Description */
        "[I] NewPriority (INTEGER): New priority value (0-255). 0 allows SINTRAN to manage priority dynamically.",  /* Parameter details */
        mon_507B_SetProcessPriority,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        40,           /* MON number (decimal) */
        "50B",         /* Octal string */
        "OPEN",    /* Short name */
        "OpenFile",          /* Long name */
        "Opens a file. You cannot access a file before you open it. Specify what kind of access you want, e.g",  /* Description */
        "[IO] FileNo (INTEGER): If 0 on input, returns the ND-500 open file number. Otherwise specifies the file number to use.\\n[I] AccessCode (INTEGER): Access code specifying type of file access:\n0 = Sequential write\n1 = Sequential read\n2 = Random read or write\n3 = Random read only\n4 = Sequential read or write\n5 = Sequential write append\n6 = Random read or write common on contiguous files\n7 = Random read common on contiguous files\n8 = Random read or write on contiguous files (direct transfer for RT programs)\n9 = Random read, write append for WriteToFile\\n[I] FileName (STRING): File name string (up to 64 characters). If empty, name is read from terminal.\\n[I] FileType (STRING): Default file type string (up to 4 characters), e.g. 'SYMB'.",  /* Parameter details */
        mon_50B_OpenFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        4             /* Param count */
    );
    mon_register_ex(
        332,           /* MON number (decimal) */
        "514B",         /* Octal string */
        "5TMOUT",    /* Short name */
        "ND500TimeOut",          /* Long name */
        "Suspends the execution of an ND-500 program for a given time. The execution then continues after the",  /* Description */
        "[I] NoOfTimeUnits (INTEGER): Number of time units to suspend. Use 0 to restart immediately (clears restart flag).\\n[I] TimeUnit (INTEGER): Type of time units: 1=basic (1/50 sec), 2=seconds, 3=minutes, 4=hours.\\n[O] ReturnStatus (INTEGER): Restart cause: 0=time elapsed, 1=interrupt, -1=scheduled for repeat.",  /* Parameter details */
        mon_514B_ND500TimeOut,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        42,           /* MON number (decimal) */
        "52B",         /* Octal string */
        "TERMO",    /* Short name */
        "TerminalMode",          /* Long name */
        "Selects various terminal functions. You may stop output on full page. Input may be converted to uppe",  /* Description */
        "[I] DeviceNumber (INTEGER): Logical device number of the terminal. See appendix B. Use 1 for your own terminal.\\n[I] Mode (INTEGER): Terminal mode (0-15):\nBit 0: Capital letters (1=yes)\nBit 1: Delay after return (1=yes)\nBit 2: Stop on full page (1=yes)\nBit 3: Auto-logout on line break (1=yes for modes 8-15)",  /* Parameter details */
        mon_52B_TerminalMode,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        43,           /* MON number (decimal) */
        "53B",         /* Octal string */
        "RSEGM",    /* Short name */
        "GetSegmentEntry",          /* Long name */
        "Gets information about a segment in the ND-100. The monitor call returns the segment entry. You spec",  /* Description */
        "[I] SegmentNumber (INTEGER): Segment number. Use 0 for RT common.\\n[O] Buffer (ARRAY): Buffer to receive segment entry (5 words / 10 bytes).",  /* Parameter details */
        mon_53B_GetSegmentEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        44,           /* MON number (decimal) */
        "54B",         /* Octal string */
        "MDLFI",    /* Short name */
        "DeleteFile",          /* Long name */
        "Deletes a file. The pages of the file are released.\n\n- You must have directory access to the file in",  /* Description */
        "[I] FileName (STRING): File name to delete. Include version number to delete specific version, otherwise all versions deleted.",  /* Parameter details */
        mon_54B_DeleteFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        45,           /* MON number (decimal) */
        "55B",         /* Octal string */
        "RSQPE",    /* Short name */
        "GetSpoolingEntry",          /* Long name */
        "Gets the next spooling queue entry, that is, the next file to be printed. The entry is removed from ",  /* Description */
        "[I] SpoolDevNumber (INTEGER): Logical device number of the printer.\\n[O] Buffer (ARRAY): 272-byte spooling entry: [0:1]=copies, [2:3]=apostrophe flag, [4:97]=filename, [98:255]=message.",  /* Parameter details */
        mon_55B_GetSpoolingEntry,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        46,           /* MON number (decimal) */
        "56B",         /* Octal string */
        "PASET",    /* Short name */
        "SetUserParam",          /* Long name */
        "Sets information about a background program. Use GetUserParam to read the 5 parameters when a progra",  /* Description */
        "[I] The five user parameters. (ARRAY): This is an array of 16-bit integers on the ND-100. ND-500 uses an array of 32-bit integers. SINTRAN III's termination handling returns the following: Parameter 1: The last byte contains the user index. The byte in front of it contains the directory index. 2: Logical device number of the terminal. 3: Fatal error or the monitor call ErrorMessage returns the error number. If ESCAPE was pressed, -1 is returned. 4: Set by SetUserParam. 5: Set by SetUserParam.",  /* Parameter details */
        mon_56B_SetUserParam,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        47,           /* MON number (decimal) */
        "57B",         /* Octal string */
        "PAGEI",    /* Short name */
        "GetUserParam",          /* Long name */
        "Gets information about why the last program terminated. There are 5 parameters for each background u",  /* Description */
        "[O] Buff (INTEGER2[5]): 5-word buffer for user parameters. [0]=dir/user index, [1]=terminal LDN, [2]=error number (-1 if escape), [3-4]=user defined.",  /* Parameter details */
        mon_57B_GetUserParam,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        5,           /* MON number (decimal) */
        "5B",         /* Octal string */
        "RDISK",    /* Short name */
        "ReadScratchFile",          /* Long name */
        "Reads randomly from the scratch file. One block is transferred. There is one scratch file connected ",  /* Description */
        "[I] BlockNumber (INTEGER): Block number to start reading from. First block is 0.\\n[O] DataDestination (ARRAY): Buffer address for the transferred data (256 words / 512 bytes).",  /* Parameter details */
        mon_5B_ReadScratchFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        49,           /* MON number (decimal) */
        "61B",         /* Octal string */
        "FIXC5",    /* Short name */
        "MemoryAllocation",          /* Long name */
        "Fixes or unfixes ND-100 segments to be used by the ND-500 Monitor. You may also reserve a contiguous",  /* Description */
        "[I] FuncCode (INTEGER): Function code. Function codes 4, 5 and 6 are the only values allowed. The functions and parameters are described on the third page for this call.\\n[I/O] Param2 (INTEGER): Parameter 2. Depends on the function code.\\n[I/O] Param3 (INTEGER): Parameter 3. Depends on the function code.\\n[I/O] Param4 (INTEGER): Parameter 4. Depends on the function code.\\n[I/O] Param5 (INTEGER): Parameter 5. Depends on the function code.\\n[I/O] Param6 (INTEGER): Parameter 6. Depends on the function code.",  /* Parameter details */
        mon_61B_MemoryAllocation,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        6             /* Param count */
    );
    mon_register_ex(
        50,           /* MON number (decimal) */
        "62B",         /* Octal string */
        "RMAX",    /* Short name */
        "GetBytesInFile",          /* Long name */
        "Gets the number of bytes in a file. Only the bytes containing data are counted.",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[O] NoOfBytes (INTEGER4): Number of bytes in the file (32-bit value).",  /* Parameter details */
        mon_62B_GetBytesInFile,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        51,           /* MON number (decimal) */
        "63B",         /* Octal string */
        "B41NW",    /* Short name */
        "In4x2Bytes",          /* Long name */
        "Reads 8 bytes from a word-oriented or character-oriented device, e.g. internal devices.\n\n- Do not us",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number. See appendix B.\\n[O] DataRead (BYTES[8]): The 8 bytes of data read (output).\\n[O] NoOfBytes (INTEGER2): Number of bytes read (output, returned in W1).",  /* Parameter details */
        mon_63B_In4x2Bytes,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        3             /* Param count */
    );
    mon_register_ex(
        52,           /* MON number (decimal) */
        "64B",         /* Octal string */
        "ERMSG",    /* Short name */
        "WarningMessage",          /* Long name */
        "Outputs a file system error message. Appendix A shows the messages connected to each error code. The",  /* Description */
        "[I] ErrCode (INTEGER): Error code number. Error code 0 is illegal. See appendix A.",  /* Parameter details */
        mon_64B_WarningMessage,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        53,           /* MON number (decimal) */
        "65B",         /* Octal string */
        "QERMS",    /* Short name */
        "ErrorMessage",          /* Long name */
        "Displays a file system error message. Appendix A shows the messages connected to each error number. ",  /* Description */
        "[I] ErrNumber (INTEGER): Error number of message to display. Do not input error number 0.",  /* Parameter details */
        mon_65B_ErrorMessage,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        54,           /* MON number (decimal) */
        "66B",         /* Octal string */
        "ISIZE",    /* Short name */
        "InBufferSpace",          /* Long name */
        "Gets the current number of bytes in the input buffer. Terminals and other character devices place in",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number. See appendix B. Use 1 for your own terminal.\\n[O] NoOfBytes (INTEGER2): Number of bytes in the input buffer (output, returned in W1).",  /* Parameter details */
        mon_66B_InBufferSpace,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        55,           /* MON number (decimal) */
        "67B",         /* Octal string */
        "OSIZE",    /* Short name */
        "OutBufferSpace",          /* Long name */
        "Gets the number of free bytes in the output buffer (number of bytes which can be written before the ",  /* Description */
        "[I] DeviceNumber (INTEGER2): Logical device number. See appendix B. Use 1 for your own terminal.\\n[O] NoOfBytes (INTEGER2): Number of free bytes in output buffer (output, returned in W1).",  /* Parameter details */
        mon_67B_OutBufferSpace,  /* Handler */
        MON_STATUS_IN_PROGRESS,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        6,           /* MON number (decimal) */
        "6B",         /* Octal string */
        "WDISK",    /* Short name */
        "WriteScratchFile",          /* Long name */
        "Writes randomly to the scratch file. One block is transferred. There is one scratch file connected t",  /* Description */
        "[I] BlockNumber (INTEGER): Block number to start writing to. First block is 0.\\n[I] Buffer (ARRAY): Buffer address containing data to write (256 words / 512 bytes).",  /* Parameter details */
        mon_6B_WriteScratchFile,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        56,           /* MON number (decimal) */
        "70B",         /* Octal string */
        "COMMND",    /* Short name */
        "CallCommand",          /* Long name */
        "Executes a SINTRAN III command from a program. The program terminates if an error occurs in the comm",  /* Description */
        "[I] Command (STRING): SINTRAN III command string to execute (up to 80 characters).",  /* Parameter details */
        mon_70B_CallCommand,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        57,           /* MON number (decimal) */
        "71B",         /* Octal string */
        "DESCF",    /* Short name */
        "DisableEscape",          /* Long name */
        "The ESCAPE key on the terminal normally terminates a program. This is called user break. This monito",  /* Description */
        "[I] DeviceNumber (INTEGER): The terminal's logical device number. This parameter is ignored for background programs. Your own terminal is always selected.",  /* Parameter details */
        mon_71B_DisableEscape,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        58,           /* MON number (decimal) */
        "72B",         /* Octal string */
        "EESCF",    /* Short name */
        "EnableEscape",          /* Long name */
        "Enables the ESCAPE key on the terminal. The ESCAPE key normally terminates a program. This is called",  /* Description */
        "[I] DeviceNumber (INTEGER2): The terminal's logical device number. Ignored for background programs (own terminal selected).",  /* Parameter details */
        mon_72B_EnableEscape,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        1             /* Param count */
    );
    mon_register_ex(
        59,           /* MON number (decimal) */
        "73B",         /* Octal string */
        "SMAX",    /* Short name */
        "SetMaxBytes",          /* Long name */
        "Sets the value of the maximum byte pointer in an opened file (i.e. the number of bytes minus 1). The",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[I] MaxBytePointer (INTEGER4): Maximum file size in bytes (32-bit value).",  /* Parameter details */
        mon_73B_SetMaxBytes,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        60,           /* MON number (decimal) */
        "74B",         /* Octal string */
        "SETBT",    /* Short name */
        "SetStartByte",          /* Long name */
        "Sets the next byte to be read or written in an opened mass-storage file.\n\n- The bytes in a file are ",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[I] BytePointer (INTEGER4): Start byte in the file (32-bit value). First byte is 0.",  /* Parameter details */
        mon_74B_SetStartByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        61,           /* MON number (decimal) */
        "75B",         /* Octal string */
        "REABT",    /* Short name */
        "GetStartByte",          /* Long name */
        "Gets the number of the next byte to access in a file. The bytes in a file are numbered from 0.\n\n- Th",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[O] BytePointer (INTEGER4): The number of the next byte to access (32-bit value).",  /* Parameter details */
        mon_75B_GetStartByte,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        62,           /* MON number (decimal) */
        "76B",         /* Octal string */
        "SETBS",    /* Short name */
        "SetBlockSize",          /* Long name */
        "Sets the block size of an opened file. Monitor calls which read randomly from, or write randomly to ",  /* Description */
        "[I] FileNumber (INTEGER2): File number. See OpenFile.\\n[I] BlockSize (LONGINT): Block size in bytes. Must be an even number. Factors of 2048 are most efficient.",  /* Parameter details */
        mon_76B_SetBlockSize,  /* Handler */
        MON_STATUS_VALIDATED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        63,           /* MON number (decimal) */
        "77B",         /* Octal string */
        "SETBL",    /* Short name */
        "SetStartBlock",          /* Long name */
        "Sets the next block to be read or written in an opened file. You may access the first bytes in the b",  /* Description */
        "[I] FileNumber (INTEGER): File number. See OpenFile.\\n[I] BlockNumber (INTEGER): Block number. Next block to be read or written. First block is 0.",  /* Parameter details */
        mon_77B_SetStartBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        2             /* Param count */
    );
    mon_register_ex(
        7,           /* MON number (decimal) */
        "7B",         /* Octal string */
        "RPAGE",    /* Short name */
        "ReadBlock",          /* Long name */
        "Reads randomly from a file. You read one block at a time. The file must be opened for random read ac",  /* Description */
        "[I] FileNumber (INTEGER2): File number. See OpenFile.\\n[I] BlockNo (INTEGER2): Block number.\\n[O] DataDestination (ARRAY): Array for data returned.\\n[O] ErrCode (INTEGER2): Standard Error Code. See appendix A.",  /* Parameter details */
        mon_7B_ReadBlock,  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        4             /* Param count */
    );
}
