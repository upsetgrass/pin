/*
    make obj-intel64/first_test.so TARGET=intel64
    pin -t obj-intel64/first_test.so -- ../target_dir/elf_dir/hello.elf
    cat first_test.out
*/
#include <iostream>
#include <fstream>
#include "pin.H"
#include <map>
#include <set>
#include <vector>
#include <fstream>
// #include "extras/crt/include/types.h"
using std::cout;
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;
using std::map;
using std::vector;
using std::set;
using std::ofstream;

typedef struct instruction_field{
    string mnemonic;    // 助记符
    OPCODE opcode;      // 指令编码     OPCODE_StringShort->string
    INT32 category;     // 所属的粗分类  CATEGORY_StringShort->string
    INT32 ins_ext;      // 指令集扩展    EXTENSION_StringShort->string
    string ins_disassemble; // 一条指令反汇编成汇编字符串
    ADDRINT ins_address;    // 指令地址，作为cpu执行时的pc
    USIZE ins_size;         // 指令字节大小
    
    UINT32 INS_OperandWidth;            // 操作数位宽bits位宽
    UINT32 INS_OperandCount;            // 操作数个数
    UINT32 INS_OperandSize;             // 操作数字节大小

    uint reg_cnt;       // 寄存器个数
    uint opelement_cnt; // 向量元素个数（标量该值为1）
    
    // 可寻址的地址范围，也就是生成地址参与的寄存器/偏移的位宽, eg:
    // Effective Address = Segment + Base + Index*Scale + Displacement
    UINT32 ins_effAddrWidth;
    BOOL ins_ismemoryvector;            // 是否有向量化内存访问
    REG ins_memorybasereg;              // 如果不存在，返回REG_INVALID()
    REG ins_memoryindexreg;             // 如果不存在，返回REG_INVALID()
    UINT32 ins_memoryscale;             // 如果没有index，scale也会返回1
    ADDRDELTA ins_memorydisplacement;   // 不存在时返回0
    UINT32 ins_memoryoperandcount;      // 内存操作数
    BOOL ins_ismemoryread;              // 是否内存读
    BOOL ins_ismemorywrite;             // 是否内存写
    
    // 流程控制属性
    BOOL ins_isbranch;
    BOOL ins_isdirectCall;
    BOOL ins_iscall;
    BOOL ins_isret;
    BOOL ins_isinterrupt;
    BOOL ins_issyscall;
} ins_all_filed;
typedef class instruction_our_field{
 public:
    string mnemonic;
    OPCODE opcode;
    UINT32 size;
    UINT32 ins_type;
    UINT32 ins_opcnt;
    instruction_our_field(INS ins)
    {
        string _mnemonic  = INS_Mnemonic(ins);
        OPCODE _opcode    = INS_Opcode(ins);
        UINT32 _size      = INS_Size(ins);
        UINT32 _ins_type  = INS_Category(ins);
        UINT32 _ins_opcnt = INS_OperandCount(ins);
        this->mnemonic  = _mnemonic;
        this->opcode    = _opcode;
        this->size      = _size;
        this->ins_type  = _ins_type;
        this->ins_opcnt = _ins_opcnt;
    }
} ins_filed;

typedef class dynamic_inf{
 public:
    set<ADDRINT> set_pc; // 用于确定某条指令是否被分析过
    map<string, uint> map_mnemonic;
    map<UINT32, uint> map_ins_tpye;
    map<OPCODE, uint> map_opcode;
    map<UINT32, uint> map_size;
    map<UINT32, uint> map_opcnt;
} dyn_inf;
typedef class static_inf{
 public:
    map<string, uint> map_mnemonic;
    map<UINT32, uint> map_ins_tpye;
    map<OPCODE, uint> map_opcode;
    map<UINT32, uint> map_size;
    map<UINT32, uint> map_opcnt;
} sta_inf;
typedef struct statistical_information{
 public:
    dyn_inf DynInf; // 动态阶段的执行流情况下的指令执行情况
    sta_inf StaInf; // 静态所有代码扫一遍的情况
    
    // 运行时执行的指令、指令流、基本块的个数
    uint icount_runtime = 0;
    uint tcount_runtime = 0;
    uint bcount_runtime = 0;
    // 执行前的指令、指令流、基本块的个数
    uint icount_static = 0;
    uint tcount_static = 0;
    uint bcount_static = 0;

    // 可以根据各种信息进行统计...
} s_inf;
static s_inf SInf;

