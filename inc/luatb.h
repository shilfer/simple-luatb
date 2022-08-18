#ifndef LUATB__H
#define LUATB__H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"

#include "vpi/vpi_user.h"
#include "vpi/sv_vpi_user.h"
#include "vpi/acc_vhdl.h"

#define LUATB_SIGNALHANDLE_TYPENAME "SignalHandle"

#define LUATB_CBTYPE_SIMTIME_WHEN       0
#define LUATB_CBTYPE_SIMTIME_INTERVAL   1
#define LUATB_CBTYPE_SIGNAL_CHANGE      2

// luatb运行时结构体
typedef struct T_LUATB_RUNTIME_ENVIRON  s_luatb_rtenv, *p_luatb_rtenv;
// luatb回调函数结构体
typedef struct T_LUATB_CALLBACK_CONTEXT s_luatb_cbctx, *p_luatb_cbctx;

struct T_LUATB_RUNTIME_ENVIRON {
    lua_State*  L;
    vpiHandle   UUT;
    // 保存所有callback_context
    int         cb_table_ref;
};

struct T_LUATB_CALLBACK_CONTEXT {
    lua_State*  L;

    int         cb_type;
    vpiHandle   cb_object;
    int         cb_func_ref;
    int         cb_data_ref;
    PLI_UINT64  cb_time;
    const char* cb_value;
    vpiHandle   cb_handle;
};

// VPI函数：获取仿真时间
PLI_UINT64 luatb_vpi_get_sim_time();
PLI_INT32  luatb_vpi_get_time_unit();
// VPI函数：仿真结束回调
PLI_INT32  luatb_vpi_end_of_sim_callback(p_cb_data);
// VPI函数：luatb_start系统函数的compiletf
PLI_INT32  luatb_vpi_systf_luatb_start_compiletf(PLI_BYTE8*);
// VPI函数：luatb_start系统函数的calltf
PLI_INT32  luatb_vpi_systf_luatb_start_calltf(PLI_BYTE8*);
// VPI函数: luatb_start系统函数的注册函数
void       luatb_vpi_register_systf_luatb_start();

// luatb库初始化
void       luatb_lua_initial_luatb_libs(p_luatb_rtenv);

// 打印函数：错误[luatb:E:T]
void luatb_info_error(const char*, ...);
// 打印函数：信息[luatb:I:T]
void luatb_info_info(const char*, ...);
// 打印函数：警告[luatb:W:T]
void luatb_info_warning(const char*, ...);
// 打印函数：调试[luatb:D:T]
void luatb_info_debug(const char*, ...);

// 回调函数：注册回调的readonly同步函数
PLI_INT32 luatb_vpi_callback_readonlysync_for_register(p_cb_data);
// 回调函数：signal change回调
PLI_INT32 luatb_vpi_callback_middlefunc_for_signal_change(p_cb_data);
// 回调函数：调用回调函数的readwrite同步函数
PLI_INT32 luatb_vpi_callback_readwritesync_for_cbcall(p_cb_data);

#endif