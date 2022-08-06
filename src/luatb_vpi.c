#include "luatb.h"

void (*vlog_startup_routines[])() = {
    luatb_vpi_register_systf_luatb_start,
    NULL};

PLI_UINT64 luatb_vpi_get_sim_time()
{
    s_vpi_time time;
    time.type = vpiSimTime;
    vpi_get_time(NULL, &time);
    PLI_UINT64 time_value, time_high, time_low;
    time_high = time.high << 32;
    time_low = time.low;
    time_value = time_high + time_low;
    return time_value;
}

PLI_INT32 luatb_vpi_end_of_sim_callback(p_cb_data cb_data)
{
    p_luatb_rtenv rtenv = (p_luatb_rtenv)cb_data->user_data;
    if (rtenv == NULL)
        return 0;
    if (rtenv->L != NULL) {
        lua_rawgeti(rtenv->L, LUA_REGISTRYINDEX, rtenv->cb_table_ref);
        int n = luaL_len(rtenv->L, -1);
        p_luatb_cbctx cbctx;
        for (int i = 1; i <= n; i++) {
            lua_geti(rtenv->L, -1, i);
            cbctx = (p_luatb_cbctx)lua_touserdata(rtenv->L, -1);
            if (cbctx != NULL) {
                // vpi_printf("[luatb:D] free callback:pointer %p\n", cbctx);
                if (cbctx->cb_value != NULL)
                    free(cbctx->cb_value);
                free(cbctx);
            }
            lua_pop(rtenv->L, 1);
        }
        lua_pop(rtenv->L, 1);
        lua_close(rtenv->L);
    }
    if (rtenv != NULL)
        free(rtenv);
    luatb_info_info("luatb runtime environment destroyed");
    return 0;
}

PLI_INT32 luatb_vpi_systf_luatb_start_compiletf(PLI_BYTE8 *ud)
{
    p_luatb_rtenv rtenv = (p_luatb_rtenv)ud;
    vpiHandle systf_handle, arg_iterator, arg_handle;
    PLI_INT32 arg_type;

    systf_handle = vpi_handle(vpiSysTfCall, NULL);
    if (systf_handle == NULL)
    {
        luatb_info_error("$luatb_start failed to obtain systf handle");
        vpi_control(vpiFinish, 0);
        return 0;
    }

    arg_iterator = vpi_iterate(vpiArgument, systf_handle);
    if (arg_iterator == NULL)
    {
        luatb_info_error("$luatb_start failed to obtain arguments handle");
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }
    // 第一个参数，是module类型的，指定操作的根模块
    arg_handle = vpi_scan(arg_iterator);
    if (arg_handle == NULL)
    {
        luatb_info_error("$luatb_start requires 2 arguments");
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }
    arg_type = vpi_get(vpiType, arg_handle);
    if (arg_type != vpiModule)
    {
        luatb_info_error("$luatb_start's #1 argument must be a module");
        vpi_release_handle(arg_handle);
        vpi_release_handle(arg_iterator);
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }
    // 第二个参数，是字符串变量类型，执行测试用例脚本
    arg_handle = vpi_scan(arg_iterator);
    if (arg_handle == NULL)
    {
        luatb_info_error("$luatb_start requires 2 arguments");
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }
    arg_type = vpi_get(vpiType, arg_handle);
    if (arg_type != vpiStringVar)
    {
        luatb_info_error("$luatb_start's #2 argument must be a string varible");
        vpi_release_handle(arg_handle);
        vpi_release_handle(arg_iterator);
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }
    // 检测第三个参数，只能接受2个参数
    arg_handle = vpi_scan(arg_iterator);
    if (arg_handle != NULL)
    {
        luatb_info_error(" $luatb_start can only have 2 arguments");
        vpi_release_handle(arg_handle);
        vpi_release_handle(arg_iterator);
        vpi_release_handle(systf_handle);
        vpi_control(vpiFinish, 0);
        return 0;
    }

    vpi_release_handle(arg_handle);
    vpi_release_handle(systf_handle);
    return 0;
}

PLI_INT32 luatb_vpi_systf_luatb_start_calltf(PLI_BYTE8 *ud)
{
    p_luatb_rtenv rtenv = (p_luatb_rtenv)ud;
    vpiHandle systf_handle, arg_iterator, uut_handle, filename_handle;
    s_vpi_value filename_value;
    // 获取函数及函数参数的句柄，在compiletf中已经进行了验证，这里不再验证
    systf_handle = vpi_handle(vpiSysTfCall, NULL);
    arg_iterator = vpi_iterate(vpiArgument, systf_handle);
    // 第一个参数，模块句柄
    uut_handle = vpi_scan(arg_iterator);
    // 第二个参数，脚本名句柄
    filename_handle = vpi_scan(arg_iterator);
    filename_value.format = vpiStringVal;
    vpi_get_value(filename_handle, &filename_value);

    // 创建LUA状态机
    rtenv->L = luaL_newstate();
    // 保存UUT句柄
    rtenv->UUT = uut_handle;
    // 存储callback的table
    lua_newtable(rtenv->L);
    rtenv->cb_table_ref = luaL_ref(rtenv->L, LUA_REGISTRYINDEX);
    // 将rtenv附加到L上，方便后续获取
    p_luatb_rtenv* p_rtenv = lua_getextraspace(rtenv->L);
    *p_rtenv = rtenv;
    // 为LUA添加标准库和vpi库
    luaL_openlibs(rtenv->L);
    luatb_lua_initial_luatb_libs(rtenv);

    luatb_info_info("luatb runtime environment created");

    // 加载脚本
    int ret = luaL_dofile(rtenv->L, filename_value.value.str);
    if (ret != LUA_OK)
    {
        luatb_info_error("error run script: %s", lua_tostring(rtenv->L, -1));
        lua_pop(rtenv->L, 1);   // 弹出错误信息
        vpi_control(vpiStop);
        return 0;
    }

    // 释放用到的句柄
    vpi_release_handle(systf_handle);
    vpi_release_handle(arg_iterator); 
    vpi_release_handle(filename_handle);

    return 0;
}

void luatb_vpi_register_systf_luatb_start()
{
    // 申请运行环境内存
    p_luatb_rtenv rtenv = (p_luatb_rtenv)malloc(sizeof(s_luatb_rtenv));
    memset(rtenv, NULL, sizeof(s_luatb_rtenv));

    // 注册仿真结束的回调
    s_cb_data cb_data_eos;
    vpiHandle cb_handle_eos;
    cb_data_eos.reason = cbEndOfSimulation;
    cb_data_eos.cb_rtn = luatb_vpi_end_of_sim_callback;
    cb_data_eos.obj = NULL;
    cb_data_eos.time = NULL;
    cb_data_eos.value = NULL;
    cb_data_eos.index = 0;
    cb_data_eos.user_data = (PLI_BYTE8 *)rtenv;
    cb_handle_eos = vpi_register_cb(&cb_data_eos);
    vpi_release_handle(cb_handle_eos);

    // 注册restart开始的回调
    // TODO: 暂时没有实现

    s_vpi_systf_data tf_data;
    tf_data.type = vpiSysFunc;
    tf_data.sysfunctype = vpiIntFunc;
    tf_data.tfname = "$luatb_start";
    tf_data.calltf = luatb_vpi_systf_luatb_start_calltf;
    tf_data.compiletf = luatb_vpi_systf_luatb_start_compiletf;
    tf_data.sizetf = NULL;
    tf_data.user_data = (PLI_BYTE8 *)rtenv;
    vpi_register_systf(&tf_data);
}
