#ifndef _TRACE_
#define _TRACE_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <signal.h>
#pragma comment (lib, "dbghelp.lib")

void
stack_trace (void)
{
    struct sym_pack
    {
        SYMBOL_INFO sym;
        char name[255];
    } sym_pack;
    void *stack[64] = {0};

    SYMBOL_INFO *symbol = &sym_pack.sym;
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof (SYMBOL_INFO);

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof (IMAGEHLP_LINE64);

    HANDLE process = GetCurrentProcess ();
    SymInitialize (process, NULL, true);
    SymSetOptions (SYMOPT_LOAD_LINES);

    unsigned short frame_count = CaptureStackBackTrace (0, 100, stack, NULL);

    printf ("----------------------------------------\n");
    printf ("Call Stack:\n");
    printf ("----------------------------------------\n");
    for (int i = 1; i < frame_count; i++)
    {
        DWORD dwDisplacement;

        SymFromAddr (process, (DWORD64) (stack[i]), 0, symbol);
        SymGetLineFromAddr (process, (DWORD64) (stack[i]), &dwDisplacement, &line);

        printf ("0x%0llX %i: %s\t(%s:%d)\n",
                symbol->Address, frame_count - i - 1,
                symbol->Name, line.FileName, line.LineNumber);
    }
    printf ("----------------------------------------\n");
}

#endif
