/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <fstream>
#include "pin.H" // 主头文件，定义了所有Pintool所需的API、类型、宏等
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;

ofstream OutFile; // 用于输出结果

// 保存动态指令计数器，使用static为了可能的编译器优化，只在本编译单元使用
// 静态存储期-程序开始时分配，程序结束时销毁，不输入栈/堆，避免生成全局符号，从而减少符号表表项
// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 icount = 0;

// KNOB机制-命令行参数支持，这里支持了输出文件名，默认为inscoutn.out，命令行参数为"-o"
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");

// 该函数是被Pin插入到目标程序每条指令执行前的函数，类似于hook回调，这里就在每条指令前加入一个+1用于程序计数
// This function is called before every instruction is executed
VOID docount() { icount++; } 

// 指令级插桩函数（注册钩子逻辑）,这里是在当Pin解析每一条目标程序的指令时，在其执行之前调用docount()，从而实现指令计数
// ins参数是被解析的目标程序机器指令
// INS_InsertCall是对该条指令ins插入自定义函数调用
// IPOINT_BEFORE是表示插入的位置是执行指令之前
// （AFUNPTR）docount被插入的函数地址
// v是保留参数，用于传入上下文信息，让外部可以给该函数传入信息
// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID* v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    // IPOINT_BEFORE        - 常规分析-统计
    // IPOINT_AFTER         - 获取执行结果
    // IPOINT_TAKEN_BRANCH  - 针对分支指令，分支命中时分析
    // IPOINT_ANYWHERE      - 指令任意位置（JIT优化用）
    // AFUNPTR              - PIN提供的通用函数指针类型，docount的类型是VOID(*)()，不是Pin类型
    // IARG_END             - 是结尾标志
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
}

// 程序结束时的收尾处理，输出总指令数到文件中
// This function is called when the application exits
VOID Fini(INT32 code, VOID* v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase); // showbase-显示进制前缀
    OutFile << "Count " << icount << endl;
    OutFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
// 出错时的帮助信息
INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage(); // 初始化Pin环境，分析工具

    OutFile.open(KnobOutputFile.Value().c_str()); // 打开输出文件

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