void write_map_string(const map<string, uint>& map, const string& filename) {
    vector<std::pair<string, uint>> vec(map.begin(), map.end());
    sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    ofstream ofs(filename.c_str());
    if (!ofs) {
        cerr << "Failed to open file" << endl;
        return;
    }
    ofs << "|--- map_mnemonic_size: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        ofs << key << ": " << value << endl;
    }
    ofs.close();
}
void write_map_Opcode(const map<OPCODE, uint>& map, const string& filename) {
    vector<std::pair<OPCODE, uint>> vec(map.begin(), map.end());
    sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    ofstream ofs(filename.c_str());
    if (!ofs) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }
    ofs << "|--- map_opcode: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        ofs << key << ": " << value << endl;
    }
    ofs.close();
}

void write_map_uint(const map<UINT32, uint>& map, const string& filename, string str) {
    vector<std::pair<UINT32, uint>> vec(map.begin(), map.end());
    sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    ofstream ofs(filename.c_str());
    if (!ofs) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }
    if(str == "map_ins_type"){
        ofs << "|--- map_ins_type: " << vec.size() << " ---|" << endl;
    }else if(str == "map_size"){
        ofs << "|--- map_size: " << vec.size() << " ---|" << endl;
    }else if(str == "map_opcnt"){
        ofs << "|--- map_opcnt: " << vec.size() << " ---|" << endl;
    }
    for (const auto& [key, value] : vec) {
        ofs << key << ": " << value << endl;
    }
    ofs.close();
}

template <typename T>
void print_order_tree_string(const T& map)
{
    vector<std::pair<string, uint>> vec(map.begin(), map.end());
    sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    cout << "|--- map_mnemonic_size: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        cout << key << ": " << value << endl;
    }
    cout << endl;
}
template <typename T>
void print_order_tree_UINT32(const T& map, string str)
{
    vector<std::pair<UINT32, uint>> vec(map.begin(), map.end());
    sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    cout << "|--- " << str << ": " << vec.size() << " ---|" << endl;
    if(str == "map_ins_type")
    {
        for (const auto& [key, value] : vec) {
            cout << CATEGORY_StringShort(key) << ": " << value << endl;
        }
        cout << endl;
    }else if(str == "map_size"){
        for (const auto& [key, value] : vec) {
            cout << "ins_size->" << key << ": " << value << endl;
        }
        cout << endl;
    }else if(str == "map_opcnt"){
        for (const auto& [key, value] : vec) {
            cout << "ins_opcnt->" << key << ": " << value << endl;
        }
        cout << endl;
    }
    
}
template <typename T>
void print_order_tree_OPCODE(const T& map)
{
    std::vector<std::pair<OPCODE, uint>> vec(map.begin(), map.end());

    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    cout << "|--- map_Opcode: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        cout << OPCODE_StringShort(key) << ": " << value << endl;
    }
    cout << endl;
}


void doicount() { SInf.icount_runtime++; }
void dobcount() { SInf.bcount_runtime++; }
void dotcount() { SInf.tcount_runtime++; }


