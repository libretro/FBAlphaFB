#include "burnint.h"
#include "hd6309_intf.h"

#define MAX_CPU		8

INT32 nHD6309Count = 0;
static INT32 nActiveCPU = 0;

static HD6309Ext *HD6309CPUContext[MAX_CPU];

static INT32 nHD6309CyclesDone[MAX_CPU];
INT32 nHD6309CyclesTotal;

static UINT8 HD6309ReadByteDummyHandler(UINT16)
{
	return 0;
}

static void HD6309WriteByteDummyHandler(UINT16, UINT8)
{
}

static UINT8 HD6309ReadOpDummyHandler(UINT16)
{
	return 0;
}

static UINT8 HD6309ReadOpArgDummyHandler(UINT16)
{
	return 0;
}

void HD6309Reset()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Reset called without init\n"));
#endif

	hd6309_reset();
}

void HD6309NewFrame()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309NewFrame called without init\n"));
#endif

	for (INT32 i = 0; i < nHD6309Count; i++) {
		nHD6309CyclesDone[i] = 0;
	}
	nHD6309CyclesTotal = 0;
}

INT32 HD6309Init(INT32 nCPU)
{
	nActiveCPU = -1;
	nHD6309Count = nHD6309Count++ % MAX_CPU;

	if (DebugCPU_HD6309Initted == 0) {
		for (INT32 i = 0; i < MAX_CPU; i++) {
			HD6309CPUContext[i] = NULL;
		}
	}

	HD6309CPUContext[nCPU] = (HD6309Ext*)BurnMalloc(sizeof(HD6309Ext));
	if (HD6309CPUContext[nCPU] == NULL) {
		return 1;
	}

	memset(HD6309CPUContext[nCPU], 0, sizeof(HD6309Ext));
	
	{
		HD6309CPUContext[nCPU]->ReadByte = HD6309ReadByteDummyHandler;
		HD6309CPUContext[nCPU]->WriteByte = HD6309WriteByteDummyHandler;
		HD6309CPUContext[nCPU]->ReadOp = HD6309ReadOpDummyHandler;
		HD6309CPUContext[nCPU]->ReadOpArg = HD6309ReadOpArgDummyHandler;
		
		nHD6309CyclesDone[nCPU] = 0;
	
		for (INT32 j = 0; j < (0x0100 * 3); j++) {
			HD6309CPUContext[nCPU]->pMemMap[j] = NULL;
		}
	}
	
	nHD6309CyclesTotal = 0;

	if (DebugCPU_HD6309Initted == 0) {
		hd6309_init();
	}
	DebugCPU_HD6309Initted = 1;

	CpuCheatRegister(0x0006, nCPU);

	return 0;
}

void HD6309Exit()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Exit called without init\n"));
#endif

	nHD6309Count = 0;

	for (INT32 i = 0; i < MAX_CPU; i++) {
		if (HD6309CPUContext[i]) {
			BurnFree(HD6309CPUContext[i]);
		}
	}
	
	DebugCPU_HD6309Initted = 0;
}

void HD6309Open(INT32 num)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Open called without init\n"));
	if (num >= nHD6309Count) bprintf(PRINT_ERROR, _T("HD6309Open called with invalid index %x\n"), num);
	if (nActiveCPU != -1) bprintf(PRINT_ERROR, _T("HD6309Open called when CPU already open with index %x\n"), num);
	if (HD6309CPUContext[num] == NULL) bprintf (PRINT_ERROR, _T("HD6309Open called for unitialized CPU %x\n"), num);
#endif

	nActiveCPU = num;
	
	hd6309_set_context(&HD6309CPUContext[nActiveCPU]->reg);
	
	nHD6309CyclesTotal = nHD6309CyclesDone[nActiveCPU];
}

void HD6309Close()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Close called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309Close called when no CPU open\n"));
#endif

	hd6309_get_context(&HD6309CPUContext[nActiveCPU]->reg);
	
	nHD6309CyclesDone[nActiveCPU] = nHD6309CyclesTotal;
	
	nActiveCPU = -1;
}

INT32 HD6309GetActive()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309GetActive called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309GetActive called when no CPU open\n"));
#endif

	return nActiveCPU;
}

void HD6309SetIRQ(INT32 vector, INT32 status)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309SetIRQ called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309SetIRQ called when no CPU open\n"));
#endif

	if (status == HD6309_IRQSTATUS_NONE) {
		hd6309_set_irq_line(vector, 0);
	}
	
	if (status == HD6309_IRQSTATUS_ACK) {
		hd6309_set_irq_line(vector, 1);
	}
	
	if (status == HD6309_IRQSTATUS_AUTO) {
		hd6309_set_irq_line(vector, 1);
		hd6309_execute(0);
		hd6309_set_irq_line(vector, 0);
		hd6309_execute(0);
	}
}

INT32 HD6309Run(INT32 cycles)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Run called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309Run called when no CPU open\n"));
#endif

	cycles = hd6309_execute(cycles);
	
	nHD6309CyclesTotal += cycles;
	
	return cycles;
}

void HD6309RunEnd()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309RunEnd called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309RunEnd called when no CPU open\n"));
#endif
}

INT32 HD6309GetPC()
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309GetPC called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309GetPC called when no CPU open\n"));
#endif

	return hd6309_get_pc();
}

