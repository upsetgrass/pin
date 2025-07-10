/*
    make obj-intel64/first_test.so TARGET=intel64
    pin -t obj-intel64/first_test.so -- ../target_dir/elf_dir/hello.elf
    cat first_test.out
*/
#include<iostream>
#include <fstream>
#include "pin.H"
#include <map>
#include <vector>
using std::cout;
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;
using std::map;
using std::vector;

static uint icount_runtime = 0;
static uint icount_static  = 0;
static uint tcount_runtime = 0;
static uint tcount_static  = 0;
static uint bcount_runtime = 0;
static uint bcount_static  = 0;


typedef struct statistical_information{
    map<string, uint> map_mnemonic;
    map<INT32, uint> map_ins_tpye;

    // 可以根据各种信息进行分类...
} s_inf;

static s_inf SInf;

template <typename T>
void print_order_tree1(const T& map)
{
    std::vector<std::pair<string, int>> vec(map.begin(), map.end());
    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    cout << "|--- map_mnemonic_size: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        cout << key << ": " << value << endl;
    }
    cout << endl;
    
}
template <typename T>
void print_order_tree2(const T& map)
{
    std::vector<std::pair<INT32, int>> vec(map.begin(), map.end());

    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 降序
    });
    cout << "|--- map_ins_tpye_size: " << vec.size() << " ---|" << endl;
    for (const auto& [key, value] : vec) {
        cout << CATEGORY_StringShort(key) << ": " << value << endl;
        // cout << xed_category_enum_t2str(key) << ": " << value << endl;
    }
    cout << endl;    
    
}

void doicount() { icount_runtime++; }
void dobcount() { bcount_runtime++; }
void dotcount() { tcount_runtime++; }
VOID get_ins(INS ins, VOID* v)
{
    icount_static++;
    // 助记符
    string mnemonic = INS_Mnemonic(ins);
    if(SInf.map_mnemonic.find(mnemonic) != SInf.map_mnemonic.end()){ SInf.map_mnemonic[mnemonic]++; }
    else { SInf.map_mnemonic[mnemonic] = 1; }
    // 指令类型
    // xed_category_enum_t ins_type = INS_Category(ins);
    INT32 ins_type = static_cast<INT32>(INS_Category(ins));
    if(SInf.map_ins_tpye.find(ins_type) != SInf.map_ins_tpye.end()){ SInf.map_ins_tpye[ins_type]++; }
    else { SInf.map_ins_tpye[ins_type] = 1; }

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)doicount, IARG_END); // 调用INS_InsertCall才是在runtime阶段调用回调
}

VOID trace(TRACE trace, VOID* v)
{
    tcount_static++;
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)dotcount, IARG_END);
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        bcount_static++;
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)dobcount, IARG_END);
    }
}

void check()
{
    std::vector<std::pair<string, int>> vec_mnemonic(SInf.map_mnemonic.begin(), SInf.map_mnemonic.end());
    std::vector<std::pair<INT32, int>> vec_ins_tpye(SInf.map_ins_tpye.begin(), SInf.map_ins_tpye.end());

    uint sum1 = 0, sum2 = 0;
    for (const auto& [key, value] : vec_mnemonic) { sum1 += value; }
    for (const auto& [key, value] : vec_ins_tpye) { sum2 += value; }
    cout << "sum_mnemonic: " << sum1 << endl;
    cout << "sum_ins_type: " << sum2 << endl;
     // 整个程序统计的各种指令类型、助记符得到的总结果和静态统计的指令条数相同
    if(sum1 == sum2 && sum1 == icount_static) cout << "|--- the result right! ---|" << endl << endl;
}
VOID fini(INS ins, VOID* v)
{
    cout << endl;
    cout << "insturction_runtime:" << icount_runtime << endl;
    cout << "insturction_static:" << icount_static << endl;
    cout << "basicblock_runtime:" << bcount_runtime << endl; 
    cout << "basicblock_static:" << bcount_static << endl;
    cout << "trace_runtime:" << tcount_runtime << endl;
    cout << "trace_static:" << tcount_static << endl << endl;;
    
    check();

    print_order_tree1(SInf.map_mnemonic);
    
    print_order_tree2(SInf.map_ins_tpye);


}

int main(int argc, char* argv[])
{
    // 初始化
    PIN_InitSymbols();
    if(PIN_Init(argc, argv) == 0) cout << "init_success!" << endl;
    else cerr << "init_err,try agein or check agein" << endl;

    // 插桩
    INS_AddInstrumentFunction((INS_INSTRUMENT_CALLBACK)get_ins, 0);
    TRACE_AddInstrumentFunction((TRACE_INSTRUMENT_CALLBACK)trace, 0);
    PIN_AddFiniFunction((FINI_CALLBACK)fini, 0);

    // 启动
    PIN_StartProgram();
    return 0;
}