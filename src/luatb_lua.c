#include "luatb.h"

/*
lua中的print函数，添加前缀
*/
static int luatb_lua_global_print(lua_State *L)
{
    int n = lua_gettop(L); /* number of arguments */
    int i;
    vpi_printf("[script:I:%llu] ", luatb_vpi_get_sim_time());
    for (i = 1; i <= n; i++) { /* for each argument */
        size_t l;
        const char *s = luaL_tolstring(L, i, &l); /* convert it to string */
        if (i > 1)                                /* not the first element? */
            vpi_printf("\t");                     /* add a tab before it */
        vpi_printf(s);                            /* print it */
        lua_pop(L, 1);                            /* pop result */
    }
    vpi_printf("\n");
    return 0;
}

/*
SignalHandle类型
*/
static int luatb_lua_type_signalhandle_finalizer(lua_State *L)
{
    vpiHandle *p_handle = lua_touserdata(L, 1);
    if (*p_handle != NULL)
    {
        vpi_release_handle(*p_handle);
        *p_handle = NULL;
    }
    return 0;
}

static void luatb_lua_type_signalhandle_initial(lua_State *L)
{
    luaL_newmetatable(L, LUATB_SIGNALHANDLE_TYPENAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index"); // metatable.__index = metatable
    lua_pushcfunction(L, luatb_lua_type_signalhandle_finalizer);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void luatb_lua_type_signalhandle_newobj(lua_State *L, vpiHandle signal, const char *path)
{
    vpiHandle *p_handle = lua_newuserdatauv(L, sizeof(vpiHandle), 1);
    *p_handle = signal;
    lua_pushstring(L, path);
    lua_setiuservalue(L, -2, 1);
    luaL_setmetatable(L, LUATB_SIGNALHANDLE_TYPENAME);
}

/*
VPI库函数：仿真控制
vpi.get_simtime() -> integer
vpi.get_timeunit() -> number
vpi.finish_sim() -> void
vpi.stop_sim() -> void
*/
static int luatb_lua_libvpi_func_get_simtime(lua_State *L)
{
    lua_Unsigned t = luatb_vpi_get_sim_time();
    lua_pushinteger(L, t);
    return 1;
}

static int luatb_lua_libvpi_func_get_timeunit(lua_State* L)
{
    lua_pushinteger(L, luatb_vpi_get_time_unit());
    return 1;
}

static int luatb_lua_libvpi_func_finish_sim(lua_State *L)
{
    vpi_control(vpiFinish, 0);
    return 0;
}

static int luatb_lua_libvpi_func_stop_sim(lua_State *L)
{
    vpi_control(vpiStop);
    return 0;
}

/*
VPI库函数：信号操作
vpi.get_signal_handle_by_path(path) -> SignalHandle|table(SignalHandle)|nil,error
vpi.get_signal_path(handle) -> string|nil,error
vpi.get_signal_value_binstr(handle) -> string|nil,error
vpi.set_signal_value_binstr(handle, binstr [,delay]) -> string|nil,error
vpi.force_signal_value_binstr(handle, binstr) -> string|nil,error
vpi.release_signal_force(handle); -> string|nil,error
*/
static int luatb_lua_libvpi_func_get_signal_handle_by_path(lua_State *L)
{
    // 获取rtenv
    p_luatb_rtenv* p_rtenv = lua_getextraspace(L);
    p_luatb_rtenv rtenv = *p_rtenv;
    if (rtenv == NULL) {
        luaL_pushfail(L);
        lua_pushstring(L, "failed to get luatb runtime environment");
        return 2;
    }
    const char *path = luaL_checkstring(L, 1);
    vpiHandle root = rtenv->UUT;
    vpiHandle signal_handle = vpi_handle_by_name(path, root);
    if (signal_handle == NULL)
    {
        s_vpi_error_info error;
        vpi_chk_error(&error);
        luaL_pushfail(L);
        lua_pushfstring(L, "failed to get handle to signal %s:\n\t%s", path, error.message);
        return 2;
    }
    // 判断handle的类型
    PLI_INT32 handle_type = vpi_get(vpiType, signal_handle);
    if (VS_TYPE_IS_VHDL(handle_type)) {
        luaL_pushfail(L);
        lua_pushstring(L, "VHDL object not supported");
        return 2;
    }
    vpiHandle array_handle, left_rng_handle, right_rng_handle;
    PLI_INT32 array_size, index_base;
    s_vpi_value left_range, right_range;
    size_t root_path_len = strlen(vpi_get_str(vpiFullName, root));
    char* object_path;
    switch(handle_type) {
        case vpiNet:
        case vpiReg:
        case vpiPort:
            luatb_lua_type_signalhandle_newobj(L, signal_handle, path);
            return 1;
        case vpiNetArray:
        case vpiRegArray:
            // 获取array的大小
            array_size = vpi_get(vpiSize, signal_handle);
            left_rng_handle = vpi_handle(vpiLeftRange, signal_handle);
            right_rng_handle = vpi_handle(vpiRightRange, signal_handle);
            left_range.format = vpiIntVal;
            right_range.format = vpiIntVal;
            vpi_get_value(left_rng_handle, &left_range);
            vpi_get_value(right_rng_handle, &right_range);
            if (left_range.value.integer <= right_range.value.integer) {
                index_base = left_range.value.integer;
            } else {
                index_base = right_range.value.integer;
            }
            lua_newtable(L);
            for (PLI_INT32 idx = 0; idx < array_size; idx++) {
                array_handle = vpi_handle_by_index(signal_handle, idx+index_base);
                object_path = vpi_get_str(vpiFullName, array_handle);
                luatb_lua_type_signalhandle_newobj(L, array_handle, object_path+root_path_len+1);
                lua_seti(L, -2, luaL_len(L, -2)+1); 
            }
            return 1;
        default:
            luaL_pushfail(L);
            lua_pushfstring(L, "object type %d not supported", handle_type);
            return 2;
    }
}

static int luatb_lua_libvpi_func_get_signal_path(lua_State *L)
{
    luaL_checkudata(L, 1, LUATB_SIGNALHANDLE_TYPENAME);
    lua_getiuservalue(L, 1, 1);
    return 1;
}

static int luatb_lua_libvpi_func_get_signal_value_binstr(lua_State *L)
{
    vpiHandle *p_signal = luaL_checkudata(L, 1, LUATB_SIGNALHANDLE_TYPENAME);
    vpiHandle signal = *p_signal;
    lua_getiuservalue(L, 1, 1);
    char* signal_path = strdup(lua_tostring(L, -1));
    lua_pop(L, 1);
    if (signal == NULL)
    {
        luaL_pushfail(L);
        lua_pushfstring("failed to get handle to signal %s", signal_path);
        free(signal_path);
        return 2;
    }
    s_vpi_value value;
    s_vpi_error_info error;
    PLI_INT32 error_code;
    value.format = vpiBinStrVal;
    vpi_get_value(signal, &value);
    error_code = vpi_chk_error(&error);
    if (error_code != 0)
    {
        luaL_pushfail(L);
        lua_pushfstring("failed to get value of signal %s:\n\t%s", signal_path, error.message);
        free(signal_path);
        return 2;
    }
    lua_pushstring(L, value.value.str);
    free(signal_path);
    return 1;
}

static int luatb_lua_libvpi_func_set_signal_value_binstr(lua_State *L)
{
    vpiHandle *p_signal = luaL_checkudata(L, 1, LUATB_SIGNALHANDLE_TYPENAME);
    const char *binstr = luaL_checkstring(L, 2);
    lua_Integer delay = luaL_optinteger(L, 3, 0);
    vpiHandle signal = *p_signal;
    if (signal == NULL)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to get handle to signal %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        luaL_pushfail(L);
        return 1;
    }
    s_vpi_value value;
    value.format = vpiBinStrVal;
    value.value.str = binstr;
    s_vpi_time time;
    time.type = vpiSimTime;
    time.high = delay >> 32;
    time.low = delay & 0xFFFFFFFF;
    s_vpi_error_info error;
    PLI_INT32 error_code;
    vpiHandle ev_handle = vpi_put_value(signal, &value, &time, vpiTransportDelay);
    error_code = vpi_chk_error(&error);
    if (error_code != 0)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to set value %s to signal %s:\n\t%s", binstr, lua_tostring(L, -1), error.message);
        lua_pop(L, 1);
        luaL_pushfail(L);
        vpi_release_handle(ev_handle);
        return 1;
    }
    // 返回设置的字符串
    lua_pushstring(L, binstr);
    vpi_release_handle(ev_handle);
    return 1;
}

