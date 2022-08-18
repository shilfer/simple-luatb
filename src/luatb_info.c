#include "luatb.h"

void luatb_info_error(const char* fmt, ...)
{
    va_list ap = NULL;
    va_start(ap, fmt);
    vpi_printf("[luatb:E:%llu] ", luatb_vpi_get_sim_time());
    vpi_vprintf(fmt, ap);
    va_end(ap);
    vpi_printf("\n");
}

void luatb_info_info(const char* fmt, ...)
{
    va_list ap = NULL;
    va_start(ap, fmt);
    vpi_printf("[luatb:I:%llu] ", luatb_vpi_get_sim_time());
    vpi_vprintf(fmt, ap);
    va_end(ap);
    vpi_printf("\n");
}

void luatb_info_warning(const char* fmt, ...)
{
    va_list ap = NULL;
    va_start(ap, fmt);
    vpi_printf("[luatb:W:%llu] ", luatb_vpi_get_sim_time());
    vpi_vprintf(fmt, ap);
    va_end(ap);
    vpi_printf("\n");
}

void luatb_info_debug(const char* fmt, ...)
{
#ifdef DEBUG
    va_list ap = NULL;
    va_start(ap, fmt);
    vpi_printf("[luatb:D:%llu] ", luatb_vpi_get_sim_time());
    vpi_vprintf(fmt, ap);
    va_end(ap);
    vpi_printf("\n");
#endif
}