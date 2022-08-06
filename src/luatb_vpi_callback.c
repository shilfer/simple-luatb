#include "luatb.h"

PLI_INT32 luatb_vpi_callback_readonlysync_for_register(p_cb_data cb_data)
{
    p_luatb_cbctx cbctx = (p_luatb_cbctx)cb_data->user_data;
    s_cb_data cb_data_new;
    s_vpi_time cb_time_new;
    s_vpi_value cb_value_new;
    vpiHandle cb_handle_new;
    if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_WHEN ||
        cbctx->cb_type == LUATB_CBTYPE_SIMTIME_INTERVAL)
    {
        PLI_UINT64 now = luatb_vpi_get_sim_time();
        PLI_UINT64 delay;
        if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_WHEN) {
            delay = cbctx->cb_time - now;
        } else if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_INTERVAL) {
            delay = cbctx->cb_time;
        }
        if (delay <= 0) {
            return 0;
        }
        cb_time_new.high = delay >> 32;
        cb_time_new.low = delay & 0xFFFFFFFF;
        cb_time_new.type = vpiSimTime;
        cb_data_new.reason = cbReadWriteSynch;
        cb_data_new.cb_rtn = luatb_vpi_callback_readwritesync_for_cbcall;
        cb_data_new.time = &cb_time_new;
        cb_data_new.user_data = (PLI_BYTE8 *)cbctx;
        cb_data_new.obj = NULL;
        cb_data_new.value = NULL;
        cb_data_new.index = 0;
        cb_handle_new = vpi_register_cb(&cb_data_new);
        vpi_release_handle(cb_handle_new);
    }
    else if (cbctx->cb_type == LUATB_CBTYPE_SIGNAL_CHANGE)
    {
        cb_value_new.format = vpiBinStrVal;
        cb_data_new.reason = cbValueChange;
        cb_data_new.cb_rtn = luatb_vpi_callback_middlefunc_for_signal_change;
        cb_data_new.time = NULL;
        cb_data_new.value = &cb_value_new;
        cb_data_new.obj = cbctx->cb_object;
        cb_data_new.index = 0;
        cb_data_new.user_data = (PLI_BYTE8*)cbctx;
        cb_handle_new = vpi_register_cb(&cb_data_new);
        cbctx->cb_handle = cb_handle_new;
    }
    return 0;
}

PLI_INT32 luatb_vpi_callback_middlefunc_for_signal_change(p_cb_data cb_data)
{
    p_luatb_cbctx cbctx = (p_luatb_cbctx)cb_data->user_data;
    cbctx->cb_value = strdup(cb_data->value->value.str);
    s_cb_data cb_data_new;
    s_vpi_time cb_time_new;
    cb_time_new.high = 0;
    cb_time_new.low = 0;
    cb_time_new.type = vpiSimTime;
    cb_data_new.reason = cbReadWriteSynch;
    cb_data_new.cb_rtn = luatb_vpi_callback_readwritesync_for_cbcall;
    cb_data_new.time = &cb_time_new;
    cb_data_new.obj = NULL;
    cb_data_new.index = 0;
    cb_data_new.user_data = (PLI_BYTE8*)cbctx;
    cb_data_new.value = NULL;
    vpiHandle cb_handle_new = vpi_register_cb(&cb_data_new);
    vpi_release_handle(cb_handle_new);
    return 0;
}


PLI_INT32 luatb_vpi_callback_readwritesync_for_cbcall(p_cb_data cb_data) 
{
    p_luatb_cbctx cbctx = (p_luatb_cbctx)cb_data->user_data;
    lua_State* L = cbctx->L;
    // 压入要调用的函数
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbctx->cb_func_ref);
    // 压入函数第一个参数，传入的table
    lua_rawgeti(L, LUA_REGISTRYINDEX, cbctx->cb_data_ref);
    if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_WHEN) {
        // 压入第二个参数，回调时间
        PLI_UINT64 time_high = cb_data->time->high;
        PLI_UINT64 time_low = cb_data->time->low;
        PLI_UINT64 time = (time_high << 32)+time_low;
        lua_pushinteger(L, time);
    } else if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_INTERVAL) {
        // 压入第二个参数，回调时间
        PLI_UINT64 time_high = cb_data->time->high;
        PLI_UINT64 time_low = cb_data->time->low;
        PLI_UINT64 time = (time_high << 32)+time_low;
        lua_pushinteger(L, time);
    } else if (cbctx->cb_type == LUATB_CBTYPE_SIGNAL_CHANGE) {
        // 压入第二个参数，信号值，binstr格式
        lua_pushstring(L, cbctx->cb_value);
        free(cbctx->cb_value);
        cbctx->cb_value = NULL;
    }
    // 调用回调函数
    int ret = lua_pcall(L, 2, 1, NULL);
    if (ret != LUA_OK) {
        luatb_info_error("callback error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);     // 弹出错误信息
        if(cbctx->cb_type == LUATB_CBTYPE_SIGNAL_CHANGE) {
            // 如果调用回调出错，则需要移除signal_change回调，避免一直调用
            // simtime_when和simtime_interval回调都是单次回调，不用移除
            vpi_remove_cb(cbctx->cb_handle);
            cbctx->cb_handle = NULL;
        }
        vpi_control(vpiStop);
        return 0;
    }
    // 回调函数返回值，表示是否继续回调
    // 只有simtime_interval和signal_change两种回调需要判断
    int cont = 1;  // 默认需要继续回调
    if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0) {
        // 只有显示的返回false值时，取消回调
        cont = 0;
    }
    lua_pop(L, 1);  // 弹出返回值
    if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_INTERVAL) {
        if (cont == 1) {    // 如需继续回调则重新注册一个
            s_cb_data cb_data_new;
            s_vpi_time cb_time_new;
            cb_time_new.type = vpiSimTime;
            cb_time_new.high = cbctx->cb_time >> 32;
            cb_time_new.low = cbctx->cb_time & 0xFFFFFFFF;
            cb_data_new.reason = cbReadWriteSynch;
            cb_data_new.cb_rtn = luatb_vpi_callback_readwritesync_for_cbcall;
            cb_data_new.time = &cb_time_new;
            cb_data_new.user_data = (PLI_BYTE8 *)cbctx;
            cb_data_new.obj = NULL;
            cb_data_new.value = NULL;
            cb_data_new.index = 0;
            vpiHandle cb_handle_new = vpi_register_cb(&cb_data_new);
            vpi_release_handle(cb_handle_new);
        }
    } else if (cbctx->cb_type == LUATB_CBTYPE_SIMTIME_INTERVAL) {
        if (cont == 0) {    // 如果不需要回调则移除
            vpi_remove_cb(cbctx->cb_handle);
            cbctx->cb_handle = NULL;
        }
    }
    return 0;
}