void DynamicAnalysisFunc(ADDRINT pc, string mnemonic, INT32 ins_type, OPCODE opcode, UINT32 size, UINT32 ins_opcnt)
{
    // 通过pc确定指令是被唯一分析的
    if(SInf.DynInf.set_pc.find(pc) == SInf.DynInf.set_pc.end()) //set中没有，说明没有被分析过
    {
        SInf.DynInf.set_pc.insert(pc);
        // // 执行过程中实际执行到的各种信息统计
        SInf.DynInf.map_mnemonic[mnemonic]++; // 助记符统计
        SInf.DynInf.map_ins_tpye[ins_type]++; // 指令类型统计
        SInf.DynInf.map_opcode[opcode]++; // Opcode统计
        SInf.DynInf.map_size[size]++; // size统计
        SInf.DynInf.map_opcnt[ins_opcnt]++;
    } 
    doicount();
}

void StaticAnalysisFunc(ins_filed InsFiled)
{
    SInf.StaInf.map_mnemonic[InsFiled.mnemonic]++; // 助记符统计
    SInf.StaInf.map_ins_tpye[InsFiled.ins_type]++; // 指令类型统计
    SInf.StaInf.map_opcode[InsFiled.opcode]++; // Opcode统计
    SInf.StaInf.map_size[InsFiled.size]++; // size统计
    SInf.StaInf.map_opcnt[InsFiled.ins_opcnt]++;
}

VOID get_ins(INS ins, VOID* v)
{
    SInf.icount_static++;
    ins_filed InsFiled(ins);
    
    StaticAnalysisFunc(InsFiled); // 真正的静态扫一遍

    // // 动态分析
    // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)DynamicAnalysisFunc, // 调用INS_InsertCall才是在runtime阶段调用回调
    //     IARG_INST_PTR,  // pin默认存储当前指令的pc
    //     IARG_PTR, new string(mnemonic),
    //     IARG_UINT32, size,
    //     IARG_UINT32, opcode,
    //     IARG_UINT32, ins_type,
    //     IARG_END);
}
VOID trace(TRACE trace, VOID* v)
{
    SInf.tcount_static++;
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)dotcount, IARG_END);
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        SInf.bcount_static++;
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)dobcount, IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            // SInf.icount_static++;

            // ADDRINT pc = INS_Address();
            string mnemonic = INS_Mnemonic(ins);
            OPCODE opcode = INS_Opcode(ins);
            UINT32 size = INS_Size(ins);
            UINT32 ins_type = INS_Category(ins);       
            UINT32 ins_opcnt = INS_OperandCount(ins);
            // StaticAnalysisFunc(mnemonic, opcode, size, ins_type); // 真正的静态扫一遍

            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)DynamicAnalysisFunc,
                IARG_INST_PTR,  // pin默认存储当前指令的pc
                IARG_PTR, new string(mnemonic),
                IARG_UINT32, size,
                IARG_UINT32, opcode,
                IARG_UINT32, ins_type,
                IARG_UINT32, ins_opcnt,
                IARG_END);
        }
    }
}