static int luatb_lua_libvpi_func_force_signal_value_binstr(lua_State *L)
{
    vpiHandle *p_signal = luaL_checkudata(L, 1, LUATB_SIGNALHANDLE_TYPENAME);
    const char *binstr = luaL_checkstring(L, 2);
    vpiHandle signal = *p_signal;
    if (signal == NULL)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to get handle to signal %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        luaL_pushfail(L);
        return 1;
    }
    s_vpi_value value;
    value.format = vpiBinStrVal;
    value.value.str = binstr;
    s_vpi_error_info error;
    PLI_INT32 error_code;
    vpiHandle ev_handle = vpi_put_value(signal, &value, NULL, vpiForceFlag);
    error_code = vpi_chk_error(&error);
    if (error_code != 0)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to force signal %s to value %s:\n\t%s", lua_tostring(L, -1), binstr, error.message);
        lua_pop(L, 1);
        luaL_pushfail(L);
        vpi_release_handle(ev_handle);
        return 1;
    }
    // 返回设置的字符串
    lua_pushstring(L, binstr);
    vpi_release_handle(ev_handle);
    return 1;
}

static int luatb_lua_libvpi_func_release_signal_force(lua_State *L)
{
    vpiHandle *p_signal = luaL_checkudata(L, 1, LUATB_SIGNALHANDLE_TYPENAME);
    vpiHandle signal = *p_signal;
    if (signal == NULL)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to get handle to signal %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        luaL_pushfail(L);
        return 1;
    }
    s_vpi_value value;
    value.format = vpiBinStrVal;
    s_vpi_error_info error;
    PLI_INT32 error_code;
    vpiHandle ev_handle = vpi_put_value(signal, &value, NULL, vpiReleaseFlag);
    error_code = vpi_chk_error(&error);
    if (error_code != 0)
    {
        lua_getiuservalue(L, 1, 1);
        luatb_info_error("failed to release force for signal %s:\n\t%s", lua_tostring(L, -1), error.message);
        lua_pop(L, 1);
        luaL_pushfail(L);
        vpi_release_handle(ev_handle);
        return 1;
    }
    // 返回release之后信号的值
    lua_pushstring(L, value.value.str);
    vpi_release_handle(ev_handle);
    return 1;
}
/*
VPI库函数：回调
vpi.register_callback_on_simtime_when(func, time, data) -> status
vpi.register_callback_on_simtime_interval(func, intv, data) -> status
vpi.register_callback_on_signal_change(func, handle, data) -> status
*/
static int luatb_lua_libvpi_func_register_callback_on_simtime_when(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_Integer time = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    // 检查时间参数，只能大于0并且大于当前仿真时间
    lua_Unsigned now_time = luatb_vpi_get_sim_time();
    lua_Unsigned set_time = time;
    if (time <= 0 || set_time <= now_time) {
        luatb_info_error("failed to register callback: time<=0 or time<=current simtime");
        luaL_pushfail(L);
        return 1;
    }

    p_luatb_cbctx cbctx = (p_luatb_cbctx)malloc(sizeof(s_luatb_cbctx));
    memset(cbctx, NULL, sizeof(s_luatb_cbctx));

    cbctx->L = L;
    /*
    移动前      移动后
    栈顶        栈顶
    3   data    func
    2   time    data
    1   func    time
    */
    lua_rotate(L, 1, 2);
    cbctx->cb_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_data_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_time = time;
    cbctx->cb_type = LUATB_CBTYPE_SIMTIME_WHEN;

    s_vpi_time cb_time;
    cb_time.type = vpiSimTime;
    cb_time.high = 0;
    cb_time.low = 0;
    s_cb_data cb_data;
    cb_data.reason = cbReadOnlySynch;
    cb_data.cb_rtn = luatb_vpi_callback_readonlysync_for_register;
    cb_data.user_data = (PLI_BYTE8*)cbctx;
    cb_data.time = &cb_time;
    cb_data.value = NULL;
    cb_data.obj = NULL;
    cb_data.index = 0;
    vpiHandle cb_handle = vpi_register_cb(&cb_data);
    s_vpi_error_info error;
    if (vpi_chk_error(&error) != 0) {
        luatb_info_error("failed to register callback: \n\t%s", error.message);
        luaL_pushfail(L);
        free(cbctx);
        return 1;
    }
    vpi_release_handle(cb_handle);
    // 将cbctx值保存起来
    p_luatb_rtenv* p_rtenv = lua_getextraspace(L);
    p_luatb_rtenv rtenv = *p_rtenv;
    lua_rawgeti(L, LUA_REGISTRYINDEX, rtenv->cb_table_ref);
    int n = luaL_len(L, -1);
    lua_pushlightuserdata(L, cbctx);
    lua_seti(L, -2, n+1);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int luatb_lua_libvpi_func_register_callback_on_simtime_interval(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_Integer intv = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    if (intv <= 0) {
        luatb_info_error("failed to register callback: intv<=0");
        luaL_pushfail(L);
        return 1;
    }

    p_luatb_cbctx cbctx = (p_luatb_cbctx)malloc(sizeof(s_luatb_cbctx));
    memset(cbctx, NULL, sizeof(s_luatb_cbctx));

    cbctx->L = L;
    /*
    移动前      移动后
    栈顶        栈顶
    3   data    func
    2   intv    data
    1   func    intv
    */
    lua_rotate(L, 1, 2);
    cbctx->cb_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_data_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_time = intv;
    cbctx->cb_type = LUATB_CBTYPE_SIMTIME_INTERVAL;

    s_vpi_time cb_time;
    cb_time.type = vpiSimTime;
    cb_time.high = 0;
    cb_time.low = 0;
    s_cb_data cb_data;
    cb_data.reason = cbReadOnlySynch;
    cb_data.cb_rtn = luatb_vpi_callback_readonlysync_for_register;
    cb_data.user_data = (PLI_BYTE8*)cbctx;
    cb_data.time = &cb_time;
    cb_data.value = NULL;
    cb_data.obj = NULL;
    cb_data.index = 0;
    vpiHandle cb_handle = vpi_register_cb(&cb_data);
    s_vpi_error_info error;
    if (vpi_chk_error(&error) != 0) {
        luatb_info_error("failed to register callback: \n\t%s", error.message);
        luaL_pushfail(L);
        free(cbctx);
        return 1;
    }
    vpi_release_handle(cb_handle);
    // 将cbctx值保存起来
    p_luatb_rtenv* p_rtenv = lua_getextraspace(L);
    p_luatb_rtenv rtenv = *p_rtenv;
    lua_rawgeti(L, LUA_REGISTRYINDEX, rtenv->cb_table_ref);
    int n = luaL_len(L, -1);
    lua_pushlightuserdata(L, cbctx);
    lua_seti(L, -2, n+1);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int luatb_lua_libvpi_func_register_callback_on_signal_change(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    vpiHandle* p_signal = luaL_checkudata(L, 2, LUATB_SIGNALHANDLE_TYPENAME);
    luaL_checktype(L, 3, LUA_TTABLE);

    if (*p_signal == NULL) {
        luatb_info_error("failed to register callback: invalid signal handle");
        luaL_pushfail(L);
        return 1;
    }

    p_luatb_cbctx cbctx = (p_luatb_cbctx)malloc(sizeof(s_luatb_cbctx));
    memset(cbctx, NULL, sizeof(s_luatb_cbctx));

    cbctx->L = L;
    /*
    移动前      移动后
    栈顶        栈顶
    3   data    func
    2   handle  data
    1   func    handle
    */
    lua_rotate(L, 1, 2);
    cbctx->cb_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_data_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    cbctx->cb_object = *p_signal;
    cbctx->cb_type = LUATB_CBTYPE_SIGNAL_CHANGE;

    s_vpi_time cb_time;
    cb_time.type = vpiSimTime;
    cb_time.high = 0;
    cb_time.low = 0;
    s_cb_data cb_data;
    cb_data.reason = cbReadOnlySynch;
    cb_data.cb_rtn = luatb_vpi_callback_readonlysync_for_register;
    cb_data.user_data = (PLI_BYTE8*)cbctx;
    cb_data.time = &cb_time;
    cb_data.value = NULL;
    cb_data.obj = NULL;
    cb_data.index = 0;
    vpiHandle cb_handle = vpi_register_cb(&cb_data);
    s_vpi_error_info error;
    if (vpi_chk_error(&error) != 0) {
        luatb_info_error("failed to register callback: \n\t%s", error.message);
        luaL_pushfail(L);
        free(cbctx);
        return 1;
    }
    vpi_release_handle(cb_handle);
    // 将cbctx值保存起来
    p_luatb_rtenv* p_rtenv = lua_getextraspace(L);
    p_luatb_rtenv rtenv = *p_rtenv;
    lua_rawgeti(L, LUA_REGISTRYINDEX, rtenv->cb_table_ref);
    int n = luaL_len(L, -1);
    lua_pushlightuserdata(L, cbctx);
    lua_seti(L, -2, n+1);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

/*
VPI库函数注册
*/
const luaL_Reg luatb_lua_libvpi_funcs[] = {
    // 仿真控制函数
    {"get_simtime", luatb_lua_libvpi_func_get_simtime},
    {"get_timeunit", luatb_lua_libvpi_func_get_timeunit},
    {"finish_sim", luatb_lua_libvpi_func_finish_sim},
    {"stop_sim", luatb_lua_libvpi_func_stop_sim},
    // 信号操作函数
    {"get_signal_handle_by_path", luatb_lua_libvpi_func_get_signal_handle_by_path},
    {"get_signal_path", luatb_lua_libvpi_func_get_signal_path},
    {"get_signal_value_binstr", luatb_lua_libvpi_func_get_signal_value_binstr},
    {"set_signal_value_binstr", luatb_lua_libvpi_func_set_signal_value_binstr},
    {"force_signal_value_binstr", luatb_lua_libvpi_func_force_signal_value_binstr},
    {"release_signal_force", luatb_lua_libvpi_func_release_signal_force},
    // 注册回调函数
    {"register_callback_on_simtime_when", luatb_lua_libvpi_func_register_callback_on_simtime_when},
    {"register_callback_on_simtime_interval", luatb_lua_libvpi_func_register_callback_on_simtime_interval},
    {"register_callback_on_signal_change", luatb_lua_libvpi_func_register_callback_on_signal_change},
    {NULL, NULL}};

void luatb_lua_initial_luatb_libs(p_luatb_rtenv rtenv)
{
    lua_State *L = rtenv->L;
    vpiHandle UUT = rtenv->UUT;
    // 创建SignalHandle对象的metatable
    luatb_lua_type_signalhandle_initial(L);
    // 创建VPI库函数
    luaL_newlib(L, luatb_lua_libvpi_funcs);
    // 设置库名
    lua_setglobal(L, "vpi");
    // 替换全局print函数
    lua_pushcfunction(L, luatb_lua_global_print);
    lua_setglobal(L, "print");
}