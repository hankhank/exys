#pragma once

extern unsigned char std_string_exys;
extern unsigned int std_string_exys_len;

extern unsigned char std_signals_exys;
extern unsigned int std_signals_exys_len;

extern unsigned char std_memory_exys;
extern unsigned int std_memory_exys_len;

extern unsigned char std_logic_exys;
extern unsigned int std_logic_exys_len;

namespace Exys
{

struct StdLibEntry
{
    const char* name;
    unsigned char* data;
    unsigned int len;
};

StdLibEntry StdLibEntries[] =
{
    {"string.exys", &::std_string_exys, ::std_string_exys_len},
    {"signals.exys", &::std_signals_exys, ::std_signals_exys_len},
    {"memory.exys", &::std_memory_exys, ::std_memory_exys_len},
    {"logic.exys", &::std_logic_exys, ::std_logic_exys_len}
};

}