void check()
{
    std::vector<std::pair<string, int>> vec_mnemonic(SInf.StaInf.map_mnemonic.begin(), SInf.StaInf.map_mnemonic.end());
    std::vector<std::pair<INT32, int>> vec_ins_tpye(SInf.StaInf.map_ins_tpye.begin(), SInf.StaInf.map_ins_tpye.end());

    uint sum1 = 0, sum2 = 0;
    for (const auto& [key, value] : vec_mnemonic) { sum1 += value; }
    for (const auto& [key, value] : vec_ins_tpye) { sum2 += value; }
    cout << "sum_mnemonic: " << sum1 << endl;
    cout << "sum_ins_type: " << sum2 << endl;
     // 整个程序统计的各种指令类型、助记符得到的总结果和静态统计的指令条数相同
    if(sum1 == sum2 && sum1 == SInf.icount_static) cout << "|--- the result right! ---|" << endl;
    cout << endl;

    // 或许可以在更多层面进行检查
}
VOID fini(INS ins, VOID* v)
{
    cout << endl;
    cout << "insturction_runtime:" << SInf.icount_runtime << endl;
    cout << "insturction_static:" << SInf.icount_static << endl;
    cout << "basicblock_runtime:" << SInf.bcount_runtime << endl; 
    cout << "basicblock_static:" << SInf.bcount_static << endl;
    cout << "trace_runtime:" << SInf.tcount_runtime << endl;
    cout << "trace_static:" << SInf.tcount_static << endl << endl;;
    
    check();

    cout << "|------ static map information ------|" << endl;
    print_order_tree_string(SInf.StaInf.map_mnemonic);
    print_order_tree_UINT32(SInf.StaInf.map_ins_tpye, "map_ins_type");
    print_order_tree_UINT32(SInf.StaInf.map_size, "map_size");
    print_order_tree_UINT32(SInf.StaInf.map_opcnt, "map_opcnt");
    // print_order_tree_OPCODE(SInf.StaInf.map_opcode); // 打印结果和mnemonic结果完全一致
    cout << endl;

    cout << "|------ dynamic map information ------|" << endl;
    print_order_tree_string(SInf.DynInf.map_mnemonic);
    print_order_tree_UINT32(SInf.DynInf.map_ins_tpye, "map_ins_type");
    print_order_tree_UINT32(SInf.DynInf.map_size, "map_size");
    print_order_tree_UINT32(SInf.DynInf.map_opcnt, "map_opcnt");
    // print_order_tree_OPCODE(SInf.DynInf.map_opcode);
    cout << endl;

    // 结果存入文件--静态结果
    write_map_string(SInf.StaInf.map_mnemonic, "./res/static_map_mnemonic.txt");
    write_map_uint(SInf.StaInf.map_ins_tpye, "./res/static_map_ins_tpye.txt", "map_ins_type");
    write_map_Opcode(SInf.StaInf.map_opcode, "./res/static_map_opcode.txt");
    write_map_uint(SInf.StaInf.map_size, "./res/static_map_size.txt", "map_size");
    write_map_uint(SInf.StaInf.map_opcnt, "./res/static_map_opcnt.txt", "map_opcnt");

    // 结果存入文件--动态结果
    write_map_string(SInf.DynInf.map_mnemonic, "./res/dynamic_map_mnemonic.txt");
    write_map_uint(SInf.DynInf.map_ins_tpye, "./res/dynamic_map_ins_tpye.txt", "map_ins_type");
    write_map_Opcode(SInf.DynInf.map_opcode, "./res/dynamic_map_opcode.txt");
    write_map_uint(SInf.DynInf.map_size, "./res/dynamic_map_size.txt", "map_size");
    write_map_uint(SInf.DynInf.map_opcnt, "./res/dynamic_map_opcnt.txt", "map_opcnt");
}

int main(int argc, char* argv[])
{
    PIN_InitSymbols();
    if(PIN_Init(argc, argv) == 0) cout << "init_success!" << endl;
    else cerr << "init_err,try agein or check agein" << endl;

    // 插桩
    INS_AddInstrumentFunction((INS_INSTRUMENT_CALLBACK)get_ins, 0); // 只要是指令就会统计，包括死代码不会执行的分支，但是包含的这些信息不会被执行
    TRACE_AddInstrumentFunction((TRACE_INSTRUMENT_CALLBACK)trace, 0); // 统计所有 trace
    PIN_AddFiniFunction((FINI_CALLBACK)fini, 0);

    // 启动
    PIN_StartProgram();
    return 0;
}

/*
问题待解决：
    针对first_test.cppd和mytool.cpp对map_mnemonic和map_ins_tpye的统计结果不同，对于first_test.cpp结果更大
    mytool.cpp:
    insturction_static:18684
    sum_mnemonic: 18684
    sum_ins_type: 18684

    first_test.cpp
    insturction_static:20327
    sum_mnemonic: 20327
    sum_ins_type: 20327
*/