INT32 HD6309MapMemory(UINT8* pMemory, UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309MapMemory called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309MapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = HD6309CPUContext[nActiveCPU]->pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & HD6309_READ)	{
			pMemMap[0     + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & HD6309_WRITE) {
			pMemMap[0x100 + i] = pMemory + ((i - cStart) << 8);
		}
		if (nType & HD6309_FETCH) {
			pMemMap[0x200 + i] = pMemory + ((i - cStart) << 8);
		}
	}
	return 0;

}

INT32 HD6309MemCallback(UINT16 nStart, UINT16 nEnd, INT32 nType)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309MemCallback called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309MemCallback called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = HD6309CPUContext[nActiveCPU]->pMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nType & HD6309_READ)	{
			pMemMap[0     + i] = NULL;
		}
		if (nType & HD6309_WRITE) {
			pMemMap[0x100 + i] = NULL;
		}
		if (nType & HD6309_FETCH) {
			pMemMap[0x200 + i] = NULL;
		}
	}
	return 0;

}

void HD6309SetReadByteHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309SetReadByteHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309SetReadByteHandler called when no CPU open\n"));
#endif

	HD6309CPUContext[nActiveCPU]->ReadByte = pHandler;
}

void HD6309SetWriteByteHandler(void (*pHandler)(UINT16, UINT8))
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309SetWriteByteHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309SetWriteByteHandler called when no CPU open\n"));
#endif

	HD6309CPUContext[nActiveCPU]->WriteByte = pHandler;
}

void HD6309SetReadOpHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309SetReadOpHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309SetReadOpHandler called when no CPU open\n"));
#endif

	HD6309CPUContext[nActiveCPU]->ReadOp = pHandler;
}

void HD6309SetReadOpArgHandler(UINT8 (*pHandler)(UINT16))
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309SetReadOpArgHandler called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309SetReadOpArgHandler called when no CPU open\n"));
#endif

	HD6309CPUContext[nActiveCPU]->ReadOpArg = pHandler;
}

UINT8 HD6309ReadByte(UINT16 Address)
{
	// check mem map
	UINT8 * pr = HD6309CPUContext[nActiveCPU]->pMemMap[0x000 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (HD6309CPUContext[nActiveCPU]->ReadByte != NULL) {
		return HD6309CPUContext[nActiveCPU]->ReadByte(Address);
	}
	
	return 0;
}

void HD6309WriteByte(UINT16 Address, UINT8 Data)
{
	// check mem map
	UINT8 * pr = HD6309CPUContext[nActiveCPU]->pMemMap[0x100 | (Address >> 8)];
	if (pr != NULL) {
		pr[Address & 0xff] = Data;
		return;
	}
	
	// check handler
	if (HD6309CPUContext[nActiveCPU]->WriteByte != NULL) {
		HD6309CPUContext[nActiveCPU]->WriteByte(Address, Data);
		return;
	}
}

UINT8 HD6309ReadOp(UINT16 Address)
{
	// check mem map
	UINT8 * pr = HD6309CPUContext[nActiveCPU]->pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (HD6309CPUContext[nActiveCPU]->ReadOp != NULL) {
		return HD6309CPUContext[nActiveCPU]->ReadOp(Address);
	}
	
	return 0;
}

UINT8 HD6309ReadOpArg(UINT16 Address)
{
	// check mem map
	UINT8 * pr = HD6309CPUContext[nActiveCPU]->pMemMap[0x200 | (Address >> 8)];
	if (pr != NULL) {
		return pr[Address & 0xff];
	}
	
	// check handler
	if (HD6309CPUContext[nActiveCPU]->ReadOpArg != NULL) {
		return HD6309CPUContext[nActiveCPU]->ReadOpArg(Address);
	}
	
	return 0;
}

void HD6309WriteRom(UINT16 Address, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309WriteRom called without init\n"));
	if (nActiveCPU == -1) bprintf(PRINT_ERROR, _T("HD6309WriteRom called when no CPU open\n"));
#endif

	// check mem map
	UINT8 * pr = HD6309CPUContext[nActiveCPU]->pMemMap[0x000 | (Address >> 8)];
	UINT8 * pw = HD6309CPUContext[nActiveCPU]->pMemMap[0x100 | (Address >> 8)];
	UINT8 * pf = HD6309CPUContext[nActiveCPU]->pMemMap[0x200 | (Address >> 8)];

	if (pr != NULL) {
		pr[Address & 0xff] = Data;
	}
	
	if (pw != NULL) {
		pw[Address & 0xff] = Data;
	}

	if (pf != NULL) {
		pf[Address & 0xff] = Data;
	}

	// check handler
	if (HD6309CPUContext[nActiveCPU]->WriteByte != NULL) {
		HD6309CPUContext[nActiveCPU]->WriteByte(Address, Data);
		return;
	}
}

INT32 HD6309Scan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugCPU_HD6309Initted) bprintf(PRINT_ERROR, _T("HD6309Scan called without init\n"));
#endif

	struct BurnArea ba;
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	for (INT32 i = 0; i < nHD6309Count; i++) {
		char szName[] = "HD6309 #n";
		szName[7] = '0' + i;

		ba.Data = &HD6309CPUContext[i];
		ba.nLen = sizeof(HD6309CPUContext[i]);
		ba.szName = szName;
		BurnAcb(&ba);
	}
	
	return 0;
}
